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

#include <xen/command.hpp>
#include <xen/command_history.hpp>
#include <xen/guide_text.hpp>
#include <xen/message_level.hpp>
#include <xen/signature.hpp>
#include <xen/string_manip.hpp>
#include <xen/xen_command_tree.hpp>
#include <xen/xen_timeline.hpp>

namespace xen::gui
{

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
    sl::Signal<void(MessageLevel, std::string const &)> on_command_response;

  public:
    CommandBar(XenTimeline &tl, CommandHistory &cmd_history,
               xen::XenCommandTree &command_tree)
        : timeline_{tl}, command_history_{cmd_history}, command_tree_{command_tree}
    {
        this->setComponentID("CommandBar");
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
        command_input_.onTextChange = [this] { this->add_guide_text(); };
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
    }

    /**
     * Closes the command bar by making it invisible and releasing keyboard focus.
     */
    auto close() -> void
    {
        this->do_escape();
        this->setVisible(false);
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
        auto const [mlevel, message] =
            execute(command_tree_, timeline_, normalize_command_string(command));
        this->on_command_response(mlevel, message);
    }

    /**
     * @brief Add ghost text that attempts to autocomplete the currently typed command
     * and displays info about arguments.
     */
    auto add_guide_text() -> void
    {
        auto const input = command_input_.getText().toStdString();

        auto const guide_text =
            std::string(input.size(), ' ') + generate_guide_text(command_tree_, input);

        ghost_text_.setText(guide_text, juce::NotificationType::dontSendNotification);
    }

    auto do_tab_press() -> void
    {
        if (!command_input_.is_cursor_at_end())
        {
            return;
        }

        auto const input = command_input_.getText().toStdString();
        auto const completed_id = complete_id(command_tree_, input);
        auto const completed_text =
            input + completed_id + (completed_id.empty() ? "" : " ");
        command_input_.setText(completed_text,
                               juce::NotificationType::dontSendNotification);
        ghost_text_.clear();
        this->add_guide_text();
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
    XenTimeline &timeline_;
    CommandInput command_input_;
    juce::TextEditor ghost_text_;
    CommandHistory &command_history_;
    xen::XenCommandTree command_tree_;
};

} // namespace xen::gui
