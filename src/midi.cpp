#include <xen/midi.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/measure.hpp>
#include <sequence/midi.hpp>
#include <sequence/sequence.hpp>
#include <sequence/tuning.hpp>
#include <sequence/utility.hpp>

#include <xen/scale.hpp>
#include <xen/state.hpp>

namespace
{

/**
 * Maps any Notes to the list of valid pitches
 */
[[nodiscard]] auto scale_translate_cell(
    sequence::Cell const &cell, std::vector<int> const &valid_pitches,
    std::size_t tuning_length, xen::TranslateDirection direction) -> sequence::Cell
{
    return std::visit(
        sequence::utility::overload{
            [&](sequence::Note note) -> sequence::Cell {
                note.pitch = xen::map_pitch_to_scale(note.pitch, valid_pitches,
                                                     tuning_length, direction);
                return note;
            },
            [](sequence::Rest const &rest) -> sequence::Cell { return rest; },
            [&](sequence::Sequence seq) -> sequence::Cell {
                for (auto &c : seq.cells)
                {
                    c = scale_translate_cell(c, valid_pitches, tuning_length,
                                             direction);
                }
                return seq;
            },
        },
        cell);
}

/**
 * Transposes notes based on a key value.
 */
[[nodiscard]] auto key_transpose_cell(sequence::Cell const &cell,
                                      int key) -> sequence::Cell
{
    return std::visit(
        sequence::utility::overload{
            [&](sequence::Note note) -> sequence::Cell {
                note.pitch += key;
                return note;
            },
            [](sequence::Rest const &rest) -> sequence::Cell { return rest; },
            [&](sequence::Sequence seq) -> sequence::Cell {
                for (auto &c : seq.cells)
                {
                    c = key_transpose_cell(c, key);
                }
                return seq;
            },
        },
        cell);
}

} // namespace

