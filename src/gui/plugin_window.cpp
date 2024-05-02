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
#include <xen/gui/command_bar.hpp>
#include <xen/gui/phrase_editor.hpp>
#include <xen/gui/status_bar.hpp>
#include <xen/gui/timeline.hpp>
#include <xen/key_core.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen::gui
{

PluginWindow::PluginWindow(XenTimeline &tl, CommandHistory &cmd_history)
    : phrases_view_accordion{"Phrases", tl.get_aux_state().current_phrase_directory},
      phrases_view{phrases_view_accordion.child}, command_bar{cmd_history}
{
    this->addAndMakeVisible(phrases_view_accordion);
    this->addAndMakeVisible(gui_timeline);
    this->addAndMakeVisible(phrase_editor);

    phrases_view_accordion.set_flexitem(juce::FlexItem{}.withHeight(125.f));

    this->addChildComponent(command_bar);
    command_bar.setVisible(false);

    this->addAndMakeVisible(status_bar);

    phrases_view.directory_view.on_directory_change.connect(
        [&](juce::File const &directory) {
            auto aux = tl.get_aux_state();
            aux.current_phrase_directory = directory;
            tl.set_aux_state(std::move(aux), false);
        });
}

auto PluginWindow::update(SequencerState const &state, AuxState const &aux,
                          std::string const &display_name) -> void
{
    phrases_view.active_sessions_view.update_this_instance_name(display_name);

    phrase_editor.phrase.set(state, aux.selected);
    phrase_editor.phrase.select(aux.selected);

    status_bar.mode_display.set(aux.input_mode);

    gui_timeline.set(state.phrase, aux.selected);

    // TODO
    // tuning_box.set_tuning(state.tuning);
}

auto PluginWindow::set_focus(std::string component_id) -> void
{
    component_id = to_lower(component_id);
    // TODO use a lambda to check if visible then to set focus.

    if (component_id == to_lower(command_bar.getComponentID().toStdString()))
    {
        if (command_bar.hasKeyboardFocus(true))
        {
            return;
        }
        // command_bar.open();
    }
    else if (component_id == to_lower(phrase_editor.getComponentID().toStdString()))
    {
        if (phrase_editor.hasKeyboardFocus(true))
        {
            return;
        }
        // Uses a key listener set up by XenEditor.
        // phrase_editor.grabKeyboardFocus();
    }
    else
    {
        throw std::invalid_argument("Invalid Component Given: '" + component_id + '\'');
    }
    // TODO SequencesLibrary
    // TODO ActiveSessions
    // TODO TuningsLibrary
    // you'll need to expose the list box or provide some function to set focus
    // to the list box via this function,
}

auto PluginWindow::show_component(std::string component_id) -> void
{
    component_id = to_lower(component_id);

    if (component_id == to_lower(command_bar.getComponentID().toStdString()))
    {
        // command_bar.setVisible(true);
    }
    else if (component_id == to_lower(phrase_editor.getComponentID().toStdString()))
    {
        // phrase_editor.setVisible(true);
    }
    else
    {
        throw std::invalid_argument("Invalid Component Given: '" + component_id + '\'');
    }
    // TODO Library
    // TODO Sequencer
}

auto PluginWindow::resized() -> void
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;

    flexbox.items.add(phrases_view_accordion.get_flexitem());
    flexbox.items.add(juce::FlexItem(gui_timeline).withHeight(30.f));
    flexbox.items.add(juce::FlexItem(phrase_editor).withFlex(1.f));
    // flexbox.items.add(juce::FlexItem(tuning_box).withHeight(140.f));
    flexbox.items.add(
        juce::FlexItem(status_bar).withHeight(ModeDisplay::preferred_size));

    flexbox.performLayout(this->getLocalBounds());

    // Overlaps, so outside of flexbox
    command_bar.setBounds(0, this->getHeight() - 23 - status_bar.getHeight(),
                          getWidth(), 23);
}

} // namespace xen::gui