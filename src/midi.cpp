#include <xen/midi.hpp>

#include <stdexcept>
#include <variant>

#include <juce_audio_processors/juce_audio_processors.h>

#include <sequence/midi.hpp>
#include <sequence/utility.hpp>

#include <xen/state.hpp>

namespace xen
{

auto state_to_timeline(DAWState const &daw_state, State const &state)
    -> sequence::midi::EventTimeline
{
    return sequence::midi::translate_to_midi_timeline(
        state.phrase, daw_state.sample_rate, daw_state.bpm, state.tuning,
        state.base_frequency);
}

auto render_to_midi(sequence::midi::EventTimeline const &timeline) -> juce::MidiBuffer
{
    namespace seq = sequence;

    auto buffer = juce::MidiBuffer{};
    for (auto const &[event, sample] : timeline)
    {
        juce::MidiMessage midi = std::visit(
            seq::utility::overload{
                [](seq::midi::NoteOn const &note) -> juce::MidiMessage {
                    return juce::MidiMessage::noteOn(1, note.note, note.velocity);
                },
                [](seq::midi::NoteOff const &note) -> juce::MidiMessage {
                    return juce::MidiMessage::noteOff(1, note.note);
                },
                [](seq::midi::PitchBend const &pitch) -> juce::MidiMessage {
                    return juce::MidiMessage::pitchWheel(1, pitch.value);
                },
            },
            event);
        buffer.addEvent(midi, (int)sample);
    }
    return buffer;
}

auto find_subrange(juce::MidiBuffer const &buffer, int begin, int end,
                   int loop_boundary) -> juce::MidiBuffer
{
    if (begin < 0 || end < 0)
    {
        throw std::out_of_range{"begin and end must be greater than or equal to zero."};
    }

    auto sub_buffer = juce::MidiBuffer{};

    if (begin <= end)
    {
        for (auto at = buffer.findNextSamplePosition(begin); at != buffer.cend(); ++at)
        {
            auto const &event = *at;
            if (event.samplePosition < end)
            {
                sub_buffer.addEvent(event.data, event.numBytes,
                                    event.samplePosition - begin);
            }
            else
            {
                break;
            }
        }
    }
    // Loop from 'begin' to the loop boundary, then from the start to 'end'
    else
    {
        for (auto at = buffer.findNextSamplePosition(begin); at != buffer.cend(); ++at)
        {
            auto const &event = *at;
            sub_buffer.addEvent(event.data, event.numBytes,
                                event.samplePosition - begin);
        }

        for (auto at = buffer.findNextSamplePosition(0); at != buffer.cend(); ++at)
        {
            auto const &event = *at;
            if (event.samplePosition < end)
            {
                // Adjust the sample position so it appears contiguous with the first
                sub_buffer.addEvent(event.data, event.numBytes,
                                    event.samplePosition + loop_boundary - begin);
            }
            else
            {
                break;
            }
        }
    }

    return sub_buffer;
}

} // namespace xen