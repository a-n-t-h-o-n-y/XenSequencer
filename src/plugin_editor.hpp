#pragma once

#include "xen_processor.hpp"

#include "gui/number_box.hpp"
#include "gui/tuning.hpp"

namespace xen
{

class PluginEditor : public juce::AudioProcessorEditor
{

  public:
    explicit PluginEditor(XenProcessor &);
    ~PluginEditor() override;

    void paint(juce::Graphics &) override;

    void resized() override;

  private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    XenProcessor &processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)

  private:
    gui::NumberBox<float> bpm_box_{juce::NormalisableRange<float>{20.f, 999.f, 0.1f},
                                   120.f, 6, false};
    gui::TuningBox tuning_box_;
};

} // namespace xen