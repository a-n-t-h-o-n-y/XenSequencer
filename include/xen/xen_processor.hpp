#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <xen/command.hpp>
#include <xen/double_buffer.hpp>
#include <xen/engine.hpp>
#include <xen/lock_free_optional.hpp>
#include <xen/message_level.hpp>
#include <xen/midi_engine.hpp>
#include <xen/runtime_command_tree.hpp>
#include <xen/runtime_state.hpp>

namespace xen
{

class XenProcessor : public juce::AudioProcessor
{
  public:
    Engine engine;
    RuntimeState runtime_state;
    int editor_width{1400};
    int editor_height{350};

  public:
    // Used to send new SequencerState to the Audio Thread.
    LockFreeOptional<SequencerState> pending_state_update;

  public:
    XenProcessor();

    ~XenProcessor() override = default;

  public:
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    void processBlock(juce::AudioBuffer<double> &, juce::MidiBuffer &) override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

    void getStateInformation(juce::MemoryBlock &dest_data) override;

    void setStateInformation(void const *data, int sizeInBytes) override;

    /**
     * Execute a string as a command using runtime command handling and engine commands.
     *
     * @details This will normalize the input string, execute runtime side effects when
     * applicable, and execute engine commands otherwise.
     * return the resulting status.
     * @param command_string The command string to execute
     */
    auto execute_command_string(std::string const &command_string)
        -> std::pair<MessageLevel, std::string>;

    [[nodiscard]] auto guide_text(std::string const &partial_command) const
        -> std::string;

    [[nodiscard]] auto complete_id(std::string const &partial_command) const
        -> std::string;

  public:
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    auto getName() const -> juce::String const override;

    auto hasEditor() const -> bool override;

    auto supportsMPE() const -> bool override;
    auto acceptsMidi() const -> bool override;
    auto producesMidi() const -> bool override;
    auto isMidiEffect() const -> bool override;
    auto getTailLengthSeconds() const -> double override;

    auto getNumPrograms() -> int override;
    auto getCurrentProgram() -> int override;
    void setCurrentProgram(int index) override;
    auto getProgramName(int index) -> juce::String const override;
    void changeProgramName(int index, juce::String const &newName) override;

  private:
    struct AudioThreadState
    {
        DAWState daw;
        SequencerState sequencer{};
        SampleCount accumulated_sample_count{0};
        MidiEngine midi_engine;
    } audio_thread_state_;

    std::string previous_command_string_{""};
    RuntimeCommandTree runtime_command_tree_;

  public:
    DoubleBuffer<AudioThreadStateForGUI> audio_thread_state_for_gui;
};

} // namespace xen
