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
#include <xen/gui/heading.hpp>
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

PluginWindow::PluginWindow(XenTimeline &tl, CommandHistory &cmd_history,
                           XenCommandTree const &command_tree)
    : heading{"XenSequencer", 1, juce::Font{"Arial", "Bold", 16.f}},
        command_bar{tl, cmd_history, command_tree}
{
    this->addAndMakeVisible(heading);
    this->addAndMakeVisible(active_sessions);
    this->addAndMakeVisible(gui_timeline);
    this->addAndMakeVisible(phrase_editor);

    // TODO
    // this->addAndMakeVisible(tuning_box);

    this->addChildComponent(command_bar);
    command_bar.setVisible(false);

    this->addAndMakeVisible(status_bar);

    command_bar.on_command_response.connect(
        [this](MessageLevel mlevel, std::string const &response) {
            status_bar.message_display.set_status(mlevel, response);
        });

    command_bar.on_escape_request.connect(
        [this] { phrase_editor.grabKeyboardFocus(); });
}

auto PluginWindow::update(State const &state, AuxState const &aux,
                          Metadata const &metadata) -> void
{
    active_sessions.update_this_instance_name(metadata.display_name);

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
    if (component_id == to_lower(command_bar.getComponentID().toStdString()))
    {
        command_bar.open();
    }
    else if (component_id == to_lower(phrase_editor.getComponentID().toStdString()))
    {
        phrase_editor.grabKeyboardFocus();
    }
    else
    {
        throw std::invalid_argument("Invalid Component Given: '" + component_id + '\'');
    }
    // TODO
    // else if (name == to_lower(tuning_box.getComponentID().toStdString()))
    // {
    //     tuning_box.grabKeyboardFocus();
    // }
}

auto PluginWindow::resized() -> void
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;

    flexbox.items.add(juce::FlexItem(heading).withHeight((float)heading.getHeight()));
    flexbox.items.add(juce::FlexItem(active_sessions).withHeight(60.f));
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