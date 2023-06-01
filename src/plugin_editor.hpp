#pragma once

#include "xen_processor.hpp"

#include "gui/heading.hpp"
#include "gui/phrase_editor.hpp"
#include "gui/tuning.hpp"

namespace xen
{

class PluginEditor : public juce::AudioProcessorEditor
{

  public:
    explicit PluginEditor(XenProcessor &);

    ~PluginEditor() override;

  protected:
    auto resized() -> void override;

  private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    XenProcessor &processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)

  private:
    gui::Heading heading_{"XenSequencer"};
    gui::PhraseEditor phrase_editor_;
    gui::TuningBox tuning_box_;
};

} // namespace xen