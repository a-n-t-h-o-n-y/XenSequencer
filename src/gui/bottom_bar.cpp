#include <xen/gui/bottom_bar.hpp>

#include <cassert>
#include <cctype>
#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>
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

LetterSquare::LetterSquare(std::string display, int margin)
    : ClickableTile{display, margin}
{
    Tile::background_color_id = ColorID::Background;
    Tile::text_color_id = ColorID::ForegroundMedium;
}

void LetterSquare::paint(juce::Graphics &g)
{
    this->Tile::paint(g);

    // Border
    g.setColour(this->findColour(ColorID::ForegroundLow));
    g.drawRect(this->getLocalBounds(), 1);
}
// -------------------------------------------------------------------------------------

InputModeIndicator::InputModeIndicator(InputMode mode)
    : LetterSquare{std::string(1, get_first_letter(mode)), 2}
{
}

void InputModeIndicator::set(InputMode mode)
{
    auto const first_letter = get_first_letter(mode);
    this->LetterSquare::set(std::string(1, first_letter));
}

// -------------------------------------------------------------------------------------

LibrarySequencerToggle::LibrarySequencerToggle(char initial)
    : LetterSquare{std::string(1, initial), 2}
{
    this->LetterSquare::clicked.connect([this] { this->emit_show_command(); });
}

void LibrarySequencerToggle::display_library_indicator()
{
    this->LetterSquare::set("L");
}

void LibrarySequencerToggle::display_sequencer_indicator()
{
    this->LetterSquare::set("S");
}

void LibrarySequencerToggle::emit_show_command()
{
    auto const display = this->LetterSquare::get();
    if (display == "L")
    {
        this->on_command("show LibraryView;focus SequencesList");
    }
    else if (display == "S")
    {
        this->on_command("show SequenceView;focus SequenceView");
    }
    else
    {
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