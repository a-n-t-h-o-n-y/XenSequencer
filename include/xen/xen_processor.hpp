#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <xen/audio_thread_state_exchange.hpp>
#include <xen/midi_engine.hpp>
#include <xen/sequencer_session_port.hpp>
#include <xen/state.hpp>
#include <xen/submission_effects.hpp>
#include <xen/workspace_settings.hpp>

namespace xen
{

class XenProcessor : public juce::AudioProcessor
{
  public:
    int editor_width{1400};
    int editor_height{350};

  public:
    explicit XenProcessor(SubmissionEffects::FailurePoint effect_failure =
                              SubmissionEffects::FailurePoint::None,
                          std::filesystem::path workspace_settings_file =
                              WorkspaceSettingsStore::default_file());

    ~XenProcessor() override = default;

    [[nodiscard]] auto session() noexcept -> SequencerSessionPort &;
    [[nodiscard]] auto session() const noexcept -> SequencerSessionPort const &;
    [[nodiscard]] auto audio_thread_state_snapshot() const noexcept
        -> AudioThreadStateForGUI;

  public:
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    void processBlock(juce::AudioBuffer<double> &, juce::MidiBuffer &) override;

    auto createEditor() -> juce::AudioProcessorEditor * override;

    void getStateInformation(juce::MemoryBlock &dest_data) override;

    void setStateInformation(void const *data, int sizeInBytes) override;

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
        OutputId output_id{CURRENT_INSTANCE_OUTPUT_ID};
        MidiEngine midi_engine;
    } audio_thread_state_;

    std::unique_ptr<SequencerSessionPort> session_;
    AudioThreadStateExchange audio_thread_state_for_gui_;
};

} // namespace xen
