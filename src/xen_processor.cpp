#include <xen/xen_processor.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>
#include <sequence/measure.hpp>

#include <xen/midi.hpp>
#include <xen/serialize_state.hpp>
#include <xen/utility.hpp>
#include <xen/xen_editor.hpp>

namespace
{

/**
 * @brief Compares two juce::MidiMessage objects for equality.
 *
 * @param a First MidiMessage to compare.
 * @param b Second MidiMessage to compare.
 * @return bool True if the MidiMessages are equal, false otherwise.
 */
[[nodiscard]] auto are_midi_messages_equal(juce::MidiMessage const &a,
                                           juce::MidiMessage const &b) -> bool
{
    if (a.getRawDataSize() != b.getRawDataSize())
    {
        return false;
    }

    if (std::memcmp(a.getRawData(), b.getRawData(), (std::size_t)a.getRawDataSize()) !=
        0)
    {
        return false;
    }

    return true;
}

} // namespace

namespace xen
{

XenProcessor::XenProcessor()
    : timeline{init_state(), {}}, plugin_state_{init_state()}, last_rendered_time_{}
{
}

auto XenProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                juce::MidiBuffer &midi_messages) -> void
{
    buffer.clear();
    midi_messages.clear();

    // Only generate MIDI if PlayHead is playing
    auto *playhead = this->getPlayHead();
    auto const position = playhead->getPosition();
    if (!position.hasValue())
    {
        throw std::runtime_error{"PlayHead position is not valid"};
    }

    if (!position->getIsPlaying())
    {
        // Comment this out for JUCE plugin host testing
        return;
    }

    // Check if MIDI needs to be rendered because of DAW or GUI changes.
    auto const bpm_daw =
        position->getBpm() ? static_cast<float>(*(position->getBpm())) : 120.f;

    bool rendered = false;

    // Separate if statements prevent State copies on BPM changes.
    if (timeline.get_last_update_time() > last_rendered_time_)
    {
        plugin_state_ = timeline.get_state().first;
        this->render();
        rendered = true;
    }
    if (!compare_within_tolerance(daw_state.bpm, bpm_daw, 0.00001f) ||
        !compare_within_tolerance((double)daw_state.sample_rate, this->getSampleRate(),
                                  0.1))
    {
        daw_state.sample_rate = static_cast<std::uint32_t>(this->getSampleRate());
        daw_state.bpm = bpm_daw;
        this->render();
        rendered = true;
    }

    // Find current MIDI events to send according to PlayHead position
    auto const samples_in_phrase = sequence::samples_count(
        plugin_state_.phrase, daw_state.sample_rate, daw_state.bpm);

    // Empty Phrase - No MIDI
    if (samples_in_phrase == 0)
    {
        return;
    }

    auto const [begin, end] = [&] {
        auto const current_sample =
            position->getTimeInSamples()
                ? *(position->getTimeInSamples())
                : throw std::runtime_error{"Sample position is not valid"};
        auto const beg = current_sample % samples_in_phrase;
        return std::pair{
            static_cast<int>(beg),
            static_cast<int>((beg + buffer.getNumSamples()) % samples_in_phrase),
        };
    }();

    auto new_midi_buffer = find_subrange(rendered_, begin, end, (int)samples_in_phrase);

    if (rendered)
    {
        this->add_midi_corrections(new_midi_buffer, *position);
    }

    if (auto const x = find_last_note_event(new_midi_buffer); x.has_value())
    {
        last_note_event_ = *x;
    }
    if (auto const x = find_last_pitch_bend_event(new_midi_buffer); x.has_value())
    {
        last_pitch_bend_event_ = *x;
    }

    midi_messages.swapWith(new_midi_buffer);
}

auto XenProcessor::createEditor() -> juce::AudioProcessorEditor *
{
    return new XenEditor{*this};
}

auto XenProcessor::getStateInformation(juce::MemoryBlock &dest_data) -> void
{
    auto const json_str = serialize(timeline.get_state().first);
    dest_data.setSize(json_str.size());
    std::memcpy(dest_data.getData(), json_str.data(), json_str.size());
}

auto XenProcessor::setStateInformation(void const *data, int sizeInBytes) -> void
{
    auto const json_str =
        std::string(static_cast<char const *>(data), (std::size_t)sizeInBytes);
    auto const state = deserialize(json_str);
    timeline.add_state(state);
}

auto XenProcessor::render() -> void
{
    rendered_ = render_to_midi(state_to_timeline(daw_state, plugin_state_));
    last_rendered_time_ = std::chrono::high_resolution_clock::now();
}

auto XenProcessor::add_midi_corrections(
    juce::MidiBuffer &buffer, juce::AudioPlayHead::PositionInfo const &position) -> void
{
    auto const at = position.getTimeInSamples()
                        ? *(position.getTimeInSamples())
                        : throw std::runtime_error{"Sample position is not valid"};
    auto const recent_note_event = find_most_recent_note_event(rendered_, at);
    auto const recent_pitch_bend_event =
        find_most_recent_pitch_bend_event(rendered_, at);

    if (last_note_event_.isNoteOn() && recent_note_event.has_value() &&
        !are_midi_messages_equal(last_note_event_, *recent_note_event))
    {
        buffer.addEvent(juce::MidiMessage::noteOff(1, last_note_event_.getNoteNumber()),
                        0);
        buffer.addEvent(juce::MidiMessage::noteOn(1, recent_note_event->getNoteNumber(),
                                                  recent_note_event->getVelocity()),
                        0);
    }
    else if (last_note_event_.isNoteOff() && recent_note_event.has_value() &&
             !are_midi_messages_equal(last_note_event_, *recent_note_event))
    {
        buffer.addEvent(juce::MidiMessage::noteOn(1, recent_note_event->getNoteNumber(),
                                                  recent_note_event->getVelocity()),
                        0);
    }

    if (recent_pitch_bend_event.has_value() &&
        !are_midi_messages_equal(last_pitch_bend_event_, *recent_pitch_bend_event))
    {
        buffer.addEvent(*recent_pitch_bend_event, 0);
    }
}

} // namespace xen

// This creates new instances of the plugin. Must be in the global namespace.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new xen::XenProcessor{};
}
