#pragma once

#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/command_history.hpp>
#include <xen/gui/command_bar.hpp>
#include <xen/gui/status_bar.hpp>
#include <xen/input_mode.hpp>

namespace xen::gui
{

/**
 * A square that displays a single letter.
 *
 * @details Use `preferred_size` to set the size of the square.
 */
class LetterSquare : public juce::Component
{
  public:
    static constexpr auto preferred_size = 23.f;

    /**
     * Emitted on Left Mouse Button Up.
     */
    sl::Signal<void()> clicked;

  public:
    /**
     * Constructs a LetterSquare with a specific initial letter.
     */
    explicit LetterSquare(char initial);

  public:
    /**
     * Sets the letter to display.
     */
    void set(char letter);

    /**
     * Returns the current letter.
     */
    [[nodiscard]] auto get() const -> char;

  public:
    void paint(juce::Graphics &g) override;

    void mouseUp(juce::MouseEvent const &event) override;

  private:
    char letter_;
};

// -------------------------------------------------------------------------------------

/**
 * Displays a single letter representing the current InputMode.
 */
class InputModeIndicator : public LetterSquare
{
  public:
    /**
     * Constructs a InputModeIndicator with a specific InputMode.
     *
     * @param input_mode The mode for which to display.
     */
    explicit InputModeIndicator(InputMode mode);

  public:
    void set(InputMode mode);

  protected:
    void lookAndFeelChanged() override;
};

// -------------------------------------------------------------------------------------

/**
 * Display whether the [L]ibrary or [S]equencer is visible in the CenterComponent.
 *
 * @details This updates the display to whatever state is passed in via its update_ui()
 * fn. It will also send the command to toggle the display when clicked.
 */
class LibrarySequencerToggle : public LetterSquare
{
  public:
    sl::Signal<void(std::string const &)> on_command;

  public:
    LibrarySequencerToggle(char initial);

  public:
    void display_library_indicator();

    void display_sequencer_indicator();

  protected:
    void lookAndFeelChanged() override;

  private:
    /**
     * Emits the command to toggle the display.
     *
     * @details This is called on left mouse click up. It will emit the command to
     * toggle the display between the Library and Sequencer but will not update its own
     * display, that will be done by the display_...() functions called by
     * PluginWindow::show();
     */
    void emit_show_command();
};

// -------------------------------------------------------------------------------------

class BottomBar : public juce::Component
{
  public:
    explicit BottomBar(CommandHistory &cmd_history);

  public:
    void show_status_bar();

    void show_command_bar();

  public:
    void resized() override;

  private:
    [[nodiscard]] auto current_component() -> juce::Component &;

  public:
    InputModeIndicator input_mode_indicator{InputMode::Movement};
    StatusBar status_bar;
    CommandBar command_bar;
    LibrarySequencerToggle library_sequencer_toggle{'L'};
};

} // namespace xen::gui