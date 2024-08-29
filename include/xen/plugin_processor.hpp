#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace xen
{

class PluginProcessor : public juce::AudioProcessor
{
  public:
    PluginProcessor();

  public:
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    auto isBusesLayoutSupported(BusesLayout const &layouts) const -> bool override;

    auto getName() const -> juce::String const override;

    auto hasEditor() const -> bool override;

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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace xen