namespace xen
{

auto state_to_timeline(sequence::Measure measure, sequence::Tuning const &tuning,
                       float base_frequency, DAWState const &daw_state,
                       std::optional<Scale> const &scale,
                       int key) -> sequence::midi::EventTimeline
{
    if (scale)
    {
        measure.cell =
            scale_translate_cell(measure.cell, generate_valid_pitches(*scale),
                                 tuning.intervals.size(), TranslateDirection::Up);
    }

    measure.cell = key_transpose_cell(measure.cell, key);

    // TODO add pitch bend range parameter to state and commands to alter it.
    return sequence::midi::translate_to_midi_timeline(
        measure, daw_state.sample_rate, daw_state.bpm, tuning, base_frequency, 48.f);
}

auto render_to_midi(sequence::midi::EventTimeline const &timeline) -> juce::MidiBuffer
{
    namespace seq = sequence;

    auto buffer = juce::MidiBuffer{};
    for (auto const &[event, sample] : timeline)
    {
        juce::MidiMessage midi = std::visit(
            seq::utility::overload{
                [](seq::midi::NoteOn const &note) -> juce::MidiMessage {
                    return juce::MidiMessage::noteOn(1, note.note, note.velocity);
                },
                [](seq::midi::NoteOff const &note) -> juce::MidiMessage {
                    return juce::MidiMessage::noteOff(1, note.note);
                },
                [](seq::midi::PitchBend const &pitch) -> juce::MidiMessage {
                    return juce::MidiMessage::pitchWheel(1, pitch.value);
                },
            },
            event);
        buffer.addEvent(midi, (int)sample);
    }
    return buffer;
}

auto find_subrange(juce::MidiBuffer const &buffer, int begin, int end,
                   int loop_boundary) -> juce::MidiBuffer
{
    if (begin < 0 || end < 0)
    {
        throw std::out_of_range{"begin and end must be greater than or equal to zero."};
    }

    auto sub_buffer = juce::MidiBuffer{};

    if (begin <= end)
    {
        for (auto at = buffer.findNextSamplePosition(begin); at != buffer.cend(); ++at)
        {
            auto const &event = *at;
            if (event.samplePosition < end)
            {
                sub_buffer.addEvent(event.data, event.numBytes,
                                    event.samplePosition - begin);
            }
            else
            {
                break;
            }
        }
    }
    // Loop from 'begin' to the loop boundary, then from the start to 'end'
    else
    {
        for (auto at = buffer.findNextSamplePosition(begin); at != buffer.cend(); ++at)
        {
            auto const &event = *at;
            sub_buffer.addEvent(event.data, event.numBytes,
                                event.samplePosition - begin);
        }

        for (auto at = buffer.findNextSamplePosition(0); at != buffer.cend(); ++at)
        {
            auto const &event = *at;
            if (event.samplePosition < end)
            {
                // Adjust the sample position so it appears contiguous with the first
                sub_buffer.addEvent(event.data, event.numBytes,
                                    event.samplePosition + loop_boundary - begin);
            }
            else
            {
                break;
            }
        }
    }

    return sub_buffer;
}

auto find_most_recent_pitch_bend_event(juce::MidiBuffer const &buffer,
                                       long sample_begin)
    -> std::optional<juce::MidiMessage>
{
    auto most_recent_pitch_bend = std::optional<juce::MidiMessage>{};

    // Loop through the buffer once, starting from iter
    for (auto iter = buffer.findNextSamplePosition((int)(sample_begin + 1L));
         iter != buffer.cend(); ++iter)
    {
        auto const metadata = *iter;
        if (metadata.getMessage().isPitchWheel())
        {
            most_recent_pitch_bend = metadata.getMessage();
        }
    }

    // Loop from the beginning of the buffer to `sample_begin`
    for (auto iter = buffer.cbegin(); iter != buffer.cend(); ++iter)
    {
        auto const metadata = *iter;

        if (metadata.samplePosition > sample_begin)
        {
            break;
        }

        if (metadata.getMessage().isPitchWheel())
        {
            most_recent_pitch_bend = metadata.getMessage();
        }
    }

    return most_recent_pitch_bend;
}

auto find_most_recent_note_event(juce::MidiBuffer const &buffer,
                                 long sample_begin) -> std::optional<juce::MidiMessage>
{
    auto most_recent_event = std::optional<juce::MidiMessage>{};

    // Loop through the buffer once, starting from iter
    for (auto iter = buffer.findNextSamplePosition((int)(sample_begin + 1L));
         iter != buffer.cend(); ++iter)
    {
        auto const metadata = *iter;
        if (metadata.getMessage().isNoteOnOrOff())
        {
            most_recent_event = metadata.getMessage();
        }
    }

    // Loop from the beginning of the buffer to `sample_begin`
    for (auto iter = buffer.cbegin(); iter != buffer.cend(); ++iter)
    {
        auto const metadata = *iter;

        if (metadata.samplePosition > sample_begin)
        {
            break;
        }

        if (metadata.getMessage().isNoteOnOrOff())
        {
            most_recent_event = metadata.getMessage();
        }
    }

    return most_recent_event;
}

auto find_last_pitch_bend_event(juce::MidiBuffer const &midi_buffer)
    -> std::optional<juce::MidiMessage>
{
    auto last_message = std::optional<juce::MidiMessage>{};

    for (auto const &metadata : midi_buffer)
    {
        if (metadata.getMessage().isPitchWheel())
        {
            last_message = metadata.getMessage();
        }
    }

    return last_message;
}

auto find_last_note_event(juce::MidiBuffer const &midi_buffer)
    -> std::optional<juce::MidiMessage>
{
    auto last_message = std::optional<juce::MidiMessage>{};

    for (auto const &metadata : midi_buffer)
    {
        if (metadata.getMessage().isNoteOnOrOff())
        {
            last_message = metadata.getMessage();
        }
    }

    return last_message;
}

} // namespace xen