#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <xen/command.hpp>
#include <xen/command_action.hpp>
#include <xen/command_catalog.hpp>
#include <xen/command_history.hpp>
#include <xen/double_buffer.hpp>
#include <xen/engine_state_mailbox.hpp>
#include <xen/message_level.hpp>
#include <xen/midi_engine.hpp>
#include <xen/state.hpp>

namespace xen
{

class XenProcessor : public juce::AudioProcessor
{
  public:
    PluginState plugin_state;
    int editor_width{1400};
    int editor_height{350};

  public:
    // Used to send new EngineState snapshots to the Audio Thread.
    EngineStateMailbox pending_engine_state_update;

  public:
    XenProcessor();

    ~XenProcessor() override = default;

  public:
    [[nodiscard]] auto get_engine_snapshot() const -> EngineSnapshot;
    [[nodiscard]] auto get_ui_snapshot_version() const noexcept -> std::uint64_t;

  public:
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    void processBlock(juce::AudioBuffer<double> &, juce::MidiBuffer &) override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

    void getStateInformation(juce::MemoryBlock &dest_data) override;

    void setStateInformation(void const *data, int sizeInBytes) override;

    /**
     * Execute a string through this processor's command catalog.
     *
     * @details This will normalize the input string, execute it on plugin_state and
     * return the resulting status.
     * @param command_string The command string to execute
     */
    auto execute_command_string(std::string const &command_string)
        -> std::pair<MessageLevel, std::string>;

    [[nodiscard]] auto command_catalog() noexcept -> CommandCatalog &;
    [[nodiscard]] auto command_catalog() const noexcept -> CommandCatalog const &;

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
        EngineState const *sequencer{};
        MidiEngine midi_engine;
    } audio_thread_state_;

    int previous_commit_id_{-1};
    CommandCatalog command_catalog_;
    std::vector<BoundCommand> previous_command_chain_{};
    std::atomic<std::uint64_t> ui_snapshot_version_{0};

  private:
    void notify_ui_state_changed() noexcept;

  public:
    DoubleBuffer<AudioThreadStateForGUI> audio_thread_state_for_gui;
};

} // namespace xen
