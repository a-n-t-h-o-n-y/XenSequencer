#pragma once

#include "xen_processor.hpp"

#include <juce_gui_basics/juce_gui_basics.h>

#include "gui/plugin_window.hpp"
#include "state.hpp"

namespace xen
{

class XenEditor : public juce::AudioProcessorEditor
{
  public:
    explicit XenEditor(XenProcessor &);

  public:
    auto update(State const &) -> void;

  protected:
    auto resized() -> void override;

  private:
    gui::PluginWindow plugin_window_;

    XenProcessor &processor_ref_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XenEditor)
};

} // namespace xen