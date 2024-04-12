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
#include <xen/gui/phrase_editor.hpp>

#include <xen/gui/accordion.hpp>
#include <xen/gui/directory_view.hpp>
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
 * The main window for the plugin, holding all other components.
 *
 * This component's main purpose is as a box of other components. It is responsible for
 * updating all child components with the current state of the timeline.
 */
class PluginWindow : public juce::Component
{
  public:
    gui::PhraseDirectoryViewComponent phrase_directory_view;
    gui::Accordion<gui::ActiveSessions> active_sessions_accordion;
    gui::ActiveSessions &active_sessions;
    gui::Timeline gui_timeline;
    gui::PhraseEditor phrase_editor;
    // gui::TuningBox tuning_box; // TODO
    gui::CommandBar command_bar;
    gui::StatusBar status_bar;

  public:
    PluginWindow(XenTimeline &tl, CommandHistory &cmd_history,
                 XenCommandTree const &command_tree);

  public:
    /**
     * Update all child components with the current state of the timeline.
     *
     * @param state The current state of the timeline
     * @param aux The current aux state of the timeline
     * @param metadata The current metadata of the timeline
     */
    auto update(State const &state, AuxState const &aux, Metadata const &metadata)
        -> void;

    /**
     * Set the focus of the plugin window by ComponentID
     *
     * @param component_id The ComponentID of the component to focus
     * @throws std::invalid_argument if the ComponentID is not found
     */
    auto set_focus(std::string component_id) -> void;

  protected:
    auto resized() -> void override;
};

} // namespace xen::gui