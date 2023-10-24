#pragma once

#include <map>
#include <string>
#include <filesystem>

#include <juce_gui_basics/juce_gui_basics.h>
#include <signals_light/signal.hpp>

#include <xen/gui/plugin_window.hpp>
#include <xen/key_core.hpp>
#include <xen/state.hpp>
#include <xen/xen_processor.hpp>
#include <xen/xen_timeline.hpp>

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

    void paint(juce::Graphics &g) override
    {
        g.fillAll(juce::Colours::black);
    }

  private:
    auto update_key_listeners(std::filesystem::path const &default_keys, std::filesystem::path const& user_keys) -> void;

  private:
    XenTimeline const &timeline_;

    gui::PluginWindow plugin_window_;

    std::map<std::string, KeyConfigListener> key_config_listeners_;

    sl::Lifetime lifetime_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XenEditor)
};

} // namespace xen