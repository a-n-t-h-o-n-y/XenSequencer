#pragma once

#include <optional>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/midi.hpp>

#include <xen/state.hpp>

namespace xen
{

/**
 * Converts the state of the plugin to a MIDI Event timeline.
 *
 * @param measure The measure to convert.
 * @param tuning The tuning to use.
 * @param base_frequency The base frequency of the tuning.
 * @param daw_state The state of the DAW.
 * @param key The key to transpose to, simple addition.
 * @param scale_translate_direction The direction to move a pitch when applying a scale.
 * @return sequence::midi::EventTimeline
 */
[[nodiscard]] auto state_to_timeline(sequence::Measure measure,
                                     sequence::Tuning const &tuning,
                                     float base_frequency, DAWState const &daw_state,
                                     std::optional<Scale> const &scale, int key,
                                     TranslateDirection scale_translate_direction)
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
 * Extract a range of MIDI values, over a buffer treated as an 'infinite' loop.
 *
 * @param buffer The buffer to extract MIDI events from.
 * @param buffer_length The intended length of the MIDI buffer.
 * @param begin The first sample to start extracting MIDI from the buffer.
 * @param end The last sample to extract MIDI from. This may be beyond buffer_length.
 * @return juce::MidiBuffer
 */
[[nodiscard]] auto extract_window(juce::MidiBuffer const &buffer,
                                  SampleCount buffer_length, SampleIndex begin,
                                  SampleIndex end) -> juce::MidiBuffer;

} // namespace xen