#include <xen/midi.hpp>

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <optional>
#include <queue>
#include <tuple>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/midi.hpp>
#include <sequence/sequence.hpp>
#include <sequence/timing.hpp>
#include <sequence/tuning.hpp>
#include <sequence/utility.hpp>

#include <xen/midi_internal.hpp>
#include <xen/scale.hpp>
#include <xen/state.hpp>

namespace
{

struct MidiEvent
{
    int sample_position;
    int priority;
    int channel;
    int note_number;
    juce::MidiMessage message;
};

[[nodiscard]] auto scale_translate_element(sequence::MusicElement const &element,
                                           std::vector<int> const &valid_pitches,
                                           std::size_t tuning_length,
                                           xen::TranslateDirection direction)
    -> sequence::MusicElement;

[[nodiscard]] auto scale_translate_cell(sequence::Cell const &cell,
                                        std::vector<int> const &valid_pitches,
                                        std::size_t tuning_length,
                                        xen::TranslateDirection direction)
    -> sequence::Cell
{
    auto out = sequence::Cell{.elements = {}, .weight = cell.weight};
    out.elements.reserve(cell.elements.size());
    for (auto const &element : cell.elements)
    {
        out.elements.push_back(scale_translate_element(
            element, valid_pitches, tuning_length, direction));
    }
    return out;
}

[[nodiscard]] auto scale_translate_element(sequence::MusicElement const &element,
                                           std::vector<int> const &valid_pitches,
                                           std::size_t tuning_length,
                                           xen::TranslateDirection direction)
    -> sequence::MusicElement
{
    return std::visit(
        sequence::utility::overload{
            [&](sequence::Note note) -> sequence::MusicElement {
                note.pitch = xen::map_pitch_to_scale(
                    note.pitch, valid_pitches, tuning_length, direction);
                return note;
            },
            [&](sequence::Sequence sequence) -> sequence::MusicElement {
                for (auto &cell : sequence.cells)
                {
                    cell = scale_translate_cell(cell, valid_pitches, tuning_length,
                                                direction);
                }
                return sequence;
            },
        },
        element);
}

[[nodiscard]] auto key_transpose_element(sequence::MusicElement const &element, int key)
    -> sequence::MusicElement;

[[nodiscard]] auto key_transpose_cell(sequence::Cell const &cell, int key)
    -> sequence::Cell
{
    auto out = sequence::Cell{.elements = {}, .weight = cell.weight};
    out.elements.reserve(cell.elements.size());
    for (auto const &element : cell.elements)
    {
        out.elements.push_back(key_transpose_element(element, key));
    }
    return out;
}

[[nodiscard]] auto key_transpose_element(sequence::MusicElement const &element, int key)
    -> sequence::MusicElement
{
    return std::visit(
        sequence::utility::overload{
            [&](sequence::Note note) -> sequence::MusicElement {
                note.pitch += key;
                return note;
            },
            [&](sequence::Sequence sequence) -> sequence::MusicElement {
                for (auto &cell : sequence.cells)
                {
                    cell = key_transpose_cell(cell, key);
                }
                return sequence;
            },
        },
        element);
}

} // namespace

