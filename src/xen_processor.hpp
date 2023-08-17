#pragma once

#include <chrono>
#include <mutex>

#include <juce_audio_processors/juce_audio_processors.h>

#include "xen_command_core.hpp"
#include "plugin_processor.hpp"
#include "state.hpp"
#include "xen_timeline.hpp"

namespace xen
{

class XenProcessor : public PluginProcessor
{
  public:
    DAWState daw_state;
    XenTimeline timeline;
    XenCommandCore command_core;

  public:
    XenProcessor();

  protected:
    auto processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) -> void override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

  private:
    /**
     * @brief Render the current State to MIDI and save in rendered_ and update time.
     */
    auto render() -> void;

  private:
    State plugin_state_; // Convenience variable, not necessary but saves cycles
    juce::MidiBuffer rendered_;
    std::chrono::high_resolution_clock::time_point last_rendered_time_;

  private:
    juce::AudioParameterFloat *base_frequency_;
};

} // namespace xen