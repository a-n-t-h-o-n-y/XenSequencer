#pragma once

#include <cassert>
#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/command_history.hpp>
#include <xen/gui/command_bar.hpp>
#include <xen/gui/status_bar.hpp>

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

    enum ColourIDs
    {
        Background = 0xB000010,
        Outline = 0xB000011,
        Letter = 0xB000012,
    };

    /**
     * Emitted on Left Mouse Button Up.
     */
    sl::Signal<void()> clicked;

  public:
    /**
     * Constructs a LetterSquare with a specific initial letter.
     */
    explicit LetterSquare(char initial) : letter_{initial}
    {
    }

  public:
    /**
     * Sets the letter to display.
     */
    auto set(char letter) -> void
    {
        letter_ = letter;
        this->repaint();
    }

    /**
     * Returns the current letter.
     */
    [[nodiscard]] auto get() const -> char
    {
        return letter_;
    }

  public:
    auto paint(juce::Graphics &g) -> void override
    {
        g.fillAll(this->findColour(LetterSquare::Background));
        g.setColour(this->findColour(LetterSquare::Outline));
        g.drawRect(this->getLocalBounds(), 1);

        g.setColour(this->findColour(LetterSquare::Letter));
        g.setFont(juce::Font{juce::Font::getDefaultMonospacedFontName(), 16.f,
                             juce::Font::bold});
        g.drawText(juce::String(std::string(1, letter_)), this->getLocalBounds(),
                   juce::Justification::centred);
    }

    auto mouseUp(juce::MouseEvent const &event) -> void override
    {
        if (event.mods.isLeftButtonDown())
        {
            this->clicked();
        }
    }

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
    explicit InputModeIndicator(InputMode mode) : LetterSquare{get_first_letter(mode)}
    {
        this->lookAndFeelChanged();
    }

  public:
    auto set(InputMode mode) -> void
    {
        auto const first_letter = get_first_letter(mode);
        this->LetterSquare::set(first_letter);
    }

  protected:
    auto lookAndFeelChanged() -> void override
    {
        this->setColour(LetterSquare::Background,
                        this->findColour((int)StatusBarColorIDs::Background));
        this->setColour(LetterSquare::Outline,
                        this->findColour((int)StatusBarColorIDs::Outline));
        this->setColour(LetterSquare::Letter,
                        this->findColour((int)StatusBarColorIDs::InputModeLetter));
    }

  private:
    [[nodiscard]] static auto get_first_letter(InputMode mode) -> char
    {
        auto const str = to_string(mode);
        assert(!str.empty());
        return static_cast<char>(std::toupper(str[0]));
    }
};

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
    LibrarySequencerToggle(char initial) : LetterSquare{initial}
    {
        this->LetterSquare::clicked.connect([this] { this->emit_show_command(); });
    }

  public:
    auto display_library_indicator() -> void
    {
        this->LetterSquare::set('L');
    }

    auto display_sequencer_indicator() -> void
    {
        this->LetterSquare::set('S');
    }

  protected:
    auto lookAndFeelChanged() -> void override
    {
        this->setColour(LetterSquare::Background,
                        this->findColour((int)StatusBarColorIDs::Background));
        this->setColour(LetterSquare::Outline,
                        this->findColour((int)StatusBarColorIDs::Outline));
        this->setColour(
            LetterSquare::Letter,
            this->findColour((int)StatusBarColorIDs::LibrarySequencerToggleLetter));
    }

  private:
    /**
     * Emits the command to toggle the display.
     *
     * @details This is called on left mouse click up. It will emit the command to
     * toggle the display between the Library and Sequencer but will not update its own
     * display, that will be done by the display_...() functions called by
     * PluginWindow::show();
     */
    auto emit_show_command() -> void
    {
        switch (this->LetterSquare::get())
        {
        case 'L':
            this->on_command("show LibraryView;focus SequencesList");
            break;
        case 'S':
            this->on_command("show SequenceView;focus SequenceView");
            break;
        default:
            assert(false);
        }
    }
};

// -------------------------------------------------------------------------------------

class BottomBar : public juce::Component
{
  public:
    explicit BottomBar(CommandHistory &cmd_history) : command_bar{cmd_history}
    {
        this->addAndMakeVisible(input_mode_indicator);
        this->addAndMakeVisible(status_bar);
        this->addChildComponent(command_bar);
        this->addAndMakeVisible(library_sequencer_toggle);
    }

  public:
    auto show_status_bar() -> void
    {
        command_bar.setVisible(false);
        status_bar.setVisible(true);
        this->resized();
    }

    auto show_command_bar() -> void
    {
        status_bar.setVisible(false);
        command_bar.setVisible(true);
        this->resized();
    }

  public:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::row;

        flexbox.items.add(juce::FlexItem{input_mode_indicator}.withWidth(
            InputModeIndicator::preferred_size));
        flexbox.items.add(juce::FlexItem{this->current_component()}.withFlex(1.f));
        flexbox.items.add(juce::FlexItem{library_sequencer_toggle}.withWidth(
            LibrarySequencerToggle::preferred_size));

        flexbox.performLayout(this->getLocalBounds());
    }

  private:
    [[nodiscard]] auto current_component() -> juce::Component &
    {
        if (status_bar.isVisible())
        {
            return status_bar;
        }
        else if (command_bar.isVisible())
        {
            return command_bar;
        }
        else
        {
            assert(false);
        }
    }

  public:
    InputModeIndicator input_mode_indicator{InputMode::Movement};
    StatusBar status_bar;
    CommandBar command_bar;
    LibrarySequencerToggle library_sequencer_toggle{'L'};
};

} // namespace xen::gui