namespace xen::midi_internal
{

namespace
{

struct ActiveChannel
{
    std::uint32_t end;
    int channel;
};

} // namespace

auto assign_mpe_channels(std::vector<sequence::midi::TimedMidiNote> const &timeline)
    -> std::vector<AssignedMidiNote>
{
    auto sorted = timeline;
    std::sort(std::begin(sorted), std::end(sorted), [](auto const &lhs, auto const &rhs) {
        return std::tie(lhs.begin, lhs.end, lhs.note, lhs.pitch_bend, lhs.velocity) <
               std::tie(rhs.begin, rhs.end, rhs.note, rhs.pitch_bend, rhs.velocity);
    });

    auto free_channels =
        std::priority_queue<int, std::vector<int>, std::greater<>>{};
    for (auto channel = mpe_first_member_channel; channel <= mpe_last_member_channel;
         ++channel)
    {
        free_channels.push(channel);
    }

    auto active = std::priority_queue<
        ActiveChannel,
        std::vector<ActiveChannel>,
        std::function<bool(ActiveChannel const &, ActiveChannel const &)>>{
        [](ActiveChannel const &lhs, ActiveChannel const &rhs) {
            return std::tie(lhs.end, lhs.channel) >
                   std::tie(rhs.end, rhs.channel);
        }};

    auto assigned = std::vector<AssignedMidiNote>{};
    assigned.reserve(sorted.size());

    for (auto const &note : sorted)
    {
        if (note.end <= note.begin)
        {
            continue;
        }

        while (!active.empty() && active.top().end <= note.begin)
        {
            free_channels.push(active.top().channel);
            active.pop();
        }

        if (free_channels.empty())
        {
            continue;
        }

        auto const channel = free_channels.top();
        free_channels.pop();

        assigned.push_back({.note = note, .channel = channel});
        active.push({.end = note.end, .channel = channel});
    }

    return assigned;
}

auto render_assigned_notes(std::vector<AssignedMidiNote> const &assigned_notes)
    -> juce::MidiBuffer
{
    auto events = std::vector<MidiEvent>{};
    events.reserve(assigned_notes.size() * 3);

    for (auto const &assigned : assigned_notes)
    {
        auto const &note = assigned.note;
        auto const channel = assigned.channel;

        events.push_back({
            .sample_position = (int)note.begin,
            .priority = 1,
            .channel = channel,
            .note_number = note.note,
            .message = juce::MidiMessage::pitchWheel(channel, note.pitch_bend),
        });
        events.push_back({
            .sample_position = (int)note.begin,
            .priority = 2,
            .channel = channel,
            .note_number = note.note,
            .message = juce::MidiMessage::noteOn(channel, note.note,
                                                 (juce::uint8)note.velocity),
        });
        events.push_back({
            .sample_position = (int)note.end,
            .priority = 0,
            .channel = channel,
            .note_number = note.note,
            .message = juce::MidiMessage::noteOff(channel, note.note),
        });
    }

    std::sort(std::begin(events), std::end(events), [](auto const &lhs, auto const &rhs) {
        return std::tie(lhs.sample_position, lhs.priority, lhs.channel,
                        lhs.note_number) <
               std::tie(rhs.sample_position, rhs.priority, rhs.channel,
                        rhs.note_number);
    });

    auto buffer = juce::MidiBuffer{};
    buffer.ensureSize((int)events.size());
    for (auto const &event : events)
    {
        buffer.addEvent(event.message, event.sample_position);
    }
    return buffer;
}

auto live_voice_from(AssignedMidiNote const &assigned) -> LiveVoice
{
    return {
        .note = assigned.note,
        .channel = assigned.channel,
    };
}

} // namespace xen::midi_internal

namespace xen
{

auto state_to_timeline(Measure measure, sequence::Tuning const &tuning,
                       float base_frequency, DAWState const &daw_state,
                       std::optional<Scale> const &scale, int key,
                       TranslateDirection scale_translate_direction)
    -> std::vector<sequence::midi::TimedMidiNote>
{
    if (scale)
    {
        measure.cell = scale_translate_cell(measure.cell, generate_valid_pitches(*scale),
                                            tuning.intervals.size(),
                                            scale_translate_direction);
    }

    measure.cell = key_transpose_cell(measure.cell, key);

    auto const sample_count = sequence::samples_count(measure.time_signature,
                                                      daw_state.sample_rate,
                                                      daw_state.bpm);

    return sequence::midi::flatten_to_midi(measure.cell.elements, 0, sample_count,
                                           tuning, base_frequency, 48.f);
}

auto render_to_midi(std::vector<sequence::midi::TimedMidiNote> const &timeline)
    -> juce::MidiBuffer
{
    return midi_internal::render_assigned_notes(
        midi_internal::assign_mpe_channels(timeline));
}

auto extract_window(juce::MidiBuffer const &buffer, SampleCount buffer_length,
                    SampleIndex begin, SampleIndex end) -> juce::MidiBuffer
{
    auto out_buffer = juce::MidiBuffer{};
    auto current_sample = begin;

    while (current_sample < end)
    {
        auto const wrapped_position = current_sample % buffer_length;

        for (auto at = buffer.findNextSamplePosition((int)wrapped_position);
             at != buffer.cend(); ++at)
        {
            auto const &event = *at;
            auto const absolute_position =
                current_sample + (SampleIndex)event.samplePosition - wrapped_position;

            if (absolute_position >= end)
            {
                break;
            }

            auto const relative_position = (int)(absolute_position - begin);
            out_buffer.addEvent(event.data, event.numBytes, relative_position);
        }

        current_sample += buffer_length - wrapped_position;
    }

    return out_buffer;
}

} // namespace xen
