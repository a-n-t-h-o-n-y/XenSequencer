#pragma once

#include <filesystem>
#include <map>
#include <string>

#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/plugin_window.hpp>
#include <xen/state.hpp>
#include <xen/xen_command_tree.hpp>
#include <xen/xen_processor.hpp>
#include <xen/xen_timeline.hpp>

namespace xen::gui
{

class XenEditor : public juce::AudioProcessorEditor
{
  public:
    explicit XenEditor(XenProcessor &);

  public:
    auto update(State const &, AuxState const &, Metadata const &) -> void;

    /**
     * @brief Set or Update the key listeners for the plugin window.
     *
     * @param default_keys The path to the default key configuration file
     * @param user_keys The path to the user key configuration file
     * @throws std::runtime_error if the key configuration files cannot be read or have
     * errors
     */
    auto update_key_listeners(std::filesystem::path const &default_keys,
                              std::filesystem::path const &user_keys) -> void;

  protected:
    auto resized() -> void override;

    auto paint(juce::Graphics &g) -> void override;

  private:
    XenTimeline &timeline_;

    std::map<std::string, KeyConfigListener> key_config_listeners_;

    sl::Lifetime lifetime_;

    sl::Signal<void(std::string const &)> on_focus_change_request_;

    XenCommandTree command_tree_;

    PluginWindow plugin_window_;
};

} // namespace xen