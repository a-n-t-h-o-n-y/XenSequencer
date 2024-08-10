#include <xen/midi_engine.hpp>

#include <algorithm>
#include <cassert>
#include <optional>
#include <utility>

#include <sequence/measure.hpp>

#include <xen/midi.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>

namespace
{

/**
 * Renders a sequence::Measure as a MIDI buffer.
 *
 * @param measure The measure to render.
 * @param tuning The tuning to use.
 * @param base_frequency The base frequency of the tuning.
 * @param daw The state of the DAW.
 * @return juce::MidiBuffer
 */
[[nodiscard]] auto render_measure(sequence::Measure const &measure,
                                  sequence::Tuning const &tuning, float base_frequency,
                                  xen::DAWState const &daw) -> juce::MidiBuffer
{
    return xen::render_to_midi(
        xen::state_to_timeline(measure, tuning, base_frequency, daw));
}

/**
 * Modifies the MIDI channel of all channel-based messages in a MidiBuffer.
 * @param midi_buffer The MidiBuffer to modify.
 * @param new_channel The new channel to set for each event. (Must be between 1 and 16)
 */
auto change_midi_channel(juce::MidiBuffer &midi_buffer, int new_channel) -> void
{
    assert(new_channel >= 1 && new_channel <= 16);

    auto temp_buffer = juce::MidiBuffer{};
    for (auto const metadata : midi_buffer)
    {
        auto msg = metadata.getMessage();

        if (msg.isNoteOnOrOff() || msg.isPitchWheel() || msg.isAftertouch() ||
            msg.isController() || msg.isProgramChange() || msg.isChannelPressure())
        {
            msg.setChannel(new_channel);
        }

        temp_buffer.addEvent(msg, metadata.samplePosition);
    }

    midi_buffer.swapWith(temp_buffer);
}

/**
 * Finds the last dangling MIDI note on event in a MidiBuffer.
 * @param buffer The MidiBuffer to search.
 * @return The last dangling MIDI note on event if found, otherwise std::nullopt.
 */
[[nodiscard]] auto find_dangling_note_on(juce::MidiBuffer const &buffer)
    -> std::optional<juce::MidiMessage>
{
    auto last_note_on = std::optional<juce::MidiMessage>{};

    for (auto const &meta : buffer)
    {
        auto const message = meta.getMessage();
        if (message.isNoteOn())
        {
            last_note_on = message;
        }
        else if (message.isNoteOff() && last_note_on)
        {
            last_note_on.reset();
        }
    }

    return last_note_on;
}

} // namespace

