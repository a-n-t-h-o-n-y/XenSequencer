#include <xen/gui/bottom_bar.hpp>

#include <cassert>
#include <cctype>
#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/color_ids.hpp>
#include <xen/gui/fonts.hpp>
#include <xen/input_mode.hpp>

namespace
{

/**
 * Return the first letter of the mode name given.
 */
[[nodiscard]] auto get_first_letter(xen::InputMode mode) -> char
{
    auto const str = to_string(mode);
    assert(!str.empty());
    return static_cast<char>(std::toupper(str[0]));
}

} // namespace

// -------------------------------------------------------------------------------------

namespace xen::gui
{

LetterSquare::LetterSquare(char initial) : letter_{initial}
{
}

void LetterSquare::set(char letter)
{
    letter_ = letter;
    this->repaint();
}

auto LetterSquare::get() const -> char
{
    return letter_;
}

void LetterSquare::paint(juce::Graphics &g)
{
    g.fillAll(this->findColour((int)LetterSquareColorIDs::Background));
    g.setColour(this->findColour((int)LetterSquareColorIDs::Outline));
    g.drawRect(this->getLocalBounds(), 1);

    g.setColour(this->findColour((int)LetterSquareColorIDs::Letter));
    g.setFont(fonts::monospaced().bold.withHeight(18.f));
    g.drawText(juce::String(std::string(1, letter_)), this->getLocalBounds(),
               juce::Justification::centred);
}

void LetterSquare::mouseUp(juce::MouseEvent const &event)
{
    if (event.mods.isLeftButtonDown())
    {
        this->clicked();
    }
}

// -------------------------------------------------------------------------------------

InputModeIndicator::InputModeIndicator(InputMode mode)
    : LetterSquare{get_first_letter(mode)}
{
    this->lookAndFeelChanged();
}

void InputModeIndicator::set(InputMode mode)
{
    auto const first_letter = get_first_letter(mode);
    this->LetterSquare::set(first_letter);
}

void InputModeIndicator::lookAndFeelChanged()
{
    this->setColour((int)LetterSquareColorIDs::Background,
                    this->findColour((int)StatusBarColorIDs::Background));
    this->setColour((int)LetterSquareColorIDs::Outline,
                    this->findColour((int)StatusBarColorIDs::Outline));
    this->setColour((int)LetterSquareColorIDs::Letter,
                    this->findColour((int)StatusBarColorIDs::InputModeLetter));
}

// -------------------------------------------------------------------------------------

LibrarySequencerToggle::LibrarySequencerToggle(char initial) : LetterSquare{initial}
{
    this->LetterSquare::clicked.connect([this] { this->emit_show_command(); });
}

void LibrarySequencerToggle::display_library_indicator()
{
    this->LetterSquare::set('L');
}

void LibrarySequencerToggle::display_sequencer_indicator()
{
    this->LetterSquare::set('S');
}

void LibrarySequencerToggle::lookAndFeelChanged()
{
    this->setColour((int)LetterSquareColorIDs::Background,
                    this->findColour((int)StatusBarColorIDs::Background));
    this->setColour((int)LetterSquareColorIDs::Outline,
                    this->findColour((int)StatusBarColorIDs::Outline));
    this->setColour(
        (int)LetterSquareColorIDs::Letter,
        this->findColour((int)StatusBarColorIDs::LibrarySequencerToggleLetter));
}

void LibrarySequencerToggle::emit_show_command()
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

// -------------------------------------------------------------------------------------

BottomBar::BottomBar(CommandHistory &cmd_history) : command_bar{cmd_history}
{
    this->addAndMakeVisible(input_mode_indicator);
    this->addAndMakeVisible(status_bar);
    this->addChildComponent(command_bar);
    this->addAndMakeVisible(library_sequencer_toggle);
}

void BottomBar::show_status_bar()
{
    command_bar.setVisible(false);
    status_bar.setVisible(true);
    this->resized();
}

void BottomBar::show_command_bar()
{
    status_bar.setVisible(false);
    command_bar.setVisible(true);
    this->resized();
}

void BottomBar::resized()
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

auto BottomBar::current_component() -> juce::Component &
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

} // namespace xen::gui