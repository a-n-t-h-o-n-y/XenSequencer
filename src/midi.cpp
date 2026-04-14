#include <xen/midi.hpp>

#include <cstddef>
#include <optional>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

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

auto state_to_timeline(Measure measure, sequence::Tuning const &tuning,
                       float base_frequency, DAWState const &daw_state,
                       std::optional<Scale> const &scale, int key,
                       TranslateDirection scale_translate_direction)
    -> std::vector<sequence::midi::TimedMidiNote>
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
        measure.cell, measure.time_signature, daw_state.sample_rate, daw_state.bpm,
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

        // Move current_sample forward by the remaining length in this buffer segment
        current_sample += buffer_length - wrapped_position;
    }

    return out_buffer;
}

} // namespace xen
