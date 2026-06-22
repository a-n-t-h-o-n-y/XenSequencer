#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <xen/command.hpp>
#include <xen/command_catalog.hpp>
#include <xen/double_buffer.hpp>
#include <xen/engine_state_mailbox.hpp>
#include <xen/message_level.hpp>
#include <xen/midi_engine.hpp>
#include <xen/state.hpp>
#include <xen/submission_effects.hpp>
#include <xen/workspace_settings.hpp>

namespace xen
{

class XenProcessor : public juce::AudioProcessor
{
  private:
    WorkspaceSettingsStore workspace_settings_store_;

  public:
    PluginState plugin_state;
    int editor_width{1400};
    int editor_height{350};

  public:
    // Used to send validated project snapshots to the audio thread.
    EngineStateMailbox pending_engine_state_update;

  public:
    explicit XenProcessor(
        SubmissionEffects::FailurePoint effect_failure =
            SubmissionEffects::FailurePoint::None,
        juce::File workspace_settings_file = WorkspaceSettingsStore::default_file());

    ~XenProcessor() override = default;

  public:
    [[nodiscard]] auto get_project_snapshot() const -> ProjectSnapshot;
    [[nodiscard]] auto get_library_snapshot() const -> LibrarySnapshot;

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
     * @param context Selection and expected project revision supplied by the caller
     */
    auto execute_command_string(std::string const &command_string,
                                CommandContext const &context)
        -> CommandApplicationResult;

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
        ProjectState const *project{};
        MidiEngine midi_engine;
    } audio_thread_state_;

    CommandCatalog command_catalog_;
    SubmissionEffects::FailurePoint effect_failure_;

  private:
    void publish_project_snapshot();

  public:
    DoubleBuffer<AudioThreadStateForGUI> audio_thread_state_for_gui;
};

} // namespace xen
