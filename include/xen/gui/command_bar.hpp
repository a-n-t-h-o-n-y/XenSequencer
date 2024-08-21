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

#include <xen/command_history.hpp>
#include <xen/gui/color_ids.hpp>
#include <xen/gui/fonts.hpp>
#include <xen/message_level.hpp>
#include <xen/signature.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>

namespace xen::gui
{

/**
 * Provides signals not provided by TextEditor
 */
class CommandInputComponent : public juce::TextEditor
{
  public:
    std::function<bool()> onTabKey;
    std::function<bool()> onArrowUpKey;
    std::function<bool()> onArrowDownKey;

    std::function<void()> focus_lost;

  public:
    CommandInputComponent()
    {
        this->setWantsKeyboardFocus(true);
        this->setMultiLine(false, false);
        this->setReturnKeyStartsNewLine(false);
        this->setEscapeAndReturnKeysConsumed(true);
    }

  public:
    [[nodiscard]] auto is_cursor_at_end() const -> bool
    {
        return this->getCaretPosition() == this->getText().length();
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

    auto focusLost(juce::Component::FocusChangeType cause) -> void override
    {
        if (cause != juce::Component::FocusChangeType::focusChangedDirectly &&
            focus_lost)
        {
            this->focus_lost();
        }
    }
};

/**
 * JUCE component that provides an interactive command bar for sending commands
 * to the command core.
 */
class CommandBar : public juce::Component
{
  public:
    sl::Signal<void(std::string const &)> on_command;
    sl::Signal<std::string(std::string const &)> on_guide_text_request;
    sl::Signal<std::string(std::string const &)> on_complete_id_request;

  public:
    explicit CommandBar(CommandHistory &cmd_history) : command_history_{cmd_history}
    {
        this->setComponentID("CommandBar");
        this->setWantsKeyboardFocus(false);

        this->addAndMakeVisible(command_input_);
        this->addAndMakeVisible(ghost_text_);

        ghost_text_.setMultiLine(false, false);
        ghost_text_.setReadOnly(true);
        ghost_text_.setEnabled(false);
        ghost_text_.setInterceptsMouseClicks(false, false);
        ghost_text_.setWantsKeyboardFocus(false);
        ghost_text_.setOpaque(false);

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

        command_input_.focus_lost = [this] { this->on_command("show StatusBar"); };

        auto const font = fonts::monospaced().regular.withHeight(16.f);
        command_input_.setFont(font);
        ghost_text_.setFont(font);

        this->lookAndFeelChanged();
    }

  public:
    auto clear() -> void
    {
        command_input_.setText("");
        ghost_text_.setText("");
    }

    auto focus() -> void
    {
        command_input_.grabKeyboardFocus();
    }

    /**
     * Closes the command bar by sending the command to show the status bar.
     */
    auto close() -> void
    {
        this->clear();
        this->on_command("show StatusBar;focus SequenceView");
    }

  public:
    auto resized() -> void override
    {
        ghost_text_.setBounds(0, 0, this->getWidth(), this->getHeight());
        command_input_.setBounds(0, 0, this->getWidth(), this->getHeight());
    }

    auto lookAndFeelChanged() -> void override
    {
        auto const bg = this->findColour((int)CommandBarColorIDs::Background);
        auto const text = this->findColour((int)CommandBarColorIDs::Text);
        auto const ghost = this->findColour((int)CommandBarColorIDs::GhostText);
        auto const outline = this->findColour((int)CommandBarColorIDs::Outline);

        command_input_.setColour(juce::TextEditor::backgroundColourId, bg);
        command_input_.setColour(juce::TextEditor::textColourId, text);
        command_input_.setColour(juce::TextEditor::focusedOutlineColourId, outline);
        command_input_.setColour(juce::TextEditor::outlineColourId, outline);

        ghost_text_.setColour(juce::TextEditor::textColourId, ghost);
        ghost_text_.setColour(juce::TextEditor::backgroundColourId,
                              juce::Colours::transparentWhite);
    }

  private:
    /**
     * Sends a command string to the command core and display the result.
     */
    auto do_send_command() -> void
    {
        auto const command = command_input_.getText().toStdString();
        command_history_.add_command(command);
        this->on_command(command);
    }

    /**
     * Add ghost text that attempts to autocomplete the currently typed command
     * and displays info about arguments.
     */
    auto add_guide_text() -> void
    {
        auto const input = command_input_.getText().toStdString();

        auto const gt = this->on_guide_text_request(input);
        if (gt.has_value())
        {
            ghost_text_.setText(std::string(input.size(), ' ') + *gt,
                                juce::NotificationType::dontSendNotification);
        }
    }

    auto do_tab_press() -> void
    {
        if (!command_input_.is_cursor_at_end())
        {
            return;
        }

        auto const input = command_input_.getText().toStdString();
        auto const id = this->on_complete_id_request(input);
        // auto const completed_id = complete_id(command_tree_, input);
        if (id.has_value())
        {
            auto const completed_text = input + *id + (id->empty() ? "" : " ");
            command_input_.setText(completed_text,
                                   juce::NotificationType::dontSendNotification);
            ghost_text_.clear();
            this->add_guide_text();
        }
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
     * Counts the number of words in a string.
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
    CommandInputComponent command_input_;
    juce::TextEditor ghost_text_;

    CommandHistory &command_history_;
};

} // namespace xen::gui
