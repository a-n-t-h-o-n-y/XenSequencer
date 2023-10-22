#pragma once

#include <stdexcept>

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
#include <xen/message_type.hpp>
#include <xen/state.hpp>
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
            [this](MessageType mtype, std::string const &response) {
                switch (mtype)
                {
                case MessageType::Error:
                    status_bar_.message_display.set_error(response);
                    break;
                case MessageType::Warning:
                    status_bar_.message_display.set_warning(response);
                    break;
                case MessageType::Success:
                    status_bar_.message_display.set_success(response);
                    break;
                default:
                    throw std::runtime_error("invalid message type");
                }
            });

        command_bar_.on_escape_request.connect(
            [this] { phrase_editor_.grabKeyboardFocus(); });

        auto slot_change_focus =
            sl::Slot<void(std::string const &)>{[this](std::string const &name) {
                if (name == "commandbar")
                {
                    command_bar_.open();
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
        on_focus_change_request.connect(slot_change_focus);
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

    auto set_key_listeners(std::map<std::string, KeyConfigListener> &listeners) -> void
    {
        // phrase_editor_.removeKeyListener(); // TODO  this fn needs ptr to previous
        phrase_editor_.addKeyListener(&listeners.at("phraseeditor"));
        listeners.at("phraseeditor")
            .on_command.connect([this](std::string const &command) {
                // TODO should this send the message to the command bar? but
                // selection etc.. shouldn't display?
                (void)execute(command_tree, timeline_,
                              normalize_command_string(command));
            });

        // TODO
        // tuning_box_.addKeyListener(listeners["tuningbox"]);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem(heading_).withHeight(heading_.getHeight()));
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
    XenTimeline &timeline_;

    gui::Heading heading_{"XenSequencer", 1, juce::Font{"Arial", "Bold", 16.f}};
    gui::Timeline gui_timeline_;
    gui::PhraseEditor phrase_editor_;
    // TODO
    // gui::TuningBox tuning_box_;
    gui::CommandBar command_bar_;
    gui::StatusBar status_bar_;

    sl::Lifetime lifetime_;
};

} // namespace xen::gui