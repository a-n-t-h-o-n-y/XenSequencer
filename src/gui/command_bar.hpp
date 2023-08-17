#pragma once

#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include "../command_core.hpp"

namespace xen::gui
{

// TODO the nullopt isn't because there is no history,
// there is a default implicit item, which is the current
// buffer of text, and when it is empty it is nullopt and
// when the index == size it is nullopt, which is the same
// when it is empty, so that is a nice way to check.
// so return nullopt when index == size and allow redo to increment
// up to size, which is an invalid index,

/**
 * @brief A class that stores a history of commands.
 *
 * This has the concept of a 'current' command, which has no state
 * stored here and returns std::nullopt when retrieved. As commands are added
 * the current command is one past the just added command.
 */
class CommandHistory
{
  public:
    /// Constructor that initializes an empty history.
    CommandHistory() : history_{}, current_index_(0)
    {
    }

  public:
    /**
     * @brief Adds a command to the history and erases all items from the current
     * index to the end.
     *
     * If the new command is a duplicate of the last, it is ignored.
     *
     * @param command The command to add to the history.
     * @return None.
     */
    auto add_command(std::string const &command) -> void
    {
        if (current_index_ != history_.size())
        {
            history_.resize(current_index_);
        }

        if (history_.empty() || command != history_.back())
        {
            history_.push_back(command);
            ++current_index_;
        }
    }

    /**
     * @brief Returns the previous command and sets the current command to it.
     *
     * @return The previous command string if available; std::nullopt if at the
     * 'current' position.
     */
    [[nodiscard]] auto previous() -> std::optional<std::string>
    {
        if (history_.empty())
        {
            return std::nullopt;
        }

        if (current_index_ != 0)
        {
            --current_index_;
        }

        return history_[current_index_];
    }

    /**
     * @brief Returns the next command and sets the current command to it.
     *
     * @return The next command string if available; std::nullopt if at the
     * 'current' position.
     */
    [[nodiscard]] auto next() -> std::optional<std::string>
    {
        if (current_index_ < history_.size())
        {
            ++current_index_;
        }

        if (current_index_ == history_.size())
        {
            return std::nullopt;
        }

        return history_[current_index_];
    }

    /**
     * @brief Returns the current command.
     *
     * @return The current command string if available; std::nullopt if at the
     * 'current' position.
     */
    [[nodiscard]] auto get_command() const -> std::optional<std::string>
    {
        if (current_index_ == history_.size())
        {
            return std::nullopt;
        }

        return history_[current_index_];
    }

  private:
    std::vector<std::string> history_;
    std::size_t current_index_;
};

/**
 * @brief Provides signals not provided by TextEditor
 */
class CommandInput : public juce::TextEditor
{
  public:
    std::function<bool()> onTabKey;
    std::function<bool()> onArrowUpKey;
    std::function<bool()> onArrowDownKey;

  public:
    CommandInput()
    {
        this->setWantsKeyboardFocus(true);
        this->setMultiLine(false, false);
        this->setReturnKeyStartsNewLine(false);
        this->setEscapeAndReturnKeysConsumed(true);
        this->setOpaque(false);
        this->setColour(juce::TextEditor::backgroundColourId,
                        juce::Colours::transparentWhite);
        this->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    }

  protected:
    auto focusGained(FocusChangeType) -> void override
    {
        this->setText("");
    }

  public:
    auto keyPressed(juce::KeyPress const &key) -> bool override
    {
        if (key == juce::KeyPress::tabKey && onTabKey)
        {
            return onTabKey();
        }
        else if (key == juce::KeyPress::upKey && onArrowUpKey)
        {
            return onArrowUpKey();
        }
        else if (key == juce::KeyPress::downKey && onArrowDownKey)
        {
            return onArrowDownKey();
        }

        return juce::TextEditor::keyPressed(key);
    }
};

/**
 * @brief JUCE component that provides an interactive command bar for sending commands
 * to the command core.
 */
class CommandBar : public juce::Component
{
  public:
    sl::Signal<void()> on_escape_request;

  public:
    explicit CommandBar(XenCommandCore &command_core)
        : command_core_{command_core}, command_history_{}
    {
        this->setWantsKeyboardFocus(true);

        this->addAndMakeVisible(ghost_text_);
        ghost_text_.setMultiLine(false, false);
        ghost_text_.setReadOnly(true);
        ghost_text_.setEnabled(false);
        ghost_text_.setColour(juce::TextEditor::textColourId, juce::Colours::grey);
        ghost_text_.setInterceptsMouseClicks(false, false);
        ghost_text_.setWantsKeyboardFocus(false);

        this->addAndMakeVisible(command_input_);
        command_input_.onReturnKey = [this] { this->do_send_command(); };
        command_input_.onTextChange = [this] { this->do_autocomplete(); };
        command_input_.onEscapeKey = [this] {
            command_input_.setText("");
            this->do_escape();
        };
        command_input_.onTabKey = [this] {
            this->do_tab_press();
            return true;
        };
        command_input_.onArrowDownKey = [this] {
            this->do_history_next();
            return true;
        };
        command_input_.onArrowUpKey = [this] {
            this->do_history_previous();
            return true;
        };
    }

  protected:
    auto resized() -> void override
    {
        ghost_text_.setBounds(0, 0, getWidth(), getHeight());
        command_input_.setBounds(0, 0, getWidth(), getHeight());
    }

    auto focusGained(FocusChangeType) -> void override
    {
        // Forward focus to child component
        command_input_.grabKeyboardFocus();
    }

  private:
    /**
     * @brief Sends a command string to the command core and display the result.
     * @param command The command string to send.
     */
    auto do_send_command() -> void
    {
        auto const command = command_input_.getText().toStdString();
        command_history_.add_command(command);
        try
        {
            auto const result = command_core_.execute_command(command);
            command_input_.setText(result);
        }
        catch (std::exception const &e)
        {
            command_input_.setText(std::string{"Exception! "} + e.what());
        }
        this->do_escape();
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

    auto do_tab_press() -> void
    {
        auto const input = command_input_.getText().toStdString();
        auto const completion = command_core_.match_command(input);

        if (completion)
        {
            auto const autoCompleteText = input + completion->substr(input.size());
            command_input_.setText(autoCompleteText,
                                   juce::NotificationType::dontSendNotification);
            ghost_text_.clear();
        }
    }

    auto do_escape() -> void
    {
        on_escape_request();
    }

    auto do_history_next() -> void
    {
        auto const cmd = command_history_.next();
        if (cmd)
        {
            command_input_.setText(*cmd);
        }
        else
        {
            // TODO use saved typing buffer
            command_input_.setText("");
        }
    }

    auto do_history_previous() -> void
    {
        auto const cmd = command_history_.previous();
        if (cmd)
        {
            command_input_.setText(*cmd);
        }
        else
        {
            // TODO use saved typing buffer
            command_input_.setText("");
        }
    }

  private:
    XenCommandCore &command_core_;
    CommandInput command_input_;
    juce::TextEditor ghost_text_;
    CommandHistory command_history_;
};

} // namespace xen::gui