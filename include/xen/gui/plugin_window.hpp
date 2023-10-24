#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <signals_light/signal.hpp>

#include <xen/command.hpp>
#include <xen/command_history.hpp>
#include <xen/gui/command_bar.hpp>
#include <xen/gui/heading.hpp>
#include <xen/gui/phrase_editor.hpp>
#include <xen/gui/status_bar.hpp>
#include <xen/gui/timeline.hpp>
#include <xen/key_core.hpp>
#include <xen/message_level.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/xen_command_tree.hpp>
// #include <xen/tuning.hpp>

namespace xen::gui
{

class PluginWindow : public juce::Component
{
  public:
    explicit PluginWindow(XenTimeline &tl, CommandHistory &cmd_history)
        : timeline_{tl}, command_bar_{tl, cmd_history}
    {
        this->addAndMakeVisible(heading_);
        this->addAndMakeVisible(gui_timeline_);
        this->addAndMakeVisible(phrase_editor_);
        // TODO
        // this->addAndMakeVisible(tuning_box_);
        this->addChildComponent(command_bar_);
        command_bar_.setVisible(false);
        this->addAndMakeVisible(status_bar_);

        heading_.set_justification(juce::Justification::centred);

        command_bar_.on_command_response.connect(
            [this](MessageLevel mlevel, std::string const &response) {
                status_bar_.message_display.set_status(mlevel, response);
            });

        command_bar_.on_escape_request.connect(
            [this] { phrase_editor_.grabKeyboardFocus(); });

        auto slot_change_focus =
            sl::Slot<void(std::string const &)>{[this](std::string const &name) {
                if (name == to_lower(command_bar_.getComponentID().toStdString()))
                {
                    command_bar_.open();
                }
                else if (name ==
                         to_lower(phrase_editor_.getComponentID().toStdString()))
                {
                    phrase_editor_.grabKeyboardFocus();
                }
                // TODO
                // else if (name ==
                // to_lower(tuning_box_.getComponentID().toStdString()))
                // {
                //     tuning_box_.grabKeyboardFocus();
                // }
                else
                {
                    throw std::runtime_error("Invalid focus change request: '" + name +
                                             "'.");
                }
            }};
        slot_change_focus.track(lifetime_);
        on_focus_change_request.connect(slot_change_focus);

        try
        {
            this->update_key_listeners(get_default_keys_file(), get_user_keys_file());
        }
        catch (std::exception const &e)
        {
            status_bar_.message_display.set_error(
                std::string{"Check `user_keys.yml`: "} + e.what());
        }
    }

  public:
    auto update(State const &state, AuxState const &aux) -> void
    {
        phrase_editor_.phrase.set(state, aux.selected);
        phrase_editor_.phrase.select(aux.selected);

        status_bar_.mode_display.set(aux.input_mode);

        gui_timeline_.set(state.phrase, aux.selected);

        // TODO
        // tuning_box_.set_tuning(state.tuning);
    }

    auto set_key_listeners(std::map<std::string, KeyConfigListener> previous_listeners,
                           std::map<std::string, KeyConfigListener> &new_listeners)
        -> void
    {
        // This relies on Component::getComponentID();
        auto const remove_listener = [&previous_listeners](juce::Component &component) {
            auto const id = to_lower(component.getComponentID().toStdString());
            auto const iter = previous_listeners.find(id);
            if (iter != std::cend(previous_listeners))
            {
                component.removeKeyListener(&(iter->second));
            }
        };

        auto const add_listener = [&new_listeners, this](juce::Component &component) {
            auto const id = to_lower(component.getComponentID().toStdString());
            component.addKeyListener(&new_listeners.at(id));
            new_listeners.at(id).on_command.connect([this](std::string const &command) {
                auto const [mlevel, msg] =
                    execute(command_tree, timeline_, normalize_command_string(command));
                status_bar_.message_display.set_status(mlevel, msg);
            });
        };

        try
        {
            remove_listener(phrase_editor_);
            add_listener(phrase_editor_);

            // TODO
            // remove_listener(tuning_box_);
            // add_listener(tuning_box_);
        }
        catch (std::exception const &e)
        {
            throw std::runtime_error("Failed to set key listeners: " +
                                     std::string{e.what()});
        }
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(
            juce::FlexItem(heading_).withHeight((float)heading_.getHeight()));
        flexbox.items.add(juce::FlexItem(gui_timeline_).withHeight(30.f));
        flexbox.items.add(juce::FlexItem(phrase_editor_).withFlex(1.f));
        // flexbox.items.add(juce::FlexItem(tuning_box_).withHeight(140.f));
        flexbox.items.add(
            juce::FlexItem(status_bar_).withHeight(ModeDisplay::preferred_size));

        flexbox.performLayout(this->getLocalBounds());

        // Overlaps, so outside of flexbox
        command_bar_.setBounds(0, this->getHeight() - 23 - status_bar_.getHeight(),
                               getWidth(), 23);
    }

  private:
    auto update_key_listeners(std::filesystem::path const &default_keys,
                              std::filesystem::path const &user_keys) -> void
    {
        auto previous_listeners = std::move(key_config_listeners_);
        key_config_listeners_ = build_key_listeners(default_keys, user_keys, timeline_);
        this->set_key_listeners(std::move(previous_listeners), key_config_listeners_);
    }

  private:
    XenTimeline &timeline_;

    gui::Heading heading_{"XenSequencer", 1, juce::Font{"Arial", "Bold", 16.f}};
    gui::Timeline gui_timeline_;
    gui::PhraseEditor phrase_editor_;
    // TODO
    // gui::TuningBox tuning_box_;
    gui::CommandBar command_bar_;
    gui::StatusBar status_bar_;

    sl::Lifetime lifetime_;

    std::map<std::string, KeyConfigListener> key_config_listeners_;
};

} // namespace xen::gui