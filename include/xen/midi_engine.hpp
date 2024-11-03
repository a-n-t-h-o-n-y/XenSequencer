#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <xen/state.hpp>

namespace xen
{

class MidiEngine
{
  private:
    /**
     * The first midi note that triggers a sequence.
     */
    static inline auto const first_midi_trigger_note = 36;

    /**
     * A currently playing sequence, controlled by an input trigger midi note.
     */
    struct LiveNote
    {
        // The channel on which the sequence is playing, if none: -1.
        int channel = -1;

        std::uint64_t start_time = 0; // accumulated sample count

        // The note in the sequence that is currently playing, if none: -1.
        int last_seq_midi_number = -1;
    };

    /**
     * A slice of a sequence that is currently playing, with begin and end with respect
     * to a single processBlock input midi buffer.
     */
    struct Slice
    {
        int begin;
        int end;
        LiveNote note;
        int trigger_note;
        bool is_end; // Whether the slice is the last for the sequence.
    };

  public:
    /**
     * Translates a slice of trigger notes to a slice of sequence notes.
     *
     * @details This is intended to be used in the processBlock function to translate
     * incoming midi triggers to the corresponding output sequence notes. This will
     * update the live_notes_ member.
     * @param triggers The incoming midi triggers.
     * @param current_sample_count The current sample count since the start of the
     * plugin.
     * @param buffer_length
     * @return The midi buffer to be sent to the DAW.
     */
    [[nodiscard]] auto step(juce::MidiBuffer const &triggers,
                            std::uint64_t current_sample_count, int buffer_length)
        -> juce::MidiBuffer;

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
     * note. A value of (std::uint64_t)-1 means the note is off.
     */
    [[nodiscard]] auto get_note_start_samples() const -> std::array<std::uint64_t, 16>;

  private:
    /**
     * Allocate an unused midi channel in the range [2, 16].
     *
     * @details If the note already has an entry in slices, that channel will be reused.
     * @param slices The array of live slices to search for an unused channel.
     * @param midi_number The midi note number to allocate a channel for.
     * @return The channel number if one is available, -1 otherwise.
     */
    [[nodiscard]] static auto allocate_channel(std::vector<Slice> const &slices,
                                               int midi_number) -> int;

  private:
    // Index is the MIDI trigger note, value contains info about the emitted sequence.
    std::array<LiveNote, 16> live_notes_;

    // Is a member only to avoid allocations, is cleared on every step(...) call.
    std::vector<Slice> slices_;

    struct MidiSequence
    {
        juce::MidiBuffer midi;
        std::uint32_t sample_count;
    };

    std::array<MidiSequence, 16> rendered_midi_;
};

} // namespace xen