#include <xen/midi_engine.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include <sequence/measure.hpp>

#include <xen/clock.hpp>
#include <xen/midi.hpp>
#include <xen/scale.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>

namespace
{

auto const first_midi_trigger_note = 36;

[[nodiscard]] auto note_to_index(int note) -> std::size_t
{
    return (std::size_t)(note - first_midi_trigger_note);
}

[[nodiscard]] auto is_valid_trigger(int note) -> bool
{
    return note >= first_midi_trigger_note && note < first_midi_trigger_note + 16;
}

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
 * Return the last note on or note off MIDI event, or std::nullopt if none in \p buffer.
 */
[[nodiscard]] auto find_last_note_event(juce::MidiBuffer const &buffer)
    -> std::optional<juce::MidiMessage>
{
    auto last_note_event = std::optional<juce::MidiMessage>{std::nullopt};
    for (auto const &meta : buffer)
    {
        if (auto const message = meta.getMessage(); message.isNoteOnOrOff())
        {
            last_note_event = message;
        }
    }
    return last_note_event;
}

/**
 * Return the last pitch wheel MIDI event value, or -1 if none in \p buffer.
 */
[[nodiscard]] auto find_last_pitch_event(juce::MidiBuffer const &buffer) -> int
{
    auto last_pitch = -1;
    for (auto const &meta : buffer)
    {
        if (auto const message = meta.getMessage(); message.isPitchWheel())
        {
            last_pitch = message.getPitchWheelValue();
        }
    }
    return last_pitch;
}

/**
 * Create a new juce::MidiBuffer that has the half open range [begin, end) from \p midi.
 */
[[nodiscard]] auto slice(juce::MidiBuffer const &midi, xen::SampleIndex begin,
                         xen::SampleIndex end) -> juce::MidiBuffer
{
    auto result = juce::MidiBuffer{};

    for (auto it = midi.findNextSamplePosition((int)begin);
         it != midi.cend() && (xen::SampleIndex)(*it).samplePosition < end; ++it)
    {
        result.addEvent((*it).getMessage(), (*it).samplePosition - (int)begin);
    }

    return result;
}

/**
 * Finds an unused midi channel and returns it.
 * @details A channel is considered unavailable if an active sequence is using it and
 * does not have an `end` before `from`. Available channels are [2-16]. Returns -1 if
 * there are no channels left to allocate.
 */
[[nodiscard]] auto allocate_channel(
    std::vector<xen::MidiEngine::ActiveSequence> active_sequences,
    xen::SampleIndex from) -> int
{
    // channel := index + 2;
    auto used = std::array<bool, 15>{}; // Initialize all elements to false

    // Mark used channels
    for (auto const &seq : active_sequences)
    {
        auto const channel = seq.midi_channel;
        assert(channel >= 2 && channel <= 16);

        if (seq.end == (xen::SampleIndex)-1 || seq.end >= from)
        {
            used[(std::size_t)(channel - 2)] = true;
        }
    }

    // Find the first unused number
    for (auto i = std::size_t{0}; i < used.size(); ++i)
    {
        if (used[i] == false)
        {
            return (int)i + 2;
        }
    }

    return -1; // If all numbers are used
}

} // namespace

