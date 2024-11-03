#include <xen/midi_engine.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <sequence/measure.hpp>

#include <xen/midi.hpp>
#include <xen/scale.hpp>
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
 * @param key The key to transpose to, simple addition.
 * @param scale_translate_direction The direction to move pitches for a Scale.
 * @return juce::MidiBuffer
 */
[[nodiscard]] auto render_measure(sequence::Measure const &measure,
                                  sequence::Tuning const &tuning, float base_frequency,
                                  xen::DAWState const &daw,
                                  std::optional<xen::Scale> const &scale, int key,
                                  xen::TranslateDirection scale_translate_direction)
    -> juce::MidiBuffer
{
    return xen::render_to_midi(xen::state_to_timeline(
        measure, tuning, base_frequency, daw, scale, key, scale_translate_direction));
}

/**
 * Modifies the MIDI channel of all channel-based messages in a MidiBuffer.
 *
 * @param midi_buffer The MidiBuffer to modify.
 * @param new_channel The new channel to set for each event. (Must be between 1 and 16)
 */
void change_midi_channel(juce::MidiBuffer &midi_buffer, int new_channel)
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
 *
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
                      std::uint64_t current_sample_count, int buffer_length)
    -> juce::MidiBuffer
{
    slices_.clear();

    // Create a Slice for each of the existing trigger notes.
    for (auto i = std::size_t{0}; i < live_notes_.size(); ++i)
    {
        auto &note = live_notes_[i];
        if (note.channel != -1)
        {
            slices_.push_back(Slice{
                .begin = 0,
                .end = -1, // placeholder value
                .note = note,
                .trigger_note = first_midi_trigger_note + (int)i,
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
                    auto &live_note = live_notes_[(
                        std::size_t)(midi_number - first_midi_trigger_note)];
                    live_note = {
                        .channel = channel,
                        .start_time = current_sample_count + (std::uint64_t)sample,
                        .last_seq_midi_number = -1,
                    };
                    slices_.push_back({
                        .begin = sample,
                        .end = -1,
                        .note = live_note,
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

                    auto &live_note = live_notes_[(
                        std::size_t)(midi_number - first_midi_trigger_note)];
                    live_note = {
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
        auto const index = (std::size_t)(slice.trigger_note - first_midi_trigger_note);
        auto const samples_in_seq = rendered_midi_[index].sample_count;
        auto const sample_offset = current_sample_count - slice.note.start_time;
        auto const begin = (sample_offset + (std::size_t)slice.begin) % samples_in_seq;
        auto const end = (sample_offset + (std::size_t)slice.end) % samples_in_seq;

        auto midi_slice = find_subrange(rendered_midi_[index].midi, (int)begin,
                                        (int)end, (int)samples_in_seq);

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

        // TODO if slice.note.last_seq_midi_number != -1 and sequence has changed

        // TODO if rendered midi does not match last seq midi number then send note off
        // and note on. this only works if last seq midi number is saved from previous
        // step() run
        // Maybe this last note is kept in a separate array so that it isn't cleared
        // each time, and also you can probably get rid of find dangling note on fn.

        out_buffer.addEvents(midi_slice, 0, -1, slice.begin);
    }

    return out_buffer;
}

void MidiEngine::update(SequencerState const &sequencer, DAWState const &daw)
{
    for (auto i = std::size_t{0}; i < sequencer.sequence_bank.size(); ++i)
    {
        auto const &measure = sequencer.sequence_bank[i];
        rendered_midi_[i] = {
            .midi = render_measure(measure, sequencer.tuning, sequencer.base_frequency,
                                   daw, sequencer.scale, sequencer.key,
                                   sequencer.scale_translate_direction),
            .sample_count = sequence::samples_count(measure, daw.sample_rate, daw.bpm),
        };
    }
}

auto MidiEngine::get_note_start_samples() const -> std::array<std::uint64_t, 16>
{
    auto result = std::array<std::uint64_t, 16>{};
    std::transform(std::cbegin(live_notes_), std::cend(live_notes_), std::begin(result),
                   [](auto const &ln) {
                       return ln.channel == -1 ? (std::uint64_t)-1 : ln.start_time;
                   });
    return result;
}

auto MidiEngine::allocate_channel(std::vector<Slice> const &slices, int midi_number)
    -> int
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
            used[(std::size_t)(channel - 2)] = true;
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