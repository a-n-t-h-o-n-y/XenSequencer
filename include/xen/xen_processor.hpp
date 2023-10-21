#pragma once

#include <chrono>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <sequence/sequence.hpp>

#include <xen/command.hpp>
#include <xen/plugin_processor.hpp>
#include <xen/state.hpp>
#include <xen/xen_command_tree.hpp>
#include <xen/xen_timeline.hpp>

namespace xen
{

class XenProcessor : public PluginProcessor
{
  public:
    DAWState daw_state;
    XenTimeline timeline;

  public:
    XenProcessor();

  protected:
    auto processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) -> void override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

    auto getStateInformation(juce::MemoryBlock &dest_data) -> void override;

    auto setStateInformation(void const *data, int sizeInBytes) -> void override;

  private:
    /**
     * @brief Render the current State to MIDI and save in rendered_ and update time.
     */
    auto render() -> void;

    auto add_midi_corrections(juce::MidiBuffer &buffer,
                              juce::AudioPlayHead::PositionInfo const &position,
                              long samples_in_phrase) -> void;

  private:
    State plugin_state_; // Convenience variable, not necessary but saves cycles
    juce::MidiBuffer rendered_;
    std::chrono::high_resolution_clock::time_point last_rendered_time_;

    juce::MidiMessage last_note_event_{juce::MidiMessage::noteOff(1, 0)};
    juce::MidiMessage last_pitch_bend_event_{juce::MidiMessage::pitchWheel(1, 0x2000)};
};

} // namespace xen