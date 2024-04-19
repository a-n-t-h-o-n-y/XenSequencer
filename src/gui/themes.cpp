#include <xen/gui/themes.hpp>

#include <map>
#include <memory>
#include <string_view>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/color_ids.hpp>

namespace
{

/**
 * Color value definitions for semantic color names.
 * @see https://github.com/hundredrabbits/Themes
 */
struct Theme
{
    juce::Colour background;
    juce::Colour fg_high;
    juce::Colour fg_med;
    juce::Colour fg_low;
    juce::Colour fg_inv;
    juce::Colour bg_high;
    juce::Colour bg_med;
    juce::Colour bg_low;
    juce::Colour bg_inv;
};

/**
 * Construct a LookAndFeel object from a Theme.
 */
[[nodiscard]] auto make_laf(Theme const &theme) -> std::unique_ptr<juce::LookAndFeel>
{
    using namespace xen::gui;
    auto laf = std::make_unique<juce::LookAndFeel_V4>();

    laf->setColour((int)AccordionColorIDs::Background, theme.background);
    laf->setColour((int)AccordionColorIDs::Text, theme.fg_high);
    laf->setColour((int)AccordionColorIDs::Triangle, theme.fg_low);

    laf->setColour((int)PhraseDirectoryViewColorIDs::TitleText, theme.fg_high);
    laf->setColour((int)PhraseDirectoryViewColorIDs::TitleBackground, theme.background);
    laf->setColour((int)PhraseDirectoryViewColorIDs::ItemBackground, theme.bg_med);
    laf->setColour((int)PhraseDirectoryViewColorIDs::ItemText, theme.fg_high);
    laf->setColour((int)PhraseDirectoryViewColorIDs::SelectedItemBackground,
                   theme.bg_low);
    laf->setColour((int)PhraseDirectoryViewColorIDs::SelectedItemText, theme.fg_high);

    laf->setColour((int)ActiveSessionsColorIDs::TitleText, theme.fg_high);
    laf->setColour((int)ActiveSessionsColorIDs::TitleBackground, theme.background);
    laf->setColour((int)ActiveSessionsColorIDs::ItemBackground, theme.bg_med);
    laf->setColour((int)ActiveSessionsColorIDs::ItemText, theme.fg_high);
    laf->setColour((int)ActiveSessionsColorIDs::SelectedItemBackground, theme.bg_low);
    laf->setColour((int)ActiveSessionsColorIDs::SelectedItemText, theme.fg_high);
    laf->setColour((int)ActiveSessionsColorIDs::BackgroundWhenEditing, theme.bg_low);
    laf->setColour((int)ActiveSessionsColorIDs::TextWhenEditing, theme.fg_high);
    laf->setColour((int)ActiveSessionsColorIDs::OutlineWhenEditing, theme.fg_low);
    laf->setColour((int)ActiveSessionsColorIDs::CurrentItemBackground, theme.bg_med);
    laf->setColour((int)ActiveSessionsColorIDs::CurrentItemText, theme.fg_med);

    laf->setColour((int)TimelineColorIDs::Background, theme.bg_med);
    laf->setColour((int)TimelineColorIDs::SelectionHighlight, theme.fg_med);
    laf->setColour((int)TimelineColorIDs::VerticalSeparator, theme.fg_med);
    laf->setColour((int)TimelineColorIDs::Note, theme.fg_high);
    laf->setColour((int)TimelineColorIDs::Rest, theme.fg_low);

    laf->setColour((int)TimeSignatureColorIDs::Background, theme.background);
    laf->setColour((int)TimeSignatureColorIDs::Text, theme.fg_high);

    laf->setColour((int)MeasureColorIDs::Background, theme.bg_med);
    laf->setColour((int)MeasureColorIDs::Outline, theme.fg_low);
    laf->setColour((int)MeasureColorIDs::SelectionHighlight, theme.fg_med);

    laf->setColour((int)RestColorIDs::Foreground, theme.bg_low);
    laf->setColour((int)RestColorIDs::Text, theme.fg_low);
    laf->setColour((int)RestColorIDs::Outline, theme.fg_low);

    laf->setColour((int)NoteColorIDs::Foreground, theme.bg_low);
    laf->setColour((int)NoteColorIDs::IntervalLow, theme.fg_low);
    laf->setColour((int)NoteColorIDs::IntervalMid, theme.fg_med);
    laf->setColour((int)NoteColorIDs::IntervalHigh, theme.fg_high);
    laf->setColour((int)NoteColorIDs::IntervalText, theme.bg_high);
    laf->setColour((int)NoteColorIDs::OctaveText, theme.bg_high);

    laf->setColour((int)StatusBarColorIDs::Background, theme.background);
    laf->setColour((int)StatusBarColorIDs::InfoText, theme.fg_high);
    laf->setColour((int)StatusBarColorIDs::DebugText, theme.fg_high);
    laf->setColour((int)StatusBarColorIDs::WarningText, theme.fg_med);
    laf->setColour((int)StatusBarColorIDs::ErrorText, theme.fg_med);
    laf->setColour((int)StatusBarColorIDs::ModeLetter, theme.fg_med);
    laf->setColour((int)StatusBarColorIDs::Outline, theme.fg_low);

    laf->setColour((int)CommandBarColorIDs::Background, theme.bg_med);
    laf->setColour((int)CommandBarColorIDs::Text, theme.fg_high);
    laf->setColour((int)CommandBarColorIDs::GhostText, theme.fg_low);
    laf->setColour((int)CommandBarColorIDs::Outline, theme.fg_low);

    return laf;
}

/**
 * A map of color themes.
 */
auto const THEMES = std::map<std::string_view, Theme>{
    {"apollo",
     {
         .background = juce::Colour{0xFF29272B},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFFE47464},
         .fg_low = juce::Colour{0xFF66606B},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFF000000},
         .bg_med = juce::Colour{0xFF201E21},
         .bg_low = juce::Colour{0xFF322E33},
         .bg_inv = juce::Colour{0xFFE47464},
     }},
    {"coal",
     {
         .background = juce::Colour{0xFFEDEAEA},
         .fg_high = juce::Colour{0xFF393B3F},
         .fg_med = juce::Colour{0xFF808790},
         .fg_low = juce::Colour{0xFFA3A3A4},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFF333333},
         .bg_med = juce::Colour{0xFF777777},
         .bg_low = juce::Colour{0xFFDDDDDD},
         .bg_inv = juce::Colour{0xFFFFFFFF},
     }},
    // TODO add themes
};

} // namespace

namespace xen::gui
{

auto find_theme(std::string_view name) -> std::unique_ptr<juce::LookAndFeel>
{
    try
    {
        return make_laf(THEMES.at(name));
    }
    catch (std::out_of_range const &)
    {
        throw std::runtime_error{"Theme not found: '" + std::string{name} + "'"};
    }
}

} // namespace xen::gui