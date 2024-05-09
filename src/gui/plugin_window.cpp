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
#include <xen/gui/color_ids.hpp>
#include <xen/gui/command_bar.hpp>
#include <xen/key_core.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen::gui
{

PluginWindow::PluginWindow(juce::File const &sequence_library_dir,
                           juce::File const &tuning_library_dir,
                           CommandHistory &cmd_history)
    : center_component{sequence_library_dir, tuning_library_dir},
      bottom_bar{cmd_history}
{
    // TODO use custom font
    label.setFont({
        juce::Font::getDefaultMonospacedFontName(),
        16.f,
        juce::Font::bold,
    });
    label.setJustificationType(juce::Justification::centred);
    label.setText("XenSequencer", juce::dontSendNotification);
    this->addAndMakeVisible(label);
    this->addAndMakeVisible(center_component);
    this->addAndMakeVisible(bottom_bar);
}

auto PluginWindow::update(SequencerState const &state, AuxState const &aux,
                          std::string const &display_name) -> void
{
    center_component.library_view.active_sessions_list.update_this_instance_name(
        display_name);

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
    else if (component_id ==
             to_lower(center_component.library_view.sequences_list.getComponentID()
                          .toStdString()))
    {
        if (center_component.library_view.sequences_list.hasKeyboardFocus(true))
        {
            return;
        }
        center_component.library_view.sequences_list.grabKeyboardFocus();
    }
    else if (component_id == to_lower(center_component.library_view.active_sessions_list
                                          .getComponentID()
                                          .toStdString()))
    {
        if (center_component.library_view.active_sessions_list.hasKeyboardFocus(true))
        {
            return;
        }
        center_component.library_view.active_sessions_list.grabKeyboardFocus();
    }
    else if (component_id ==
             to_lower(center_component.library_view.tunings_list.getComponentID()
                          .toStdString()))
    {
        if (center_component.library_view.tunings_list.hasKeyboardFocus(true))
        {
            return;
        }
        center_component.library_view.tunings_list.grabKeyboardFocus();
    }
    else
    {
        throw std::invalid_argument("Invalid Component Given: " +
                                    single_quote(component_id));
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
        center_component.show_sequence_view();
        bottom_bar.library_sequencer_toggle.display_library_indicator();
    }
    else if (component_id ==
             to_lower(center_component.library_view.getComponentID().toStdString()))
    {
        center_component.show_library_view();
        bottom_bar.library_sequencer_toggle.display_sequencer_indicator();
    }
    else
    {
        throw std::invalid_argument("Invalid Component Given: " +
                                    single_quote(component_id));
    }
    // TODO Library
    // TODO These Library/Sequencer changes should also call down to the status bar's
    // indicator to change the letter displayed?
}

auto PluginWindow::resized() -> void
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;

    flexbox.items.add(juce::FlexItem(label).withHeight(23.f));
    flexbox.items.add(juce::FlexItem(center_component).withFlex(1.f));
    flexbox.items.add(
        juce::FlexItem(bottom_bar).withHeight(InputModeIndicator::preferred_size));

    flexbox.performLayout(this->getLocalBounds());
}

auto PluginWindow::lookAndFeelChanged() -> void
{
    label.setColour(juce::Label::textColourId,
                    label.findColour((int)DirectoryViewColorIDs::ItemText));
    label.setColour(juce::Label::backgroundColourId,
                    label.findColour((int)DirectoryViewColorIDs::ItemBackground));

    // TODO move label into its own class, then you can remove this overload that has to
    // call on each child manually
    center_component.lookAndFeelChanged();
    bottom_bar.lookAndFeelChanged();
}

} // namespace xen::gui