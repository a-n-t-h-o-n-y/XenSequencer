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
#include <xen/double_buffer.hpp>
#include <xen/gui/active_sessions.hpp>
#include <xen/gui/bottom_bar.hpp>
#include <xen/gui/center_component.hpp>
#include <xen/gui/color_ids.hpp>
#include <xen/gui/command_bar.hpp>
#include <xen/gui/directory_view.hpp>
#include <xen/gui/title_bar.hpp>
#include <xen/key_core.hpp>
#include <xen/state.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen
{
struct SequencerState;
struct AuxState;
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
    TitleBar title_bar;
    gui::CenterComponent center_component;
    gui::BottomBar bottom_bar;

  public:
    PluginWindow(juce::File const &sequence_library_dir,
                 juce::File const &tuning_library_dir, CommandHistory &cmd_history,
                 DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state);

  public:
    /**
     * Update all child components with the current state of the timeline.
     *
     * @param state The current state of the timeline
     * @param aux The current aux state of the timeline
     * @param display_name The name of the current instance
     */
    auto update(SequencerState const &state, AuxState const &aux,
                std::string const &display_name) -> void;

    /**
     * Set the focus of the plugin window by ComponentID
     *
     * @param component_id The ComponentID of the component to focus
     * @throws std::invalid_argument if the ComponentID is not found
     */
    auto set_focus(std::string component_id) -> void;

    /**
     * Lookup up the component by ComponentID and update the GUI to show it.
     *
     * @param component_id The ComponentID of the component to show
     * @throws std::invalid_argument if the ComponentID is not found
     */
    auto show_component(std::string component_id) -> void;

  public:
    auto resized() -> void override;
};

} // namespace xen::gui