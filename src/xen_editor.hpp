#pragma once

#include "xen_processor.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include "command_core.hpp"
#include "gui/plugin_window.hpp"
#include "state.hpp"

namespace xen
{

class XenEditor : public juce::AudioProcessorEditor
{
  public:
    explicit XenEditor(XenProcessor &);

  public:
    auto update(State const &, SelectedState const &) -> void;

  protected:
    auto resized() -> void override;

  private:
    gui::PluginWindow plugin_window_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XenEditor)
};

} // namespace xen