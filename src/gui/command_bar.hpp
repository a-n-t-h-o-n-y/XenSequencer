#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../command_core.hpp"

namespace xen::gui
{

// up/down to cycle through history
// esc to remove focus and typed command
// ghost text autocomplete
// tab to autocomplete

// not sure about display of multiline messages... like 'help'

class CommandBar : public juce::Component
{
  public:
    explicit CommandBar(CommandCore &command_core) : command_core_{command_core}
    {
        this->addAndMakeVisible(command_input_);
        this->setSize(400, 25);

        addAndMakeVisible(ghost_text_);
        ghost_text_.setMultiLine(false, false);
        ghost_text_.setReadOnly(true);
        ghost_text_.setEnabled(false);
        ghost_text_.setColour(juce::TextEditor::textColourId, juce::Colours::grey);
        ghost_text_.setInterceptsMouseClicks(false, false);

        addAndMakeVisible(command_input_);
        command_input_.setMultiLine(false, false);
        command_input_.setReturnKeyStartsNewLine(false);
        command_input_.setEscapeAndReturnKeysConsumed(true);
        command_input_.setOpaque(false); // Make the command input transparent
        command_input_.setColour(
            juce::TextEditor::backgroundColourId,
            juce::Colours::transparentWhite); // Make the background transparent
        command_input_.setColour(juce::TextEditor::textColourId,
                                 juce::Colours::black); // or any other color
        command_input_.onReturnKey = [this] { this->do_send_command(); };
        command_input_.onTextChange = [this] { this->do_autocomplete(); };
    }

  protected:
    auto resized() -> void override
    {
        command_input_.setBounds(this->getLocalBounds());
        ghost_text_.setBounds(this->getLocalBounds());
    }

  private:
    /**
     * @brief Sends a command string to the command core and display the result.
     * @param command The command string to send.
     */
    auto do_send_command() -> void
    {
        auto const command = command_input_.getText().toStdString();
        try
        {
            auto const result = command_core_.execute_command(command);
            command_input_.setText(result);
        }
        catch (std::exception const &e)
        {
            command_input_.setText(e.what());
        }
    }

    /**
     * @brief Autocomplete the command input and draws ghosted text.
     */
    auto do_autocomplete() -> void
    {
        auto const input = command_input_.getText().toStdString();
        auto const completion = command_core_.match_command(input);

        if (completion)
        {
            auto const autoCompleteText = input + completion->substr(input.size());
            ghost_text_.setText(autoCompleteText,
                                juce::NotificationType::dontSendNotification);
        }
        else
        {
            ghost_text_.clear();
        }
    }

  private:
    CommandCore &command_core_;
    juce::TextEditor command_input_;
    juce::TextEditor ghost_text_;
};

} // namespace xen::gui