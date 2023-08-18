#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "heading.hpp"
#include "phrase_editor.hpp"
// #include "tuning.hpp"
#include "../command_core.hpp"
#include "../state.hpp"
#include "command_bar.hpp"

namespace xen::gui
{

class PluginWindow : public juce::Component
{
  public:
    explicit PluginWindow(XenCommandCore &command_core) : command_bar_{command_core}
    {
        this->addAndMakeVisible(&heading_);
        this->addAndMakeVisible(&phrase_editor_);
        // TODO
        // this->addAndMakeVisible(&tuning_box_);
        this->addAndMakeVisible(&command_bar_);

        heading_.set_justification(juce::Justification::centred);

        phrase_editor_.on_command_bar_request.connect(
            [this] { command_bar_.grabKeyboardFocus(); });

        command_bar_.on_escape_request.connect(
            [this] { phrase_editor_.grabKeyboardFocus(); });
    }

  public:
    auto update(State const &state, SelectedState const &) -> void
    {
        phrase_editor_.phrase.set(state.phrase, state);
        // TODO then use selected state to select the selected cell

        // TODO
        // tuning_box_.set_tuning(state.tuning);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem(heading_).withHeight(30.f));
        flexbox.items.add(juce::FlexItem(phrase_editor_).withFlex(1.f));
        // flexbox.items.add(juce::FlexItem(tuning_box_).withHeight(140.f));
        flexbox.items.add(juce::FlexItem(command_bar_).withHeight(25.f));

        flexbox.performLayout(this->getLocalBounds());
    }

  private:
    // TODO - child components
    gui::Heading heading_{"XenSequencer"};
    gui::PhraseEditor phrase_editor_;
    // gui::TuningBox tuning_box_;
    gui::CommandBar command_bar_;
};

} // namespace xen::gui