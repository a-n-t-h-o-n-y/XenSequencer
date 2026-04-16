#include <xen/midi.hpp>

#include <cstddef>
#include <optional>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/midi.hpp>
#include <sequence/sequence.hpp>
#include <sequence/timing.hpp>
#include <sequence/tuning.hpp>
#include <sequence/utility.hpp>

#include <xen/scale.hpp>
#include <xen/state.hpp>

namespace
{

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
    auto buffer = juce::MidiBuffer{};
    buffer.ensureSize((int)timeline.size() * 3);
    for (auto const &note : timeline)
    {
        buffer.addEvent(juce::MidiMessage::pitchWheel(1, note.pitch_bend),
                        (int)note.begin);
        buffer.addEvent(
            juce::MidiMessage::noteOn(1, note.note, (juce::uint8)note.velocity),
            (int)note.begin);
        buffer.addEvent(juce::MidiMessage::noteOff(1, note.note), (int)note.end);
    }
    return buffer;
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
