#pragma once

#include <map>
#include <memory>
#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

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
    void update();

    /**
     * Set or Update the key listeners for the plugin window.
     *
     * @param default_keys The path to the default key configuration file
     * @param user_keys The path to the user key configuration file
     * @throws std::runtime_error if the key configuration files cannot be read or have
     * errors
     */
    void update_key_listeners(juce::File const &default_keys,
                              juce::File const &user_keys);

  public:
    void resized() override;

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
    void execute_command_string(std::string const &command_string);

    /**
     * Set the key listeners for the plugin window.
     *
     * @details This removes previous_listeners and adds new_listeners.
     * update_key_listeners should be used in most cases.
     */
    void set_key_listeners(
        std::map<std::string, xen::KeyConfigListener> previous_listeners,
        std::map<std::string, xen::KeyConfigListener> &new_listeners);

  private:
    XenProcessor &processor_;

    std::map<std::string, KeyConfigListener> key_config_listeners_;

    sl::Lifetime lifetime_;

    juce::TooltipWindow tooltip_window_;
};

} // namespace xen::gui