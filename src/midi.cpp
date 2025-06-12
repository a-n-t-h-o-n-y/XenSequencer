#include <xen/midi.hpp>

#include <cstddef>
#include <optional>
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
[[nodiscard]] auto scale_translate_cell(sequence::Cell const &cell,
                                        std::vector<int> const &valid_pitches,
                                        std::size_t tuning_length,
                                        xen::TranslateDirection direction)
    -> sequence::Cell
{
    return {
        .element =
            std::visit(sequence::utility::overload{
                           [&](sequence::Note note) -> sequence::MusicElement {
                               note.pitch = xen::map_pitch_to_scale(
                                   note.pitch, valid_pitches, tuning_length, direction);
                               return note;
                           },
                           [](sequence::Rest const &rest) -> sequence::MusicElement {
                               return rest;
                           },
                           [&](sequence::Sequence seq) -> sequence::MusicElement {
                               for (auto &c : seq.cells)
                               {
                                   c = scale_translate_cell(c, valid_pitches,
                                                            tuning_length, direction);
                               }
                               return seq;
                           },
                       },
                       cell.element),
        .weight = cell.weight,
    };
}

/**
 * Transposes notes based on a key value.
 */
[[nodiscard]] auto key_transpose_cell(sequence::Cell const &cell, int key)
    -> sequence::Cell
{
    return {
        .element =
            std::visit(sequence::utility::overload{
                           [&](sequence::Note note) -> sequence::MusicElement {
                               note.pitch += key;
                               return note;
                           },
                           [](sequence::Rest const &rest) -> sequence::MusicElement {
                               return rest;
                           },
                           [&](sequence::Sequence seq) -> sequence::MusicElement {
                               for (auto &c : seq.cells)
                               {
                                   c = key_transpose_cell(c, key);
                               }
                               return seq;
                           },
                       },
                       cell.element),
        .weight = cell.weight,
    };
}

} // namespace

namespace xen
{

auto state_to_timeline(sequence::Measure measure, sequence::Tuning const &tuning,
                       float base_frequency, DAWState const &daw_state,
                       std::optional<Scale> const &scale, int key,
                       TranslateDirection scale_translate_direction)
    -> sequence::midi::EventTimeline
{
    if (scale)
    {
        measure.cell =
            scale_translate_cell(measure.cell, generate_valid_pitches(*scale),
                                 tuning.intervals.size(), scale_translate_direction);
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
    buffer.ensureSize(timeline.size());
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

        // Move current_sample forward by the remaining length in this buffer segment
        current_sample += buffer_length - wrapped_position;
    }

    return out_buffer;
}

} // namespace xen