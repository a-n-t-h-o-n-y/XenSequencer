#pragma once

#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <xen/active_sessions.hpp>
#include <xen/command.hpp>
#include <xen/command_history.hpp>
#include <xen/double_buffer.hpp>
#include <xen/gui/themes.hpp>
#include <xen/lock_free_queue.hpp>
#include <xen/midi_engine.hpp>
#include <xen/plugin_processor.hpp>
#include <xen/state.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen
{

class XenProcessor : public PluginProcessor
{
  public:
    PluginState plugin_state;
    ActiveSessions active_sessions;
    XenCommandTree command_tree;
    int editor_width{1000};
    int editor_height{300};

  public:
    // Used to send new SequencerState to the Audio Thread.
    LockFreeQueue<SequencerState, 16> new_state_transfer_queue;

  public:
    XenProcessor();

    ~XenProcessor() override = default;

  protected:
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    void processBlock(juce::AudioBuffer<double> &, juce::MidiBuffer &) override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

    void getStateInformation(juce::MemoryBlock &dest_data) override;

    void setStateInformation(void const *data, int sizeInBytes) override;

  private:
    struct AudioThreadState
    {
        DAWState daw;
        std::uint64_t accumulated_sample_count{0};
        MidiEngine midi_engine;
    } audio_thread_state_;

  public:
    DoubleBuffer<AudioThreadStateForGUI> audio_thread_state_for_gui;
};

} // namespace xen