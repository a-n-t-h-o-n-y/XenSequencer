#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <sequence/sequence.hpp>

#include <xen/active_sessions.hpp>
#include <xen/command.hpp>
#include <xen/command_history.hpp>
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

  public:
    // Used to send new SequencerState to the Audio Thread.
    LockFreeQueue<SequencerState, 16> new_state_transfer_queue;

  public:
    XenProcessor();

    ~XenProcessor() override = default;

  protected:
    auto processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) -> void override;

    auto processBlock(juce::AudioBuffer<double> &, juce::MidiBuffer &) -> void override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

    auto getStateInformation(juce::MemoryBlock &dest_data) -> void override;

    auto setStateInformation(void const *data, int sizeInBytes) -> void override;

  private:
    /**
     * Render the current SequencerState to MIDI and save in rendered_ and update time.
     */
    auto render() -> void;

    auto add_midi_corrections(juce::MidiBuffer &buffer,
                              juce::AudioPlayHead::PositionInfo const &position,
                              long samples_in_phrase) -> void;

  private:
    struct AudioThreadState
    {
        DAWState daw;
        std::uint64_t accumulated_sample_count{0};
        MidiEngine midi_engine;
    } audio_thread_state_;

    // SequencerState sequencer_state_copy_; // Not necessary, but saves cycles.
    // juce::MidiBuffer rendered_;

    // bool is_playing_{false};
    // juce::MidiMessage last_note_event_{juce::MidiMessage::noteOff(1, 0)};
    // juce::MidiMessage last_pitch_bend_event_{juce::MidiMessage::pitchWheel(1,
    // 0x2000)};
};

} // namespace xen