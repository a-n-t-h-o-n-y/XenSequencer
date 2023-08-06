#include "xen_processor.hpp"

#include <cstdint>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>

#include "midi.hpp"
#include "xen_editor.hpp"

#include <sequence/measure.hpp>

namespace xen
{

XenProcessor::XenProcessor() : timeline_{init_state()}
{
    this->addParameter(base_frequency_ = new juce::AudioParameterFloat(
                           juce::ParameterID{"base_frequency", 1},
                           "Base Frequency", // parameter name
                           juce::NormalisableRange<float>(20.f, 20'000.f, 1.f, 0.2f),
                           440.f)); // default value

    timeline_.add_state(init_state());
    // this->thread_safe_update(state_);
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

    // Take copies for thread safety
    // auto state_copy = timeline_.get_state();
    // auto state_copy = State{};
    // {
    //     auto const lock = std::lock_guard{state_mutex_};
    //     state_copy = state_;
    // }

    // : throw std::runtime_error{"BPM is not valid"};

    // if sample rate, bpm, or base frequency has changed, update state and rerender
    // midi. You do this here and you do it on GUI state updates.
    // if (state_copy.sample_rate != this->getSampleRate() || state_copy.bpm != bpm_daw
    // ||
    //     state_copy.base_frequency != *(this->base_frequency_))
    // {
    //     state_copy.sample_rate = static_cast<std::uint32_t>(this->getSampleRate());
    //     state_copy.bpm = bpm_daw;
    //     state_copy.base_frequency = *(this->base_frequency_);
    //     this->thread_safe_update(state_copy);
    // }

    // Check if MIDI needs to be rendered because of DAW or GUI changes.
    auto const bpm_daw =
        position->getBpm() ? static_cast<float>(*(position->getBpm())) : 120.f;

    // TODO can this be removed so it is only called if update is needed?
    auto const state = timeline_.get_state();

    if (daw_state_.sample_rate != this->getSampleRate() || daw_state_.bpm != bpm_daw ||
        render_needed_)
    {
        render_needed_ = false;
        daw_state_.sample_rate = static_cast<std::uint32_t>(this->getSampleRate());
        daw_state_.bpm = bpm_daw;
        rendered_ = render_to_midi(state_to_timeline(daw_state_, state));
    }

    // Find current MIDI events to send according to PlayHead position
    auto const samples_in_phrase =
        sequence::samples_count(state.phrase, daw_state_.sample_rate, daw_state_.bpm);

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

    // auto rendered_copy = juce::MidiBuffer{};
    // {
    //     auto const lock = std::lock_guard{rendered_mutex_};
    //     rendered_copy = rendered_;
    // }
    auto new_midi_buffer = find_subrange(rendered_, begin, end, (int)samples_in_phrase);
    midi_messages.swapWith(new_midi_buffer);
}

auto XenProcessor::createEditor() -> juce::AudioProcessorEditor *
{
    return new XenEditor{*this};
}

// auto XenProcessor::thread_safe_render(DAWState const &daw_state, State const &state)
//     -> void
// {
//     auto midi = render_to_midi(state_to_timeline(daw_state, state));
//     {
//         auto const lock = std::lock_guard{rendered_mutex_};
//         rendered_ = std::move(midi);
//     }
// }

} // namespace xen

// This creates new instances of the plugin. Must be in the global namespace.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new xen::XenProcessor{};
}
