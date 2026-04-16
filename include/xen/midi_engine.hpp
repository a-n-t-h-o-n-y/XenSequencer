#pragma once

#include <cstddef>

#include <juce_audio_basics/juce_audio_basics.h>

#include <xen/clock.hpp>
#include <xen/state.hpp>

namespace xen
{

class MidiEngine
{
  public:
    /**
     * Translates a slice of transport time to a slice of sequence notes.
     *
     * @details This is intended to be used in the processBlock function to translate
     * the current transport window to the corresponding output sequence notes.
     * Incoming MIDI is passed through unchanged.
     * @param midi_input The incoming MIDI buffer to preserve.
     * @param offset The sample index offset to begin processing from.
     * @param length The number of samples to process.
     * @param daw The state of the DAW.
     * @return The midi buffer to be sent to the DAW.
     */
    [[nodiscard]] auto step(juce::MidiBuffer const &midi_input, SampleIndex offset,
                            SampleCount length, DAWState const &daw)
        -> juce::MidiBuffer;

    /**
     * Render the current EngineState to MIDI and save in rendered_midi_.
     *
     * @details This only renders Measures where there has been a change since the
     * previous render, and stores updates in rendered_.
     * @param sequencer The current state of the sequencer.
     * @param daw The current state of the DAW.
     */
    void update(EngineState const &sequencer, DAWState const &daw);

    [[nodiscard]] auto get_loop_phase(SampleIndex offset, DAWState const &daw) const
        -> double;

  private:
    struct MidiSequence
    {
        juce::MidiBuffer midi;
        SampleCount sample_count;
    };
    MidiSequence rendered_midi_{};
};

} // namespace xen
