#pragma once

#include <functional>
#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <sequence/pattern.hpp>

#include <xen/command_history.hpp>

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
    CommandInputComponent();

  public:
    [[nodiscard]] auto is_cursor_at_end() const -> bool;

  public:
    auto keyPressed(juce::KeyPress const &key) -> bool override;

    void focusLost(juce::Component::FocusChangeType cause) override;
};

// -------------------------------------------------------------------------------------

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
    sl::Signal<void(sequence::Pattern const &)> on_pattern_update;

  public:
    explicit CommandBar(CommandHistory &cmd_history);

  public:
    void clear();

    void focus();

    /**
     * Closes the command bar by sending the command to show the status bar.
     */
    void close();

    /**
     * Parses any text in the command bar and returns a sequence::Pattern.
     * @details returns the default {0, {1}} pattern if the text does not contain a
     * valid pattern.
     */
    [[nodiscard]] auto extract_pattern_from_content() const -> sequence::Pattern;

  public:
    void resized() override;

    void lookAndFeelChanged() override;

  private:
    /**
     * Sends a command string to the command core and display the result.
     */
    void do_send_command();

    /**
     * Add ghost text that attempts to autocomplete the currently typed command
     * and displays info about arguments.
     */
    void add_guide_text();

    void do_tab_press();

    void do_history_next();

    void do_history_previous();

  private:
    CommandInputComponent command_input_;
    juce::TextEditor ghost_text_;

    CommandHistory &command_history_;
    sequence::Pattern last_pattern_{0, {1}};
};

} // namespace xen::gui