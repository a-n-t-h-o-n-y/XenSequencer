#pragma once

#include <atomic>
#include <mutex>

#include <juce_audio_processors/juce_audio_processors.h>

#include <sequence/measure.hpp>

#include "midi.hpp"
#include "plugin_processor.hpp"
#include "state.hpp"
#include "timeline.hpp"

namespace xen
{

class XenProcessor : public PluginProcessor
{
  public:
    XenProcessor();

  public:
    // auto thread_safe_update(State const &state) -> void
    // {
    //     timeline_.add_state(state);
    //     this->thread_safe_render();
    // }

  protected:
    auto processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) -> void override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

    // private:
    //   auto thread_safe_render() -> void;

  private:
    Timeline<State> timeline_;

    // TODO anytime timeline_ is modified change this, should it be in timeline class?
    // you might want to integrate this with command core.
    std::atomic<bool> render_needed_ = true;

    DAWState daw_state_;

    // State state_ = init_state();
    // std::mutex state_mutex_;

    // ProcessBlock should be the only thing calling this, from a single thread
    // so maybe the mutex is unnecessary?
    juce::MidiBuffer rendered_;
    // std::mutex rendered_mutex_;

  private:
    juce::AudioParameterFloat *base_frequency_;
};

} // namespace xen