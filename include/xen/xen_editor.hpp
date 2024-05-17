#pragma once

#include <filesystem>
#include <map>
#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/plugin_window.hpp>
#include <xen/key_core.hpp>
#include <xen/state.hpp>
#include <xen/xen_command_tree.hpp>
#include <xen/xen_processor.hpp>

namespace xen::gui
{

class XenEditor : public juce::AudioProcessorEditor
{
  public:
    PluginWindow plugin_window;

  public:
    explicit XenEditor(XenProcessor &, int width, int height);

  public:
    /**
     * Updates GUI components using the current processor_.plugin_state member.
     */
    auto update_ui() -> void;

    /**
     * Set or Update the key listeners for the plugin window.
     *
     * @param default_keys The path to the default key configuration file
     * @param user_keys The path to the user key configuration file
     * @throws std::runtime_error if the key configuration files cannot be read or have
     * errors
     */
    auto update_key_listeners(juce::File const &default_keys,
                              juce::File const &user_keys) -> void;

  public:
    auto resized() -> void override;

    [[nodiscard]] auto createKeyboardFocusTraverser()
        -> std::unique_ptr<juce::ComponentTraverser> override;

  private:
    /**
     * Execute a command string in the plugin window.
     *
     * @details This will normalize the input string, execute it on
     * processor_.plugin_state and send the resulting status to the status bar.
     * @param command_string The command string to execute
     */
    auto execute_command_string(std::string const &command_string) -> void;

    /**
     * Set the key listeners for the plugin window.
     *
     * @details This removes previous_listeners and adds new_listeners.
     * update_key_listeners should be used in most cases.
     */
    auto set_key_listeners(
        std::map<std::string, xen::KeyConfigListener> previous_listeners,
        std::map<std::string, xen::KeyConfigListener> &new_listeners) -> void;

  private:
    XenProcessor &processor_;

    std::map<std::string, KeyConfigListener> key_config_listeners_;

    sl::Lifetime lifetime_;
};

} // namespace xen::gui