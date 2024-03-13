#pragma once

#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>

#include <sequence/midi.hpp>

#include <xen/state.hpp>

namespace xen
{

/**
 * Converts the state of the plugin to a MIDI Event timeline.
 *
 * @param state The state of the plugin.
 * @return sequence::midi::EventTimeline
 */
[[nodiscard]] auto state_to_timeline(DAWState const &daw_state, State const &state)
    -> sequence::midi::EventTimeline;

/**
 * Renders a sequence library midi::EventTimeline as a MIDI buffer.
 *
 * @param timeline The MIDI Event timeline.
 * @return juce::MidiBuffer
 */
[[nodiscard]] auto render_to_midi(sequence::midi::EventTimeline const &timeline)
    -> juce::MidiBuffer;

/**
 * Finds the subset of MIDI messages in a buffer between two samples, half-open
 * [begin, end).
 *
 * @param buffer The MIDI buffer.
 * @param begin The beginning sample, inclusive.
 * @param end The ending sample, not inclusive.
 * @param loop_boundary The length of the Phrase in samples, for loop boundary
 * calculations.
 * @return juce::MidiBuffer
 *
 * @throws std::out_of_range if begin or end are less than zero.
 */
[[nodiscard]] auto find_subrange(juce::MidiBuffer const &buffer, int begin, int end,
                                 int loop_boundary) -> juce::MidiBuffer;

/**
 * Finds the most recent pitch bend event in a MIDI buffer.
 *
 * @param buffer The MIDI buffer to search.
 * @param sample_begin The sample position to start searching from, inclusive.
 * @return An optional containing the most recent pitch bend event if found;
 * std::nullopt otherwise.
 */
[[nodiscard]] auto find_most_recent_pitch_bend_event(juce::MidiBuffer const &buffer,
                                                     long sample_begin)
    -> std::optional<juce::MidiMessage>;

/**
 * Finds the most recent note on or off event in a MIDI buffer.
 *
 * @param buffer The MIDI buffer to search.
 * @param sample_begin The sample position to start searching from, inclusive.
 * @return An optional containing the most recent note event if found;
 * std::nullopt otherwise.
 */
[[nodiscard]] auto find_most_recent_note_event(juce::MidiBuffer const &buffer,
                                               long sample_begin)

    -> std::optional<juce::MidiMessage>;

/**
 * Get the last MIDI pitch bend message from a JUCE MidiBuffer.
 *
 * @param midi_buffer The MidiBuffer to read from.
 * @return std::optional<juce::MidiMessage> The last MidiMessage pitch bend if
 * found, otherwise std::nullopt.
 */
[[nodiscard]] auto find_last_pitch_bend_event(juce::MidiBuffer const &midi_buffer)
    -> std::optional<juce::MidiMessage>;

/**
 * Get the last MIDI Note on or off message from a JUCE MidiBuffer.
 *
 * @param midi_buffer The MidiBuffer to read from.
 * @return std::optional<juce::MidiMessage> The last MidiMessage Note on or off if
 * found, otherwise std::nullopt.
 */
[[nodiscard]] auto find_last_note_event(juce::MidiBuffer const &midi_buffer)
    -> std::optional<juce::MidiMessage>;

} // namespace xen