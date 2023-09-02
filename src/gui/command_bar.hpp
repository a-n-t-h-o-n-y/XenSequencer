#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include "../message_type.hpp"
#include "../signature.hpp"
#include "../xen_command_core.hpp"

namespace xen::gui
{

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
        // First truncate history if needed
        if (current_index_ != history_.size())
        {
            history_.resize(++current_index_);
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

  public:
    [[nodiscard]] auto is_cursor_at_end() const -> bool
    {
        return this->getCaretPosition() == this->getText().length();
    }

  protected:
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
    sl::Signal<void(MessageType, std::string const &)> on_command_response;

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
        command_input_.onReturnKey = [this] {
            this->do_send_command();
            this->clear();
            this->close();
        };
        command_input_.onTextChange = [this] { this->do_autocomplete(); };
        command_input_.onEscapeKey = [this] { this->close(); };
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

        auto const font = juce::Font{juce::Font::getDefaultMonospacedFontName(), 14.f,
                                     juce::Font::plain};
        command_input_.setFont(font);
        ghost_text_.setFont(font);
    }

  public:
    auto clear() -> void
    {
        command_input_.setText("");
        ghost_text_.setText("");
    }

    /**
     * Opens the command bar by making it visible and grabing keyboard focus.
     */
    auto open() -> void
    {
        this->setVisible(true);
        this->grabKeyboardFocus();
        this->getParentComponent()->resized();
    }

    /**
     * Closes the command bar by making it invisible and releasing keyboard focus.
     */
    auto close() -> void
    {
        this->do_escape();
        this->setVisible(false);
        this->getParentComponent()->resized();
    }

  protected:
    auto resized() -> void override
    {
        ghost_text_.setBounds(0, 0, this->getWidth(), this->getHeight());
        command_input_.setBounds(0, 0, this->getWidth(), this->getHeight());
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
            auto const [mtype, message] = command_core_.execute_command(command);
            this->on_command_response(mtype, message);
        }
        catch (std::exception const &e)
        {
            this->on_command_response(MessageType::Error, e.what());
        }
    }

    /**
     * @brief Autocomplete the command input and draws ghosted text.
     */
    auto do_autocomplete() -> void
    {
        auto const input = command_input_.getText().toStdString();
        auto const signature = command_core_.get_matched_signature(input);
        auto const stripped_input = sequence::strip_pattern_chars(input);
        auto const pattern_str = sequence::extract_pattern_str(input);
        if (!pattern_str.empty() && pattern_str.back() != ' ')
        {
            ghost_text_.clear();
            return;
        }

        if (signature)
        {
            auto const arg_count = count_words(stripped_input) - 1;

            auto autocomplete_text = [&] {
                if (stripped_input.size() > signature->name.size())
                {
                    return input;
                }
                else
                {
                    auto name = signature->name;
                    name.replace(0, stripped_input.size(), stripped_input.size(), ' ');
                    name.insert(0, pattern_str.size(), ' ');
                    return name;
                }
            }();

            if (input.back() != ' ')
            {
                autocomplete_text += ' ';
            }
            // Add remaining untyped arguments
            for (auto i = arg_count; i < signature->arguments.size(); ++i)
            {
                autocomplete_text += signature->arguments[i] + ' ';
            }

            ghost_text_.setText(autocomplete_text,
                                juce::NotificationType::dontSendNotification);
        }
        else
        {
            ghost_text_.clear();
        }
    }

    auto do_tab_press() -> void
    {
        if (!command_input_.is_cursor_at_end())
        {
            return;
        }

        auto const input = command_input_.getText().toStdString();
        auto const signature = command_core_.get_matched_signature(input);
        auto const stripped_input = sequence::strip_pattern_chars(input);
        auto const pattern_str = sequence::extract_pattern_str(input);
        if (!pattern_str.empty() && pattern_str.back() != ' ')
        {
            return;
        }

        if (signature)
        {
            auto completed_text = std::string{};

            if (stripped_input.size() <= signature->name.size())
            {
                completed_text += pattern_str + signature->name + " ";
            }
            else
            {
                completed_text += input;

                auto const arg_count = count_words(stripped_input) - 1;
                if (stripped_input.back() == ' ' &&
                    arg_count < signature->arguments.size())
                {
                    CommandBase const *command =
                        command_core_.get_matched_command(stripped_input);

                    if (command)
                    {
                        auto const values = command->get_default_arg_strings();
                        completed_text += values[arg_count] + " ";
                    }
                }
            }

            command_input_.setText(completed_text,
                                   juce::NotificationType::dontSendNotification);
            ghost_text_.clear();
            this->do_autocomplete();
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

    /**
     * @brief Counts the number of words in a string.
     *
     * @param input_str The string to count words in.
     * @return The number of words.
     */
    [[nodiscard]] static auto count_words(std::string const &input_str) -> std::size_t
    {
        auto word_count = std::size_t{0};
        auto word_start = std::cbegin(input_str);
        auto const word_end = std::cend(input_str);

        while (word_start != word_end)
        {
            // Skip leading whitespaces
            word_start = std::find_if_not(word_start, word_end, ::isspace);

            if (word_start == word_end)
            {
                break;
            }

            // Find the end of the current word
            auto next_space = std::find_if(word_start, word_end, ::isspace);

            ++word_count;
            word_start = next_space;
        }

        return word_count;
    }

  private:
    XenCommandCore &command_core_;
    CommandInput command_input_;
    juce::TextEditor ghost_text_;
    CommandHistory command_history_;
};

} // namespace xen::gui