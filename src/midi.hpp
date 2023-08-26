#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <sequence/midi.hpp>

#include "state.hpp"

namespace xen
{

/**
 * @brief Converts the state of the plugin to a MIDI Event timeline.
 *
 * @param state The state of the plugin.
 * @return sequence::midi::EventTimeline
 */
[[nodiscard]] auto state_to_timeline(DAWState const &daw_state, State const &state)
    -> sequence::midi::EventTimeline;

/**
 * @brief Renders a sequence library midi::EventTimeline as a MIDI buffer.
 *
 * @param timeline The MIDI Event timeline.
 * @return juce::MidiBuffer
 */
[[nodiscard]] auto render_to_midi(sequence::midi::EventTimeline const &timeline)
    -> juce::MidiBuffer;

/**
 * @brief Finds the subset of MIDI messages in a buffer between two samples, half-open
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

} // namespace xen