#pragma once

#include "xen_processor.hpp"

#include "gui/heading.hpp"
#include "gui/phrase_editor.hpp"
#include "gui/tuning.hpp"
#include "state.hpp"

namespace xen
{

class PluginEditor : public juce::AudioProcessorEditor
{

  public:
    explicit PluginEditor(XenProcessor &, State const &);

  public:
    auto set_state(State const &) -> void;

  protected:
    auto resized() -> void override;

  private:
    gui::Heading heading_{"XenSequencer"};
    gui::PhraseEditor phrase_editor_;
    gui::TuningBox tuning_box_;

  private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    XenProcessor &processor_ref_;

    State cache_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace xen