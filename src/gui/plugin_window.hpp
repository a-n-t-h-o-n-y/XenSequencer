#pragma once

#include <iostream> //temp
#include <stdexcept>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <signals_light/signal.hpp>

#include "../command_core.hpp"
#include "../key_core.hpp"
#include "../state.hpp"
#include "command_bar.hpp"
#include "heading.hpp"
#include "phrase_editor.hpp"
// #include "tuning.hpp"

namespace xen::gui
{

class PluginWindow : public juce::Component
{
  public:
    explicit PluginWindow(XenCommandCore &command_core)
        : command_bar_{command_core}, command_core_{command_core}
    {
        this->addAndMakeVisible(&heading_);
        this->addAndMakeVisible(&phrase_editor_);
        // TODO
        // this->addAndMakeVisible(&tuning_box_);
        this->addAndMakeVisible(&command_bar_);

        heading_.set_justification(juce::Justification::centred);

        // phrase_editor_.on_command_bar_request.connect(
        //     [this] { command_bar_.grabKeyboardFocus(); });

        command_bar_.on_escape_request.connect(
            [this] { phrase_editor_.grabKeyboardFocus(); });

        phrase_editor_.on_command.connect([this](std::string const &command) {
            // TODO should this send the message to the command bar? but selection etc..
            // shouldn't display?
            std::cerr << command_core_.execute_command(command) << std::endl;
        });

        auto slot_change_focus =
            sl::Slot<void(std::string const &)>{[this](std::string const &name) {
                if (name == "commandbar")
                {
                    command_bar_.grabKeyboardFocus();
                }
                else if (name == "phraseeditor")
                {
                    phrase_editor_.grabKeyboardFocus();
                }
                else if (name == "tuningbox")
                {
                    // TODO
                    // tuning_box_.grabKeyboardFocus();
                }
                else
                {
                    throw std::runtime_error("invalid focus change request");
                }
            }};
        slot_change_focus.track(lifetime_);
        command_core_.on_focus_change_request.connect(slot_change_focus);
    }

  public:
    auto update(State const &state, AuxState const &aux) -> void
    {
        phrase_editor_.phrase.set(state, aux.selected);
        phrase_editor_.phrase.select(aux.selected);

        // TODO
        // tuning_box_.set_tuning(state.tuning);
    }

    auto set_key_listeners(std::map<std::string, KeyConfigListener> &listeners) -> void
    {
        phrase_editor_.addKeyListener(&listeners.at("phraseeditor"));
        listeners.at("phraseeditor")
            .on_command.connect([this](std::string const &command) {
                // TODO should this send the message to the command bar? but
                // selection etc.. shouldn't display?
                std::cerr << command_core_.execute_command(command) << std::endl;
            });

        // TODO
        // tuning_box_.addKeyListener(listeners["tuningbox"]);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem(heading_).withHeight(30.f));
        flexbox.items.add(juce::FlexItem(phrase_editor_).withFlex(1.f));
        // flexbox.items.add(juce::FlexItem(tuning_box_).withHeight(140.f));
        flexbox.items.add(juce::FlexItem(command_bar_).withHeight(23.f));

        flexbox.performLayout(this->getLocalBounds());
    }

  private:
    gui::Heading heading_{"XenSequencer"};
    gui::PhraseEditor phrase_editor_;
    // TODO
    // gui::TuningBox tuning_box_;
    gui::CommandBar command_bar_;
    XenCommandCore &command_core_;
    sl::Lifetime lifetime_;
};

} // namespace xen::gui