#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <xen/clock.hpp>
#include <xen/midi_internal.hpp>
#include <xen/state.hpp>

namespace xen
{

class MidiEngine
{
  public:
    static constexpr auto max_live_voices =
        static_cast<std::size_t>(midi_internal::mpe_last_member_channel -
                                 midi_internal::mpe_first_member_channel + 1);

    struct LiveVoiceSet
    {
        std::array<midi_internal::LiveVoice, max_live_voices> voices{};
        std::size_t size{};
    };

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
     * Render the current ProjectState to MIDI and save in rendered_midi_.
     *
     * @details This only renders Measures where there has been a change since the
     * previous render, and stores updates in rendered_.
     * @param sequencer The current state of the sequencer.
     * @param daw The current state of the DAW.
     */
    void update(ProjectState const &project, DAWState const &daw);
    void update(ProjectState const &project, DAWState const &daw,
                OutputId const &output_id);

    [[nodiscard]] auto get_loop_phase(SampleIndex offset, DAWState const &daw) const
        -> double;

  private:
    struct MidiSequence
    {
        juce::MidiBuffer midi;
        std::vector<midi_internal::AssignedMidiNote> assigned_notes{};
        SampleCount sample_count{};
        SampleCount phase_origin{};
    };

    [[nodiscard]] static auto render(ProjectState const &project, DAWState const &daw,
                                     OutputId const &output_id)
        -> std::optional<MidiSequence>;

    MidiSequence rendered_midi_{};
    LiveVoiceSet active_live_voices_{};
};

} // namespace xen
