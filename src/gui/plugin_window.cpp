#include <xen/gui/plugin_window.hpp>

#include <filesystem>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <sequence/sequence.hpp>

#include <xen/command_history.hpp>
#include <xen/gui/active_sessions.hpp>
#include <xen/gui/bottom_bar.hpp>
#include <xen/gui/command_bar.hpp>
#include <xen/gui/phrase_editor.hpp>
#include <xen/gui/timeline.hpp>
#include <xen/key_core.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen::gui
{

PluginWindow::PluginWindow(juce::File const &phrase_library_dir,
                           CommandHistory &cmd_history)
    : phrases_view_accordion{"Phrases", phrase_library_dir},
      phrases_view{phrases_view_accordion.child}, bottom_bar{cmd_history}
{
    this->addAndMakeVisible(phrases_view_accordion);
    this->addAndMakeVisible(center_component);
    // this->addAndMakeVisible(phrase_editor);

    phrases_view_accordion.set_flexitem(juce::FlexItem{}.withHeight(125.f));

    this->addAndMakeVisible(bottom_bar);
}

auto PluginWindow::update(SequencerState const &state, AuxState const &aux,
                          std::string const &display_name) -> void
{
    phrases_view.active_sessions_view.update_this_instance_name(display_name);

    center_component.update_ui(state, aux);
    center_component.sequence_view.select(aux.selected.cell);

    bottom_bar.input_mode_indicator.set(aux.input_mode);
}

auto PluginWindow::set_focus(std::string component_id) -> void
{
    component_id = to_lower(component_id);
    // TODO use a lambda to check if visible then to set focus that is generic for all.

    if (component_id == to_lower(bottom_bar.command_bar.getComponentID().toStdString()))
    {
        if (bottom_bar.command_bar.hasKeyboardFocus(true))
        {
            return;
        }
        bottom_bar.command_bar.focus();
    }
    else if (component_id ==
             to_lower(center_component.sequence_view.getComponentID().toStdString()))
    {
        if (center_component.sequence_view.hasKeyboardFocus(true))
        {
            return;
        }
        center_component.sequence_view.grabKeyboardFocus();
    }
    else
    {
        throw std::invalid_argument("Invalid Component Given: '" + component_id + '\'');
    }
}

auto PluginWindow::show_component(std::string component_id) -> void
{
    component_id = to_lower(component_id);

    if (component_id == to_lower(bottom_bar.command_bar.getComponentID().toStdString()))
    {
        bottom_bar.show_command_bar();
    }
    else if (component_id ==
             to_lower(bottom_bar.status_bar.getComponentID().toStdString()))
    {
        bottom_bar.show_status_bar();
    }
    else if (component_id ==
             to_lower(center_component.sequence_view.getComponentID().toStdString()))
    {
        center_component.sequence_view.setVisible(true);
    }
    else
    {
        throw std::invalid_argument("Invalid Component Given: '" + component_id + '\'');
    }
    // TODO Library
    // TODO These Library/Sequencer changes should also call down to the status bar's
    // indicator to change the letter displayed?
}

auto PluginWindow::resized() -> void
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;

    flexbox.items.add(phrases_view_accordion.get_flexitem());
    flexbox.items.add(juce::FlexItem(center_component).withFlex(1.f));
    flexbox.items.add(
        juce::FlexItem(bottom_bar).withHeight(InputModeIndicator::preferred_size));

    flexbox.performLayout(this->getLocalBounds());
}

} // namespace xen::gui