namespace xen
{

auto MidiEngine::step(juce::MidiBuffer const &triggers,
                      std::uint64_t current_sample_count,
                      int buffer_length) -> juce::MidiBuffer
{
    slices_.clear();

    // Add live notes to slices
    for (auto i = std::size_t{0}; i < live_notes_.size(); ++i)
    {
        auto &note = live_notes_[i];
        if (note.channel != -1)
        {
            slices_.push_back(Slice{
                .begin = 0,
                .end = -1,
                .note = note,
                .trigger_note = (int)i + first_midi_trigger_note,
                .is_end = false,
            });
        }
    }

    // Handle trigger notes on/off
    for (auto const &metadata : triggers)
    {
        auto const message = metadata.getMessage();
        auto const sample = metadata.samplePosition;

        if (message.isNoteOn())
        {
            auto const midi_number = message.getNoteNumber();
            if (midi_number >= first_midi_trigger_note &&
                midi_number < first_midi_trigger_note + 16)
            {
                auto const channel = allocate_channel(slices_, midi_number);
                if (channel != -1)
                {
                    live_notes_[midi_number - first_midi_trigger_note] = {
                        .channel = channel,
                        .start_time = current_sample_count + sample,
                        .last_seq_midi_number = -1,
                    };
                    slices_.push_back({
                        .begin = sample,
                        .end = -1,
                        .note = live_notes_[midi_number - first_midi_trigger_note],
                        .trigger_note = midi_number,
                        .is_end = false,
                    });
                }
            }
        }
        else if (message.isNoteOff())
        {
            auto const midi_number = message.getNoteNumber();
            if (midi_number >= first_midi_trigger_note &&
                midi_number < first_midi_trigger_note + 16)
            {
                auto const slice_it =
                    std::ranges::find_if(slices_, [midi_number](Slice const &slice) {
                        return slice.trigger_note == midi_number && slice.end == -1;
                    });
                // It should always be found if DAW is consistent, but some are not.
                if (slice_it != std::end(slices_))
                {
                    slice_it->end = sample;
                    slice_it->is_end = true;

                    live_notes_[midi_number - first_midi_trigger_note] = {
                        .channel = -1,
                        .start_time = 0,
                        .last_seq_midi_number = -1,
                    };
                }
            }
        }
    }

    for (auto &slice : slices_)
    {
        if (slice.end == -1)
        {
            slice.end = buffer_length;
        }
    }

    // Grab MIDI output for each sequence slice
    auto out_buffer = juce::MidiBuffer{};

    for (auto &slice : slices_)
    {
        auto const index = slice.trigger_note - first_midi_trigger_note;
        auto const samples_in_seq =
            sequence::samples_count({sequencer_copy_.sequence_bank[index]},
                                    daw_copy_.sample_rate, daw_copy_.bpm);

        auto const sample_offset = current_sample_count - slice.note.start_time;

        auto const begin = (sample_offset + slice.begin) % samples_in_seq;
        auto const end = (sample_offset + slice.end) % samples_in_seq;

        auto midi_slice =
            find_subrange(rendered_midi_[index], begin, end, samples_in_seq);

        change_midi_channel(midi_slice, slice.note.channel);

        auto const last_note = find_dangling_note_on(midi_slice);
        if (last_note.has_value())
        {
            // Update current sequence note if exists
            live_notes_[index].last_seq_midi_number = last_note->getNoteNumber();
            slice.note.last_seq_midi_number = last_note->getNoteNumber();
        }

        // Add NoteOff Event if the slice is the last for the seq and trigger is off.
        if (slice.is_end && slice.note.last_seq_midi_number != -1)
        {
            midi_slice.addEvent(
                juce::MidiMessage::noteOff(slice.note.channel,
                                           slice.note.last_seq_midi_number),
                slice.end);
        }

        out_buffer.addEvents(midi_slice, 0, -1, slice.begin);
    }

    return out_buffer;
}

auto MidiEngine::update(DAWState daw) -> void
{
    this->update(sequencer_copy_, daw);
}

auto MidiEngine::update(SequencerState sequencer, DAWState daw) -> void
{
    assert(sequencer_copy_.sequence_bank.size() == sequencer.sequence_bank.size());

    if (!compare_within_tolerance(daw_copy_.bpm, daw.bpm, 0.0001f) ||
        sequencer_copy_.tuning != sequencer.tuning ||
        sequencer_copy_.base_frequency != sequencer.base_frequency ||
        daw_copy_.sample_rate != daw.sample_rate)
    {
        // Render Everything
        for (auto i = std::size_t{0}; i < sequencer.sequence_bank.size(); ++i)
        {
            rendered_midi_[i] =
                render_measure(sequencer.sequence_bank[i], sequencer.tuning,
                               sequencer.base_frequency, daw);
        }
    }
    else
    {
        // For each measure in the sequencer, if it has changed, render it.
        for (auto i = std::size_t{0}; i < sequencer.sequence_bank.size(); ++i)
        {
            if (sequencer_copy_.sequence_bank[i] != sequencer.sequence_bank[i])
            {
                rendered_midi_[i] =
                    render_measure(sequencer.sequence_bank[i], sequencer.tuning,
                                   sequencer.base_frequency, daw);
            }
        }
    }

    sequencer_copy_ = std::move(sequencer);
    daw_copy_ = std::move(daw);
}

auto MidiEngine::allocate_channel(std::vector<Slice> const &slices,
                                  int midi_number) -> int
{
    assert(midi_number >= first_midi_trigger_note &&
           midi_number < first_midi_trigger_note + 16);

    auto used = std::array<bool, 15>{}; // Initialize all elements to false

    // Mark used numbers
    for (auto const &[a_, b_, live_note, trigger_note, c_] : slices)
    {
        // Midi note can't be 'on' more than once at any given time, so reuse channel.
        // TODO does the above mean you should only read ch 1 input triggers?
        if (trigger_note == midi_number)
        {
            return live_note.channel;
        }

        auto const channel = live_note.channel;
        if (channel >= 2 && channel <= 16)
        {
            used[channel - 2] = true;
        }
    }

    // Find the first unused number
    for (auto i = std::size_t{0}; i < 15; ++i)
    {
        if (!used[i])
        {
            return (int)i + 2;
        }
    }

    return -1; // Return -1 if all numbers are used
}

} // namespace xen