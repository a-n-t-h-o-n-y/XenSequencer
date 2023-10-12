#include <xen/xen_processor.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>
#include <sequence/measure.hpp>

#include <xen/midi.hpp>
#include <xen/xen_editor.hpp>

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

    // Separate if statements prevent State copies on BPM changes.
    if (timeline.get_last_update_time() > last_rendered_time_)
    {
        plugin_state_ = timeline.get_state().first;
        this->render();
    }
    if (daw_state.bpm != bpm_daw || daw_state.sample_rate != this->getSampleRate())
    {
        daw_state.sample_rate = static_cast<std::uint32_t>(this->getSampleRate());
        daw_state.bpm = bpm_daw;
        this->render();
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
    midi_messages.swapWith(new_midi_buffer);
}

auto XenProcessor::createEditor() -> juce::AudioProcessorEditor *
{
    return new XenEditor{*this};
}

auto XenProcessor::render() -> void
{
    rendered_ = render_to_midi(state_to_timeline(daw_state, plugin_state_));
    last_rendered_time_ = std::chrono::high_resolution_clock::now();
}

} // namespace xen

// This creates new instances of the plugin. Must be in the global namespace.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new xen::XenProcessor{};
}
