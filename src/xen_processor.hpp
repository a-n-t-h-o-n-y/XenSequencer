#pragma once

#include <mutex>

#include <juce_audio_processors/juce_audio_processors.h>

#include <sequence/measure.hpp>

#include "midi.hpp"
#include "plugin_processor.hpp"
#include "state.hpp"

namespace xen
{

class XenProcessor : public PluginProcessor
{
  public:
    XenProcessor();

  public:
    auto thread_safe_update(State const &state) -> void
    {
        this->thread_safe_assign_state(state);
        this->thread_safe_render();
    }

  protected:
    auto processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) -> void override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

  private:
    auto thread_safe_render() -> void;

    auto thread_safe_assign_state(State state) -> void;

  private:
    State state_ = init_state();
    std::mutex state_mutex_;

    juce::MidiBuffer rendered_ = render_to_midi(state_to_timeline(state_));
    std::mutex rendered_mutex_;

    juce::AudioParameterFloat *base_frequency_;
};

} // namespace xen