namespace xen
{

auto MidiEngine::step(juce::MidiBuffer const &midi_input, SampleIndex offset,
                      SampleCount length, DAWState const &daw) -> juce::MidiBuffer
{
    // Not perfect, but it is for UI display so its fine.
    auto const buffer_start_time = Clock::now();

    // Make corrections for modified rendered_midi_ entries.
    auto out_buffer = juce::MidiBuffer{};
    for (auto &as : active_sequences_)
    {
        assert(as.rendered_midi_index < rendered_midi_.size());
        auto const &rendered = rendered_midi_[as.rendered_midi_index];
        auto const previous_slice =
            slice(rendered.midi, 0, (offset - as.begin) % rendered.sample_count);
        auto const note_event = find_last_note_event(previous_slice);
        // Correct Note Value
        if (!note_event.has_value() || note_event->isNoteOff()) // No Note
        {
            // Note A -> Note Off
            if (as.last_note_on != -1)
            {
                out_buffer.addEvent(
                    juce::MidiMessage::noteOff(as.midi_channel, as.last_note_on), 0);
                as.last_note_on = -1;
            }
        }
        else if (note_event.has_value() && note_event->isNoteOn()) // Note
        {
            // Note Off -> Note A
            if (as.last_note_on == -1)
            {
                out_buffer.addEvent(juce::MidiMessage::noteOn(
                                        as.midi_channel, note_event->getNoteNumber(),
                                        note_event->getVelocity()),
                                    0);
                as.last_note_on = note_event->getNoteNumber();
            }
            // Note A -> Note B
            else if (as.last_note_on != note_event->getNoteNumber())
            {
                out_buffer.addEvent(
                    juce::MidiMessage::noteOff(as.midi_channel, as.last_note_on), 0);
                out_buffer.addEvent(juce::MidiMessage::noteOn(
                                        as.midi_channel, note_event->getNoteNumber(),
                                        note_event->getVelocity()),
                                    0);
                as.last_note_on = note_event->getNoteNumber();
            }
        }

        // Correct Pitch Wheel
        auto const pitch_event = find_last_pitch_event(previous_slice);
        if (pitch_event != -1 && as.last_pitch_wheel != pitch_event)
        {
            out_buffer.addEvent(
                juce::MidiMessage::pitchWheel(as.midi_channel, pitch_event), 0);
            as.last_pitch_wheel = pitch_event;
        }
    }

    // Update active_sequences_ from midi_input.
    for (auto const &metadata : midi_input)
    {
        auto const message = metadata.getMessage();
        auto const sample = offset + (SampleIndex)metadata.samplePosition;
        auto const pressed_time = [&] {
            auto const duration = std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>((double)metadata.samplePosition /
                                              (double)daw.sample_rate));
            return buffer_start_time + duration;
        }();

        if (message.isNoteOn())
        {

            if (auto const note = message.getNoteNumber(); is_valid_trigger(note))
            {
                active_sequences_.push_back({
                    .begin = sample,
                    .begin_at = pressed_time,
                    .end = (SampleIndex)-1,
                    .midi_channel = allocate_channel(active_sequences_, sample),
                    .last_note_on = -1,
                    .last_pitch_wheel = 8'192,
                    .rendered_midi_index = note_to_index(note),
                });
            }
        }
        else if (message.isNoteOff())
        {
            auto const at = std::ranges::find(
                active_sequences_, note_to_index(message.getNoteNumber()),
                [](auto const &x) { return x.rendered_midi_index; });
            if (at != std::end(active_sequences_))
            {
                at->end = sample;
            }
        }
        else if (message.isAllNotesOff())
        {
            std::ranges::for_each(active_sequences_, [&](auto &x) { x.end = sample; });
        }
        else
        {
            out_buffer.addEvent(message, metadata.samplePosition);
        }
    }

    // Grab MIDI from rendered_midi_ array.
    for (auto &as : active_sequences_)
    {
        assert(as.rendered_midi_index < rendered_midi_.size());
        auto const &rendered = rendered_midi_[as.rendered_midi_index];
        auto const wbegin =
            (SampleIndex)std::max((std::int64_t)offset - (std::int64_t)as.begin,
                                  (std::int64_t)0) %
            rendered.sample_count;
        auto const wend =
            wbegin + std::min(offset + length, as.end) - std::max(offset, as.begin);

        auto midi = extract_window(rendered.midi, rendered.sample_count, wbegin, wend);

        if (auto const last_note = find_last_note_event(midi); last_note.has_value())
        {
            if (last_note->isNoteOn())
            {
                as.last_note_on = last_note->getNoteNumber();
            }
            else
            {
                assert(last_note->isNoteOff());
                as.last_note_on = -1;
            }
        }

        if (auto const last_pitch = find_last_pitch_event(midi); last_pitch != -1)
        {
            as.last_pitch_wheel = last_pitch;
        }

        change_midi_channel(midi, as.midi_channel);

        // Turn note off if a sequence is terminated (NoteOff from input buffer).
        if (as.end != (SampleIndex)-1 && as.last_note_on != -1)
        {
            midi.addEvent(juce::MidiMessage::noteOff(as.midi_channel, as.last_note_on),
                          (int)(as.end - offset));
        }

        out_buffer.addEvents(midi, 0, -1, std::max((int)as.begin - (int)offset, 0));
    }

    // Remove sequences as they become inactive.
    std::erase_if(active_sequences_,
                  [](auto const &x) { return x.end != (SampleIndex)-1; });

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

auto MidiEngine::get_trigger_note_start_times() const
    -> std::array<Clock::time_point, 16>
{
    auto result = std::array<Clock::time_point, 16>{};
    result.fill(Clock::time_point{});
    for (auto const &as : active_sequences_)
    {
        result[as.rendered_midi_index] = as.begin_at;
    }
    return result;
}

} // namespace xen