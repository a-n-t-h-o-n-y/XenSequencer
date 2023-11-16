#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <sequence/sequence.hpp>

#include <xen/command_history.hpp>
#include <xen/gui/active_sessions.hpp>
#include <xen/gui/command_bar.hpp>
#include <xen/gui/heading.hpp>
#include <xen/gui/phrase_editor.hpp>
#include <xen/gui/status_bar.hpp>
#include <xen/gui/timeline.hpp>
#include <xen/key_core.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen
{
struct State;
struct AuxState;
struct Metadata;
} // namespace xen

namespace xen::gui
{

/**
 * @brief The main window for the plugin, holding all other components.
 */
class PluginWindow : public juce::Component
{
  private:
    XenTimeline &timeline_;
    std::map<std::string, KeyConfigListener> key_config_listeners_;

    sl::Signal<void(std::string const &)> on_focus_change_request_;
    sl::Lifetime lifetime_;

    xen::XenCommandTree command_tree_;

  public:
    gui::Heading heading;
    gui::ActiveSessions active_sessions;
    gui::Timeline gui_timeline;
    gui::PhraseEditor phrase_editor;
    // gui::TuningBox tuning_box; // TODO
    gui::CommandBar command_bar;
    gui::StatusBar status_bar;

  public:
    explicit PluginWindow(XenTimeline &tl, CommandHistory &cmd_history);

  public:
    auto update(State const &state, AuxState const &aux, Metadata const &metadata)
        -> void;

    auto set_key_listeners(std::map<std::string, KeyConfigListener> previous_listeners,
                           std::map<std::string, KeyConfigListener> &new_listeners)
        -> void;

  protected:
    auto resized() -> void override;

  private:
    auto update_key_listeners(std::filesystem::path const &default_keys,
                              std::filesystem::path const &user_keys) -> void;
};

} // namespace xen::gui