#pragma once

#include <string>

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/double_buffer.hpp>
#include <xen/gui/bottom_bar.hpp>
#include <xen/gui/center_component.hpp>
#include <xen/gui/title_bar.hpp>

namespace xen
{
class CommandHistory;
struct PluginState;
} // namespace xen

namespace xen::gui
{

/**
 * The main window for the plugin, holding all other components.
 *
 * @details This component's main purpose is as a box of other components. It is
 * responsible for updating all child components with the current state of the timeline.
 */
class PluginWindow : public juce::Component
{
  public:
    TitleBar title_bar;
    CenterComponent center_component;
    BottomBar bottom_bar;

  public:
    PluginWindow(juce::File const &sequence_library_dir,
                 juce::File const &tuning_library_dir, CommandHistory &cmd_history,
                 DoubleBuffer<AudioThreadStateForGUI> const &audio_thread_state);

  public:
    /**
     * Update all child components with the current PluginState.
     *
     * @param ps The current state of the plugin.
     */
    void update(PluginState const &ps);

    /**
     * Set the focus of the plugin window by ComponentID
     *
     * @param component_id The ComponentID of the component to focus
     * @throws std::invalid_argument if the ComponentID is not found
     */
    void set_focus(std::string component_id);

    /**
     * Lookup up the component by ComponentID and update the GUI to show it.
     *
     * @param component_id The ComponentID of the component to show
     * @throws std::invalid_argument if the ComponentID is not found
     */
    void show_component(std::string component_id);

  public:
    void resized() override;
};

} // namespace xen::gui