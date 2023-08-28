#pragma once

#include <map>
#include <string>

#include <juce_gui_basics/juce_gui_basics.h>

#include "command_core.hpp"
#include "gui/plugin_window.hpp"
#include "key_core.hpp"
#include "state.hpp"
#include "xen_processor.hpp"

namespace xen
{

class XenEditor : public juce::AudioProcessorEditor
{
  public:
    explicit XenEditor(XenProcessor &);

  public:
    auto update(State const &, AuxState const &) -> void;

  protected:
    auto resized() -> void override;

  private:
    auto update_key_listeners(std::string const &filename) -> void;

  private:
    XenProcessor const &processor_;

    gui::PluginWindow plugin_window_;

    std::map<std::string, KeyConfigListener> key_config_listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XenEditor)
};

} // namespace xen