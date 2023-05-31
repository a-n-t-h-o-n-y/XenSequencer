#include "xen_processor.hpp"

#include <cassert> //temp

#include <cstdint>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>

#include "midi.hpp"
#include "plugin_editor.hpp"

#include <sequence/measure.hpp>

namespace xen
{

XenProcessor::XenProcessor()
{
    this->addParameter(base_frequency_ = new juce::AudioParameterFloat(
                           "base_frequency", // parameter ID
                           "Base Frequency", // parameter name
                           juce::NormalisableRange<float>(20.f, 20'000.f, 1.f, 0.2f),
                           440.f)); // default value
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

    // Check if MIDI needs to be rendered
    auto const bpm =
        position->getBpm() ? static_cast<float>(*(position->getBpm())) : 120.f;
    // : throw std::runtime_error{"BPM is not valid"};
    if (state_.sample_rate != this->getSampleRate() || state_.bpm != bpm ||
        *(this->base_frequency_) != state_.base_frequency)
    {
        state_.sample_rate = static_cast<std::uint32_t>(this->getSampleRate());
        state_.bpm = bpm;
        state_.base_frequency = *(this->base_frequency_);
        rendered_ = render_to_midi(state_to_timeline(state_));
    }

    // Find current MIDI events to send according to PlayHead position
    auto const samples_in_phrase =
        sequence::samples_count(state_.phrase, state_.sample_rate, state_.bpm);

    auto const [begin, end] = [&] {
        auto const current_sample =
            position->getTimeInSamples()
                ? *(position->getTimeInSamples())
                : throw std::runtime_error{"Sample position is not valid"};
        auto const begin = current_sample % samples_in_phrase;
        return std::pair{
            static_cast<int>(begin),
            static_cast<int>((begin + buffer.getNumSamples()) % samples_in_phrase),
        };
    }();

    auto new_midi_buffer = find_subrange(rendered_, begin, end, samples_in_phrase);
    midi_messages.swapWith(new_midi_buffer);
}

auto XenProcessor::createEditor() -> juce::AudioProcessorEditor *
{
    return new PluginEditor{*this};
}

} // namespace xen

// This creates new instances of the plugin. Must be in the global namespace.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new xen::XenProcessor{};
}
