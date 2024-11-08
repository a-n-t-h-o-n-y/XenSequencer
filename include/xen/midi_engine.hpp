#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <xen/state.hpp>

namespace xen
{

class MidiEngine
{
  public:
    /**
     * A potentially unterminated sample range representing a pressed trigger note.
     */
    struct ActiveSequence
    {
        SampleIndex begin;
        SampleIndex end; // -1 if currently unterminated (no note off read).
        int midi_channel;
        int last_note_on; // -1 if no sequence note currently 'on'.
        int last_pitch_wheel;
        std::size_t rendered_midi_index;
    };

  public:
    /**
     * Translates a slice of trigger notes to a slice of sequence notes.
     *
     * @details This is intended to be used in the processBlock function to translate
     * incoming midi triggers to the corresponding output sequence notes. This will
     * update the live_sequences_ member.
     * @param midi_input The incoming midi triggers.
     * @param offset The sample index offset to begin processing from.
     * @param length The number of samples to process.
     * @return The midi buffer to be sent to the DAW.
     */
    [[nodiscard]] auto step(juce::MidiBuffer const &midi_input, SampleIndex offset,
                            SampleCount length) -> juce::MidiBuffer;

    /**
     * Render the current SequencerState to MIDI and save in rendered_midi_.
     *
     * @details This only renders Measures where there has been a change since the
     * previous render, and stores updates in rendered_.
     * @param sequencer The current state of the sequencer.
     * @param daw The current state of the DAW.
     */
    void update(SequencerState const &sequencer, DAWState const &daw);

    /**
     * For use by GUI thread, stored in processor by processBlock
     *
     * @details These are the accumulated sample start time offsets for each user input
     * note. A value of (SampleIndex)-1 means the note is off.
     */
    [[nodiscard]] auto get_note_start_samples() const -> std::array<SampleIndex, 16>;

  private:
    // Only contains unterminated sequences between steps.
    std::vector<ActiveSequence> active_sequences_;

    struct MidiSequence
    {
        juce::MidiBuffer midi;
        SampleCount sample_count;
    };
    std::array<MidiSequence, 16> rendered_midi_;
};

} // namespace xen