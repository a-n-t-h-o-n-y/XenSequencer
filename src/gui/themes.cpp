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
    laf->setColour((int)AccordionColorIDs::TitleUnderline, theme.fg_low);

    laf->setColour((int)PhraseDirectoryViewColorIDs::TitleText, theme.fg_high);
    laf->setColour((int)PhraseDirectoryViewColorIDs::TitleBackground, theme.background);
    laf->setColour((int)PhraseDirectoryViewColorIDs::ItemBackground, theme.bg_high);
    laf->setColour((int)PhraseDirectoryViewColorIDs::ItemText, theme.fg_high);
    laf->setColour((int)PhraseDirectoryViewColorIDs::SelectedItemBackground,
                   theme.bg_low);
    laf->setColour((int)PhraseDirectoryViewColorIDs::SelectedItemText, theme.fg_high);

    laf->setColour((int)ActiveSessionsColorIDs::TitleText, theme.fg_high);
    laf->setColour((int)ActiveSessionsColorIDs::TitleBackground, theme.background);
    laf->setColour((int)ActiveSessionsColorIDs::ItemBackground, theme.bg_high);
    laf->setColour((int)ActiveSessionsColorIDs::ItemText, theme.fg_high);
    laf->setColour((int)ActiveSessionsColorIDs::SelectedItemBackground, theme.bg_low);
    laf->setColour((int)ActiveSessionsColorIDs::SelectedItemText, theme.fg_high);
    laf->setColour((int)ActiveSessionsColorIDs::BackgroundWhenEditing, theme.bg_low);
    laf->setColour((int)ActiveSessionsColorIDs::TextWhenEditing, theme.fg_high);
    laf->setColour((int)ActiveSessionsColorIDs::OutlineWhenEditing, theme.fg_low);
    laf->setColour((int)ActiveSessionsColorIDs::CurrentItemBackground, theme.bg_med);
    laf->setColour((int)ActiveSessionsColorIDs::CurrentItemText, theme.fg_med);

    laf->setColour((int)TimelineColorIDs::Background, theme.bg_med);
    laf->setColour((int)TimelineColorIDs::SelectionHighlight, theme.fg_low);
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

    {"battlestation",
     {
         .background = juce::Colour{0xFF222222},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFFAFFEC7},
         .fg_low = juce::Colour{0xFF888888},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFF555555},
         .bg_med = juce::Colour{0xFF333333},
         .bg_low = juce::Colour{0xFF111111},
         .bg_inv = juce::Colour{0xFFAFFEC7},
     }},

    {"berry",
     {
         .background = juce::Colour{0xFF9EB7FF},
         .fg_high = juce::Colour{0xFF3E8281},
         .fg_med = juce::Colour{0xFFFFFFFF},
         .fg_low = juce::Colour{0xFFC5F0EC},
         .fg_inv = juce::Colour{0xFFFFFFFF},
         .bg_high = juce::Colour{0xFF1C0A16},
         .bg_med = juce::Colour{0xFF499897},
         .bg_low = juce::Colour{0xFF6ADEDC},
         .bg_inv = juce::Colour{0xFF6ADEDC},
     }},

    {"bigtime",
     {
         .background = juce::Colour{0xFF4682B4},
         .fg_high = juce::Colour{0xFF000000},
         .fg_med = juce::Colour{0xFF2F4F4F},
         .fg_low = juce::Colour{0xFFFFA500},
         .fg_inv = juce::Colour{0xFF9932CC},
         .bg_high = juce::Colour{0xFFF8F8FF},
         .bg_med = juce::Colour{0xFF696969},
         .bg_low = juce::Colour{0xFF778899},
         .bg_inv = juce::Colour{0xFF6B8E23},
     }},

    {"boysenberry",
     {
         .background = juce::Colour{0xFF171717},
         .fg_high = juce::Colour{0xFFEFEFEF},
         .fg_med = juce::Colour{0xFF999999},
         .fg_low = juce::Colour{0xFF873260},
         .fg_inv = juce::Colour{0xFF919191},
         .bg_high = juce::Colour{0xFF373737},
         .bg_med = juce::Colour{0xFF272727},
         .bg_low = juce::Colour{0xFF000000},
         .bg_inv = juce::Colour{0xFF873260},
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

    {"cobalt",
     {
         .background = juce::Colour{0xFF18364A},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFFFFC600},
         .fg_low = juce::Colour{0xFF0088FF},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFF1B1A1C},
         .bg_med = juce::Colour{0xFF204863},
         .bg_low = juce::Colour{0xFF15232D},
         .bg_inv = juce::Colour{0xFFFFFFFF},
     }},

    {"commodore",
     {
         .background = juce::Colour{0xFFA5A7FC},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFF444AE3},
         .fg_low = juce::Colour{0xFFFFD7CD},
         .fg_inv = juce::Colour{0xFF444AE3},
         .bg_high = juce::Colour{0xFFC2C4FF},
         .bg_med = juce::Colour{0xFFBABCFF},
         .bg_low = juce::Colour{0xFFB0B2FF},
         .bg_inv = juce::Colour{0xFFFFFFFF},
     }},

    {"forestlawn",
     {
         .background = juce::Colour{0xFFCD853F},
         .fg_high = juce::Colour{0xFF000000},
         .fg_med = juce::Colour{0xFF8B0000},
         .fg_low = juce::Colour{0xFF8B4513},
         .fg_inv = juce::Colour{0xFF00CED1},
         .bg_high = juce::Colour{0xFF90EE90},
         .bg_med = juce::Colour{0xFF32CD32},
         .bg_low = juce::Colour{0xFF9ACD32},
         .bg_inv = juce::Colour{0xFF000000},
     }},

    {"frameio",
     {
         .background = juce::Colour{0xFF333848},
         .fg_high = juce::Colour{0xFFCCCCCC},
         .fg_med = juce::Colour{0xFF5B52FE},
         .fg_low = juce::Colour{0xFF4C576F},
         .fg_inv = juce::Colour{0xFFFFFFFF},
         .bg_high = juce::Colour{0xFFEDEEF2},
         .bg_med = juce::Colour{0xFF262B37},
         .bg_low = juce::Colour{0xFF394153},
         .bg_inv = juce::Colour{0xFF5B52FE},
     }},

    {"gameboy",
     {
         .background = juce::Colour{0xFF9BBC0F},
         .fg_high = juce::Colour{0xFF0F380F},
         .fg_med = juce::Colour{0xFF0F380F},
         .fg_low = juce::Colour{0xFF306230},
         .fg_inv = juce::Colour{0xFF9BBC0F},
         .bg_high = juce::Colour{0xFF8BAC0F},
         .bg_med = juce::Colour{0xFF8BAC0F},
         .bg_low = juce::Colour{0xFF9BBC0F},
         .bg_inv = juce::Colour{0xFF0F380F},
     }},

    {"garden",
     {
         .background = juce::Colour{0xFF28211C},
         .fg_high = juce::Colour{0xFFFFEFC9},
         .fg_med = juce::Colour{0xFF9F9FA2},
         .fg_low = juce::Colour{0xFFA3832C},
         .fg_inv = juce::Colour{0xFF666666},
         .bg_high = juce::Colour{0xFFAA0000},
         .bg_med = juce::Colour{0xFF214C05},
         .bg_low = juce::Colour{0xFF48413A},
         .bg_inv = juce::Colour{0xFF4CB1CF},
     }},

    {"gotham",
     {
         .background = juce::Colour{0xFF0A0F14},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFF98D1CE},
         .fg_low = juce::Colour{0xFFEDB54B},
         .fg_inv = juce::Colour{0xFFC33027},
         .bg_high = juce::Colour{0xFF093748},
         .bg_med = juce::Colour{0xFF081F2D},
         .bg_low = juce::Colour{0xFF10151B},
         .bg_inv = juce::Colour{0xFF8FAF9F},
     }},

    {"haxe",
     {
         .background = juce::Colour{0xFF141419},
         .fg_high = juce::Colour{0xFFFAB20B},
         .fg_med = juce::Colour{0xFFF47216},
         .fg_low = juce::Colour{0xFFF1471D},
         .fg_inv = juce::Colour{0xFF141419},
         .bg_high = juce::Colour{0xFF141419},
         .bg_med = juce::Colour{0xFF141419},
         .bg_low = juce::Colour{0xFF141419},
         .bg_inv = juce::Colour{0xFFFFFFFF},
     }},

    {"isotope",
     {
         .background = juce::Colour{0xFF000000},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFF33FF00},
         .fg_low = juce::Colour{0xFFFF0099},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFF505050},
         .bg_med = juce::Colour{0xFF000000},
         .bg_low = juce::Colour{0xFF000000},
         .bg_inv = juce::Colour{0xFFFFFFFF},
     }},

    {"kawaii",
     {
         .background = juce::Colour{0xFFD09090},
         .fg_high = juce::Colour{0xFF000000},
         .fg_med = juce::Colour{0xFFFFFAFA},
         .fg_low = juce::Colour{0xFF6EA2A1},
         .fg_inv = juce::Colour{0xFFFF1493},
         .bg_high = juce::Colour{0xFF7FFFD4},
         .bg_med = juce::Colour{0xFF6ADEDC},
         .bg_low = juce::Colour{0xFFB08686},
         .bg_inv = juce::Colour{0xFF7FFFD4},
     }},

    {"laundry",
     {
         .background = juce::Colour{0xFF1B1A1E},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFFFF2851},
         .fg_low = juce::Colour{0xFF3E3D42},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFFBDBCC1},
         .bg_med = juce::Colour{0xFF63606B},
         .bg_low = juce::Colour{0xFF151417},
         .bg_inv = juce::Colour{0xFFFF2851},
     }},

    {"lotus",
     {
         .background = juce::Colour{0xFF161616},
         .fg_high = juce::Colour{0xFFF0C098},
         .fg_med = juce::Colour{0xFF999999},
         .fg_low = juce::Colour{0xFF444444},
         .fg_inv = juce::Colour{0xFF222222},
         .bg_high = juce::Colour{0xFFFFFFFF},
         .bg_med = juce::Colour{0xFF333333},
         .bg_low = juce::Colour{0xFF222222},
         .bg_inv = juce::Colour{0xFFF0C098},
     }},

    {"mahou",
     {
         .background = juce::Colour{0xFFE0B1CB},
         .fg_high = juce::Colour{0xFF231942},
         .fg_med = juce::Colour{0xFF48416D},
         .fg_low = juce::Colour{0xFF917296},
         .fg_inv = juce::Colour{0xFFE0B1CB},
         .bg_high = juce::Colour{0xFF5E548E},
         .bg_med = juce::Colour{0xFFFFFFFF},
         .bg_low = juce::Colour{0xFFBE95C4},
         .bg_inv = juce::Colour{0xFF9F86C0},
     }},

    {"marble",
     {
         .background = juce::Colour{0xFFFBFBF2},
         .fg_high = juce::Colour{0xFF3A3738},
         .fg_med = juce::Colour{0xFF847577},
         .fg_low = juce::Colour{0xFFBDB8B8},
         .fg_inv = juce::Colour{0xFFA6A2A2},
         .bg_high = juce::Colour{0xFF676164},
         .bg_med = juce::Colour{0xFFA6A2A2},
         .bg_low = juce::Colour{0xFFCFD2CD},
         .bg_inv = juce::Colour{0xFF676164},
     }},

    {"murata",
     {
         .background = juce::Colour{0xFF111111},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFFE8DACB},
         .fg_low = juce::Colour{0xFF5A6970},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFFBBBBBB},
         .bg_med = juce::Colour{0xFF8498A2},
         .bg_low = juce::Colour{0xFF333333},
         .bg_inv = juce::Colour{0xFFB9615A},
     }},

    {"muzieca",
     {
         .background = juce::Colour{0xFF090909},
         .fg_high = juce::Colour{0xFF818181},
         .fg_med = juce::Colour{0xFF707070},
         .fg_low = juce::Colour{0xFF595959},
         .fg_inv = juce::Colour{0xFF272727},
         .bg_high = juce::Colour{0xFF272727},
         .bg_med = juce::Colour{0xFF181818},
         .bg_low = juce::Colour{0xFF111111},
         .bg_inv = juce::Colour{0xFF818181},
     }},

    {"nightowl",
     {
         .background = juce::Colour{0xFF011627},
         .fg_high = juce::Colour{0xFF7FDBCA},
         .fg_med = juce::Colour{0xFF82AAFF},
         .fg_low = juce::Colour{0xFFC792EA},
         .fg_inv = juce::Colour{0xFF637777},
         .bg_high = juce::Colour{0xFF5F7E97},
         .bg_med = juce::Colour{0xFF456075},
         .bg_low = juce::Colour{0xFF2F4759},
         .bg_inv = juce::Colour{0xFF7FDBCA},
     }},

    {"ninetynine",
     {
         .background = juce::Colour{0xFF000000},
         .fg_high = juce::Colour{0xFFEFEFEF},
         .fg_med = juce::Colour{0xFFCDCDCD},
         .fg_low = juce::Colour{0xFF676767},
         .fg_inv = juce::Colour{0xFF0A0A0A},
         .bg_high = juce::Colour{0xFFEEEEEE},
         .bg_med = juce::Colour{0xFFFFD220},
         .bg_low = juce::Colour{0xFF464646},
         .bg_inv = juce::Colour{0xFFFF3300},
     }},

    {"noir",
     {
         .background = juce::Colour{0xFF222222},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFFCCCCCC},
         .fg_low = juce::Colour{0xFF999999},
         .fg_inv = juce::Colour{0xFFFFFFFF},
         .bg_high = juce::Colour{0xFF888888},
         .bg_med = juce::Colour{0xFF666666},
         .bg_low = juce::Colour{0xFF444444},
         .bg_inv = juce::Colour{0xFF000000},
     }},

    {"nord",
     {
         .background = juce::Colour{0xFF2E3440},
         .fg_high = juce::Colour{0xFFECEFF4},
         .fg_med = juce::Colour{0xFF9DC4C3},
         .fg_low = juce::Colour{0xFFB4B8C0},
         .fg_inv = juce::Colour{0xFF5E81AC},
         .bg_high = juce::Colour{0xFF5E81AC},
         .bg_med = juce::Colour{0xFF434C5E},
         .bg_low = juce::Colour{0xFF3B4252},
         .bg_inv = juce::Colour{0xFFABCDCC},
     }},

    {"obsidian",
     {
         .background = juce::Colour{0xFF22282A},
         .fg_high = juce::Colour{0xFFF1F2F3},
         .fg_med = juce::Colour{0xFF93C763},
         .fg_low = juce::Colour{0xFFEC7600},
         .fg_inv = juce::Colour{0xFF963A46},
         .bg_high = juce::Colour{0xFF678CB1},
         .bg_med = juce::Colour{0xFF4F6164},
         .bg_low = juce::Colour{0xFF42464C},
         .bg_inv = juce::Colour{0xFFFFCD22},
     }},

    {"op-1",
     {
         .background = juce::Colour{0xFF0E0D11},
         .fg_high = juce::Colour{0xFFEFEFEF},
         .fg_med = juce::Colour{0xFF26936F},
         .fg_low = juce::Colour{0xFFA5435A},
         .fg_inv = juce::Colour{0xFF0E0D11},
         .bg_high = juce::Colour{0xFF191A26},
         .bg_med = juce::Colour{0xFF14151F},
         .bg_low = juce::Colour{0xFF101119},
         .bg_inv = juce::Colour{0xFF9F9FB3},
     }},

    {"orca",
     {
         .background = juce::Colour{0xFF000000},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFF777777},
         .fg_low = juce::Colour{0xFF444444},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFFDDDDDD},
         .bg_med = juce::Colour{0xFF72DEC2},
         .bg_low = juce::Colour{0xFF222222},
         .bg_inv = juce::Colour{0xFFFFB545},
     }},

    {"pawbin",
     {
         .background = juce::Colour{0xFF2B2933},
         .fg_high = juce::Colour{0xFFF2F2F2},
         .fg_med = juce::Colour{0xFF00BDD6},
         .fg_low = juce::Colour{0xFFAA9FDF},
         .fg_inv = juce::Colour{0xFF1A1820},
         .bg_high = juce::Colour{0xFF1A1820},
         .bg_med = juce::Colour{0xFF24212C},
         .bg_low = juce::Colour{0xFF34303B},
         .bg_inv = juce::Colour{0xFFF2F2F2},
     }},

    {"pico8",
     {
         .background = juce::Colour{0xFF000000},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFFFFF1E8},
         .fg_low = juce::Colour{0xFFFF78A9},
         .fg_inv = juce::Colour{0xFFFFFFFF},
         .bg_high = juce::Colour{0xFFC2C3C7},
         .bg_med = juce::Colour{0xFF83769C},
         .bg_low = juce::Colour{0xFF695F56},
         .bg_inv = juce::Colour{0xFF00AEFE},
     }},

    {"polivoks",
     {
         .background = juce::Colour{0xFF111111},
         .fg_high = juce::Colour{0xFFEFEFEF},
         .fg_med = juce::Colour{0xFFFF4444},
         .fg_low = juce::Colour{0xFF333333},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFF666666},
         .bg_med = juce::Colour{0xFF444444},
         .bg_low = juce::Colour{0xFF222222},
         .bg_inv = juce::Colour{0xFFFF4444},
     }},

    {"rainonwires",
     {
         .background = juce::Colour{0xFF010101},
         .fg_high = juce::Colour{0xFFC692BB},
         .fg_med = juce::Colour{0xFF149106},
         .fg_low = juce::Colour{0xFF8A6682},
         .fg_inv = juce::Colour{0xFF8D2E71},
         .bg_high = juce::Colour{0xFF8D2E71},
         .bg_med = juce::Colour{0xFF6E2455},
         .bg_low = juce::Colour{0xFF010101},
         .bg_inv = juce::Colour{0xFF159106},
     }},

    {"roguelight",
     {
         .background = juce::Colour{0xFF352B31},
         .fg_high = juce::Colour{0xFFF5F5D4},
         .fg_med = juce::Colour{0xFF70838C},
         .fg_low = juce::Colour{0xFF4A6B83},
         .fg_inv = juce::Colour{0xFF352B31},
         .bg_high = juce::Colour{0xFF96CF85},
         .bg_med = juce::Colour{0xFF5A6970},
         .bg_low = juce::Colour{0xFF4A3B44},
         .bg_inv = juce::Colour{0xFFF5F5D4},
     }},

    {"sk",
     {
         .background = juce::Colour{0xFF000709},
         .fg_high = juce::Colour{0xFFCBCBD3},
         .fg_med = juce::Colour{0xFF897668},
         .fg_low = juce::Colour{0xFF523D2C},
         .fg_inv = juce::Colour{0xFF3F4F5B},
         .bg_high = juce::Colour{0xFFABA49E},
         .bg_med = juce::Colour{0xFF59574B},
         .bg_low = juce::Colour{0xFF372823},
         .bg_inv = juce::Colour{0xFF8C5A3D},
     }},

    {"snow",
     {
         .background = juce::Colour{0xFFEEEFEE},
         .fg_high = juce::Colour{0xFF222222},
         .fg_med = juce::Colour{0xFF999999},
         .fg_low = juce::Colour{0xFFBBBCBB},
         .fg_inv = juce::Colour{0xFF545454},
         .bg_high = juce::Colour{0xFF545454},
         .bg_med = juce::Colour{0xFFCED0CE},
         .bg_low = juce::Colour{0xFFF5F5F5},
         .bg_inv = juce::Colour{0xFFED2C3E},
     }},

    {"solarised.dark",
     {
         .background = juce::Colour{0xFF073642},
         .fg_high = juce::Colour{0xFF93A1A1},
         .fg_med = juce::Colour{0xFF6C71C4},
         .fg_low = juce::Colour{0xFF586E75},
         .fg_inv = juce::Colour{0xFF002B36},
         .bg_high = juce::Colour{0xFFFDF6E3},
         .bg_med = juce::Colour{0xFFEEE8D5},
         .bg_low = juce::Colour{0xFF002B36},
         .bg_inv = juce::Colour{0xFFCB4B16},
     }},

    {"solarised.light",
     {
         .background = juce::Colour{0xFFEEE8D5},
         .fg_high = juce::Colour{0xFF586E75},
         .fg_med = juce::Colour{0xFF6C71C4},
         .fg_low = juce::Colour{0xFF93A1A1},
         .fg_inv = juce::Colour{0xFFFDF6E3},
         .bg_high = juce::Colour{0xFF002B36},
         .bg_med = juce::Colour{0xFF073642},
         .bg_low = juce::Colour{0xFFFDF6E3},
         .bg_inv = juce::Colour{0xFFCB4B16},
     }},

    {"sonicpi",
     {
         .background = juce::Colour{0xFFFFFFFF},
         .fg_high = juce::Colour{0xFF000000},
         .fg_med = juce::Colour{0xFFED1E92},
         .fg_low = juce::Colour{0xFFAAAAAA},
         .fg_inv = juce::Colour{0xFFFFFFFF},
         .bg_high = juce::Colour{0xFF444444},
         .bg_med = juce::Colour{0xFF555555},
         .bg_low = juce::Colour{0xFFCED0CE},
         .bg_inv = juce::Colour{0xFFED1E92},
     }},

    {"soyuz",
     {
         .background = juce::Colour{0xFF111111},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFFAAAAAA},
         .fg_low = juce::Colour{0xFF555555},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFFFC533E},
         .bg_med = juce::Colour{0xFF666666},
         .bg_low = juce::Colour{0xFF333333},
         .bg_inv = juce::Colour{0xFFFC533E},
     }},

    {"tape",
     {
         .background = juce::Colour{0xFFDAD7CD},
         .fg_high = juce::Colour{0xFF696861},
         .fg_med = juce::Colour{0xFFFFFFFF},
         .fg_low = juce::Colour{0xFFB3B2AC},
         .fg_inv = juce::Colour{0xFF43423E},
         .bg_high = juce::Colour{0xFF43423E},
         .bg_med = juce::Colour{0xFFC2C1BB},
         .bg_low = juce::Colour{0xFFE5E3DC},
         .bg_inv = juce::Colour{0xFFEB3F48},
     }},

    {"teenage",
     {
         .background = juce::Colour{0xFFA1A1A1},
         .fg_high = juce::Colour{0xFF222222},
         .fg_med = juce::Colour{0xFFE00B30},
         .fg_low = juce::Colour{0xFF888888},
         .fg_inv = juce::Colour{0xFFFFFFFF},
         .bg_high = juce::Colour{0xFF555555},
         .bg_med = juce::Colour{0xFFFBBA2D},
         .bg_low = juce::Colour{0xFFB3B3B3},
         .bg_inv = juce::Colour{0xFF0E7242},
     }},

    {"teletext",
     {
         .background = juce::Colour{0xFF000000},
         .fg_high = juce::Colour{0xFFFFFFFF},
         .fg_med = juce::Colour{0xFFFFFF00},
         .fg_low = juce::Colour{0xFF00FF00},
         .fg_inv = juce::Colour{0xFF000000},
         .bg_high = juce::Colour{0xFFFF00FF},
         .bg_med = juce::Colour{0xFFFF0000},
         .bg_low = juce::Colour{0xFF0000FF},
         .bg_inv = juce::Colour{0xFFFFFFFF},
     }},

    {"vacuui",
     {
         .background = juce::Colour{0xFF22282A},
         .fg_high = juce::Colour{0xFFF1F2F3},
         .fg_med = juce::Colour{0xFFA6E22E},
         .fg_low = juce::Colour{0xFF66D9EF},
         .fg_inv = juce::Colour{0xFFF92672},
         .bg_high = juce::Colour{0xFF678CB1},
         .bg_med = juce::Colour{0xFF4F6164},
         .bg_low = juce::Colour{0xFF42464C},
         .bg_inv = juce::Colour{0xFFE6DB74},
     }},

    {"zenburn",
     {
         .background = juce::Colour{0xFF464646},
         .fg_high = juce::Colour{0xFFDCDCCC},
         .fg_med = juce::Colour{0xFFDCA3A3},
         .fg_low = juce::Colour{0xFF7F9F7F},
         .fg_inv = juce::Colour{0xFF000D18},
         .bg_high = juce::Colour{0xFF262626},
         .bg_med = juce::Colour{0xFF333333},
         .bg_low = juce::Colour{0xFF3F3F3F},
         .bg_inv = juce::Colour{0xFF8FAF9F},
     }},
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