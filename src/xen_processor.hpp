#pragma once

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
    ~XenProcessor() override = default;

  public:
    auto processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) -> void override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

  private:
    State state_ = demo_state();
    juce::MidiBuffer rendered_ = render_to_midi(state_to_timeline(state_));

    juce::AudioParameterFloat *base_frequency_;
};

} // namespace xen