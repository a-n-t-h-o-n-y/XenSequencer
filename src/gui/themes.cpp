#include <xen/gui/themes.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/color_ids.hpp>
#include <xen/gui/fonts.hpp>

namespace
{

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
  public:
    void drawCornerResizer(juce::Graphics &, int, int, bool, bool) override
    {
        // Left blank on purpose.
    }

    void drawButtonText(juce::Graphics &g, juce::TextButton &button, bool,
                        bool) override
    {
        g.setFont(xen::gui::fonts::monospaced().bold.withHeight(20.f));
        g.setColour(button.findColour(juce::TextButton::textColourOffId));
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                   juce::Justification::centred, true);
    }
};

using xen::gui::Theme;

/**
 * An array of color themes, sorted by name.
 */
constexpr auto THEMES = [] {
    auto themes = std::array<std::pair<std::string_view, Theme>, 55>{{
        {"apollo",
         {
             .background = 0xFF29272B,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFFE47464,
             .fg_low = 0xFF66606B,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFF000000,
             .bg_med = 0xFF201E21,
             .bg_low = 0xFF322E33,
             .bg_inv = 0xFFE47464,
         }},

        {"battlestation",
         {
             .background = 0xFF222222,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFFAFFEC7,
             .fg_low = 0xFF888888,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFF555555,
             .bg_med = 0xFF333333,
             .bg_low = 0xFF111111,
             .bg_inv = 0xFFAFFEC7,
         }},

        {"berry",
         {
             .background = 0xFF9EB7FF,
             .fg_high = 0xFF3E8281,
             .fg_med = 0xFFFFFFFF,
             .fg_low = 0xFFC5F0EC,
             .fg_inv = 0xFFFFFFFF,
             .bg_high = 0xFF1C0A16,
             .bg_med = 0xFF499897,
             .bg_low = 0xFF6ADEDC,
             .bg_inv = 0xFF6ADEDC,
         }},

        {"bigtime",
         {
             .background = 0xFF4682B4,
             .fg_high = 0xFF000000,
             .fg_med = 0xFF2F4F4F,
             .fg_low = 0xFFFFA500,
             .fg_inv = 0xFF9932CC,
             .bg_high = 0xFFF8F8FF,
             .bg_med = 0xFF696969,
             .bg_low = 0xFF778899,
             .bg_inv = 0xFF6B8E23,
         }},

        {"boysenberry",
         {
             .background = 0xFF171717,
             .fg_high = 0xFFEFEFEF,
             .fg_med = 0xFF999999,
             .fg_low = 0xFF873260,
             .fg_inv = 0xFF919191,
             .bg_high = 0xFF373737,
             .bg_med = 0xFF272727,
             .bg_low = 0xFF000000,
             .bg_inv = 0xFF873260,
         }},

        {"coal",
         {
             .background = 0xFFEDEAEA,
             .fg_high = 0xFF393B3F,
             .fg_med = 0xFF808790,
             .fg_low = 0xFFA3A3A4,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFF333333,
             .bg_med = 0xFF777777,
             .bg_low = 0xFFDDDDDD,
             .bg_inv = 0xFFFFFFFF,
         }},

        {"cobalt",
         {
             .background = 0xFF18364A,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFFFFC600,
             .fg_low = 0xFF0088FF,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFF1B1A1C,
             .bg_med = 0xFF204863,
             .bg_low = 0xFF15232D,
             .bg_inv = 0xFFFFFFFF,
         }},

        {"commodore",
         {
             .background = 0xFFA5A7FC,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFF444AE3,
             .fg_low = 0xFFFFD7CD,
             .fg_inv = 0xFF444AE3,
             .bg_high = 0xFFC2C4FF,
             .bg_med = 0xFFBABCFF,
             .bg_low = 0xFFB0B2FF,
             .bg_inv = 0xFFFFFFFF,
         }},

        {"forestlawn",
         {
             .background = 0xFFCD853F,
             .fg_high = 0xFF000000,
             .fg_med = 0xFF8B0000,
             .fg_low = 0xFF8B4513,
             .fg_inv = 0xFF00CED1,
             .bg_high = 0xFF90EE90,
             .bg_med = 0xFF32CD32,
             .bg_low = 0xFF9ACD32,
             .bg_inv = 0xFF000000,
         }},

        {"frameio",
         {
             .background = 0xFF333848,
             .fg_high = 0xFFCCCCCC,
             .fg_med = 0xFF5B52FE,
             .fg_low = 0xFF4C576F,
             .fg_inv = 0xFFFFFFFF,
             .bg_high = 0xFFEDEEF2,
             .bg_med = 0xFF262B37,
             .bg_low = 0xFF394153,
             .bg_inv = 0xFF5B52FE,
         }},

        {"gameboy",
         {
             .background = 0xFF9BBC0F,
             .fg_high = 0xFF0F380F,
             .fg_med = 0xFF0F380F,
             .fg_low = 0xFF306230,
             .fg_inv = 0xFF9BBC0F,
             .bg_high = 0xFF8BAC0F,
             .bg_med = 0xFF8BAC0F,
             .bg_low = 0xFF9BBC0F,
             .bg_inv = 0xFF0F380F,
         }},

        {"garden",
         {
             .background = 0xFF28211C,
             .fg_high = 0xFFFFEFC9,
             .fg_med = 0xFF9F9FA2,
             .fg_low = 0xFFA3832C,
             .fg_inv = 0xFF666666,
             .bg_high = 0xFFAA0000,
             .bg_med = 0xFF214C05,
             .bg_low = 0xFF48413A,
             .bg_inv = 0xFF4CB1CF,
         }},

        {"gotham",
         {
             .background = 0xFF0A0F14,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFF98D1CE,
             .fg_low = 0xFFEDB54B,
             .fg_inv = 0xFFC33027,
             .bg_high = 0xFF093748,
             .bg_med = 0xFF081F2D,
             .bg_low = 0xFF10151B,
             .bg_inv = 0xFF8FAF9F,
         }},

        {"haxe",
         {
             .background = 0xFF141419,
             .fg_high = 0xFFFAB20B,
             .fg_med = 0xFFF47216,
             .fg_low = 0xFFF1471D,
             .fg_inv = 0xFF141419,
             .bg_high = 0xFF141419,
             .bg_med = 0xFF141419,
             .bg_low = 0xFF141419,
             .bg_inv = 0xFFFFFFFF,
         }},

        {"isotope",
         {
             .background = 0xFF000000,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFF33FF00,
             .fg_low = 0xFFFF0099,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFF505050,
             .bg_med = 0xFF000000,
             .bg_low = 0xFF000000,
             .bg_inv = 0xFFFFFFFF,
         }},

        {"kawaii",
         {
             .background = 0xFFD09090,
             .fg_high = 0xFF000000,
             .fg_med = 0xFFFFFAFA,
             .fg_low = 0xFF6EA2A1,
             .fg_inv = 0xFFFF1493,
             .bg_high = 0xFF7FFFD4,
             .bg_med = 0xFF6ADEDC,
             .bg_low = 0xFFB08686,
             .bg_inv = 0xFF7FFFD4,
         }},

        {"laundry",
         {
             .background = 0xFF1B1A1E,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFFFF2851,
             .fg_low = 0xFF3E3D42,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFFBDBCC1,
             .bg_med = 0xFF63606B,
             .bg_low = 0xFF151417,
             .bg_inv = 0xFFFF2851,
         }},

        {"lotus",
         {
             .background = 0xFF161616,
             .fg_high = 0xFFF0C098,
             .fg_med = 0xFF999999,
             .fg_low = 0xFF444444,
             .fg_inv = 0xFF222222,
             .bg_high = 0xFFFFFFFF,
             .bg_med = 0xFF333333,
             .bg_low = 0xFF222222,
             .bg_inv = 0xFFF0C098,
         }},

        {"mahou",
         {
             .background = 0xFFE0B1CB,
             .fg_high = 0xFF231942,
             .fg_med = 0xFF48416D,
             .fg_low = 0xFF917296,
             .fg_inv = 0xFFE0B1CB,
             .bg_high = 0xFF5E548E,
             .bg_med = 0xFFFFFFFF,
             .bg_low = 0xFFBE95C4,
             .bg_inv = 0xFF9F86C0,
         }},

        {"marble",
         {
             .background = 0xFFFBFBF2,
             .fg_high = 0xFF3A3738,
             .fg_med = 0xFF847577,
             .fg_low = 0xFFBDB8B8,
             .fg_inv = 0xFFA6A2A2,
             .bg_high = 0xFF676164,
             .bg_med = 0xFFA6A2A2,
             .bg_low = 0xFFCFD2CD,
             .bg_inv = 0xFF676164,
         }},

        {"murata",
         {
             .background = 0xFF111111,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFFE8DACB,
             .fg_low = 0xFF5A6970,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFFBBBBBB,
             .bg_med = 0xFF8498A2,
             .bg_low = 0xFF333333,
             .bg_inv = 0xFFB9615A,
         }},

        {"muzieca",
         {
             .background = 0xFF090909,
             .fg_high = 0xFF818181,
             .fg_med = 0xFF707070,
             .fg_low = 0xFF595959,
             .fg_inv = 0xFF272727,
             .bg_high = 0xFF272727,
             .bg_med = 0xFF181818,
             .bg_low = 0xFF111111,
             .bg_inv = 0xFF818181,
         }},

        {"nightowl",
         {
             .background = 0xFF011627,
             .fg_high = 0xFF7FDBCA,
             .fg_med = 0xFF82AAFF,
             .fg_low = 0xFFC792EA,
             .fg_inv = 0xFF637777,
             .bg_high = 0xFF5F7E97,
             .bg_med = 0xFF456075,
             .bg_low = 0xFF2F4759,
             .bg_inv = 0xFF7FDBCA,
         }},

        {"ninetynine",
         {
             .background = 0xFF000000,
             .fg_high = 0xFFEFEFEF,
             .fg_med = 0xFFCDCDCD,
             .fg_low = 0xFF676767,
             .fg_inv = 0xFF0A0A0A,
             .bg_high = 0xFFEEEEEE,
             .bg_med = 0xFFFFD220,
             .bg_low = 0xFF464646,
             .bg_inv = 0xFFFF3300,
         }},

        {"noir",
         {
             .background = 0xFF222222,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFFCCCCCC,
             .fg_low = 0xFF999999,
             .fg_inv = 0xFFFFFFFF,
             .bg_high = 0xFF888888,
             .bg_med = 0xFF666666,
             .bg_low = 0xFF444444,
             .bg_inv = 0xFF000000,
         }},

        {"nord",
         {
             .background = 0xFF2E3440,
             .fg_high = 0xFFECEFF4,
             .fg_med = 0xFF9DC4C3,
             .fg_low = 0xFFB4B8C0,
             .fg_inv = 0xFF5E81AC,
             .bg_high = 0xFF5E81AC,
             .bg_med = 0xFF434C5E,
             .bg_low = 0xFF3B4252,
             .bg_inv = 0xFFABCDCC,
         }},

        {"obsidian",
         {
             .background = 0xFF22282A,
             .fg_high = 0xFFF1F2F3,
             .fg_med = 0xFF93C763,
             .fg_low = 0xFFEC7600,
             .fg_inv = 0xFF963A46,
             .bg_high = 0xFF678CB1,
             .bg_med = 0xFF4F6164,
             .bg_low = 0xFF42464C,
             .bg_inv = 0xFFFFCD22,
         }},

        {"op-1",
         {
             .background = 0xFF0E0D11,
             .fg_high = 0xFFEFEFEF,
             .fg_med = 0xFF26936F,
             .fg_low = 0xFFA5435A,
             .fg_inv = 0xFF0E0D11,
             .bg_high = 0xFF191A26,
             .bg_med = 0xFF14151F,
             .bg_low = 0xFF101119,
             .bg_inv = 0xFF9F9FB3,
         }},

        {"orca",
         {
             .background = 0xFF000000,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFF777777,
             .fg_low = 0xFF444444,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFFDDDDDD,
             .bg_med = 0xFF72DEC2,
             .bg_low = 0xFF222222,
             .bg_inv = 0xFFFFB545,
         }},

        {"pawbin",
         {
             .background = 0xFF2B2933,
             .fg_high = 0xFFF2F2F2,
             .fg_med = 0xFF00BDD6,
             .fg_low = 0xFFAA9FDF,
             .fg_inv = 0xFF1A1820,
             .bg_high = 0xFF1A1820,
             .bg_med = 0xFF24212C,
             .bg_low = 0xFF34303B,
             .bg_inv = 0xFFF2F2F2,
         }},

        {"pico8",
         {
             .background = 0xFF000000,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFFFFF1E8,
             .fg_low = 0xFFFF78A9,
             .fg_inv = 0xFFFFFFFF,
             .bg_high = 0xFFC2C3C7,
             .bg_med = 0xFF83769C,
             .bg_low = 0xFF695F56,
             .bg_inv = 0xFF00AEFE,
         }},

        {"polivoks",
         {
             .background = 0xFF111111,
             .fg_high = 0xFFEFEFEF,
             .fg_med = 0xFFFF4444,
             .fg_low = 0xFF333333,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFF666666,
             .bg_med = 0xFF444444,
             .bg_low = 0xFF222222,
             .bg_inv = 0xFFFF4444,
         }},

        {"rainonwires",
         {
             .background = 0xFF010101,
             .fg_high = 0xFFC692BB,
             .fg_med = 0xFF149106,
             .fg_low = 0xFF8A6682,
             .fg_inv = 0xFF8D2E71,
             .bg_high = 0xFF8D2E71,
             .bg_med = 0xFF6E2455,
             .bg_low = 0xFF010101,
             .bg_inv = 0xFF159106,
         }},

        {"roguelight",
         {
             .background = 0xFF352B31,
             .fg_high = 0xFFF5F5D4,
             .fg_med = 0xFF70838C,
             .fg_low = 0xFF4A6B83,
             .fg_inv = 0xFF352B31,
             .bg_high = 0xFF96CF85,
             .bg_med = 0xFF5A6970,
             .bg_low = 0xFF4A3B44,
             .bg_inv = 0xFFF5F5D4,
         }},

        {"sk",
         {
             .background = 0xFF000709,
             .fg_high = 0xFFCBCBD3,
             .fg_med = 0xFF897668,
             .fg_low = 0xFF523D2C,
             .fg_inv = 0xFF3F4F5B,
             .bg_high = 0xFFABA49E,
             .bg_med = 0xFF59574B,
             .bg_low = 0xFF372823,
             .bg_inv = 0xFF8C5A3D,
         }},

        {"snow",
         {
             .background = 0xFFEEEFEE,
             .fg_high = 0xFF222222,
             .fg_med = 0xFF999999,
             .fg_low = 0xFFBBBCBB,
             .fg_inv = 0xFF545454,
             .bg_high = 0xFF545454,
             .bg_med = 0xFFCED0CE,
             .bg_low = 0xFFF5F5F5,
             .bg_inv = 0xFFED2C3E,
         }},

        {"solarised.dark",
         {
             .background = 0xFF073642,
             .fg_high = 0xFF93A1A1,
             .fg_med = 0xFF6C71C4,
             .fg_low = 0xFF586E75,
             .fg_inv = 0xFF002B36,
             .bg_high = 0xFFFDF6E3,
             .bg_med = 0xFFEEE8D5,
             .bg_low = 0xFF002B36,
             .bg_inv = 0xFFCB4B16,
         }},

        {"solarised.light",
         {
             .background = 0xFFEEE8D5,
             .fg_high = 0xFF586E75,
             .fg_med = 0xFF6C71C4,
             .fg_low = 0xFF93A1A1,
             .fg_inv = 0xFFFDF6E3,
             .bg_high = 0xFF002B36,
             .bg_med = 0xFF073642,
             .bg_low = 0xFFFDF6E3,
             .bg_inv = 0xFFCB4B16,
         }},

        {"sonicpi",
         {
             .background = 0xFFFFFFFF,
             .fg_high = 0xFF000000,
             .fg_med = 0xFFED1E92,
             .fg_low = 0xFFAAAAAA,
             .fg_inv = 0xFFFFFFFF,
             .bg_high = 0xFF444444,
             .bg_med = 0xFF555555,
             .bg_low = 0xFFCED0CE,
             .bg_inv = 0xFFED1E92,
         }},

        {"soyuz",
         {
             .background = 0xFF111111,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFFAAAAAA,
             .fg_low = 0xFF555555,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFFFC533E,
             .bg_med = 0xFF666666,
             .bg_low = 0xFF333333,
             .bg_inv = 0xFFFC533E,
         }},

        {"tape",
         {
             .background = 0xFFDAD7CD,
             .fg_high = 0xFF696861,
             .fg_med = 0xFFFFFFFF,
             .fg_low = 0xFFB3B2AC,
             .fg_inv = 0xFF43423E,
             .bg_high = 0xFF43423E,
             .bg_med = 0xFFC2C1BB,
             .bg_low = 0xFFE5E3DC,
             .bg_inv = 0xFFEB3F48,
         }},

        {"teenage",
         {
             .background = 0xFFA1A1A1,
             .fg_high = 0xFF222222,
             .fg_med = 0xFFE00B30,
             .fg_low = 0xFF888888,
             .fg_inv = 0xFFFFFFFF,
             .bg_high = 0xFF555555,
             .bg_med = 0xFFFBBA2D,
             .bg_low = 0xFFB3B3B3,
             .bg_inv = 0xFF0E7242,
         }},

        {"teletext",
         {
             .background = 0xFF000000,
             .fg_high = 0xFFFFFFFF,
             .fg_med = 0xFFFFFF00,
             .fg_low = 0xFF00FF00,
             .fg_inv = 0xFF000000,
             .bg_high = 0xFFFF00FF,
             .bg_med = 0xFFFF0000,
             .bg_low = 0xFF0000FF,
             .bg_inv = 0xFFFFFFFF,
         }},

        {"vacuui",
         {
             .background = 0xFF22282A,
             .fg_high = 0xFFF1F2F3,
             .fg_med = 0xFFA6E22E,
             .fg_low = 0xFF66D9EF,
             .fg_inv = 0xFFF92672,
             .bg_high = 0xFF678CB1,
             .bg_med = 0xFF4F6164,
             .bg_low = 0xFF42464C,
             .bg_inv = 0xFFE6DB74,
         }},

        {"zenburn",
         {
             .background = 0xFF464646,
             .fg_high = 0xFFDCDCCC,
             .fg_med = 0xFFDCA3A3,
             .fg_low = 0xFF7F9F7F,
             .fg_inv = 0xFF000D18,
             .bg_high = 0xFF262626,
             .bg_med = 0xFF333333,
             .bg_low = 0xFF3F3F3F,
             .bg_inv = 0xFF8FAF9F,
         }},
    }};

    std::sort(themes.begin(), themes.end(),
              [](auto const &a, auto const &b) { return a.first < b.first; });

    return themes;
}();

} // namespace

namespace xen::gui
{

auto find_theme(std::string_view name) -> Theme
{
    auto const it = std::ranges::lower_bound(
        THEMES, name, std::less{}, &std::pair<std::string_view, Theme>::first);

    if (it != THEMES.cend() && it->first == name)
    {
        return it->second;
    }

    throw std::runtime_error{"Theme not found: '" + std::string{name} + "'"};
}

auto make_laf(Theme const &theme) -> std::unique_ptr<juce::LookAndFeel>
{
    auto laf = std::make_unique<CustomLookAndFeel>();

    auto sc = [&](auto id, std::uint32_t argb) {
        laf->setColour((int)id, juce::Colour{argb});
    };

    sc(AccordionColorIDs::Background, theme.background);
    sc(AccordionColorIDs::Text, theme.fg_high);
    sc(AccordionColorIDs::Triangle, theme.fg_low);
    sc(AccordionColorIDs::TitleUnderline, theme.fg_low);

    sc(DirectoryViewColorIDs::TitleText, theme.fg_high);
    sc(DirectoryViewColorIDs::TitleBackground, theme.background);
    sc(DirectoryViewColorIDs::ItemBackground, theme.bg_med);
    sc(DirectoryViewColorIDs::ItemText, theme.fg_high);
    sc(DirectoryViewColorIDs::SelectedItemBackground, theme.bg_low);
    sc(DirectoryViewColorIDs::SelectedItemText, theme.fg_high);

    sc(ActiveSessionsColorIDs::TitleText, theme.fg_high);
    sc(ActiveSessionsColorIDs::TitleBackground, theme.background);
    sc(ActiveSessionsColorIDs::ItemBackground, theme.bg_med);
    sc(ActiveSessionsColorIDs::ItemText, theme.fg_high);
    sc(ActiveSessionsColorIDs::SelectedItemBackground, theme.bg_low);
    sc(ActiveSessionsColorIDs::SelectedItemText, theme.fg_high);
    sc(ActiveSessionsColorIDs::BackgroundWhenEditing, theme.bg_low);
    sc(ActiveSessionsColorIDs::TextWhenEditing, theme.fg_high);
    sc(ActiveSessionsColorIDs::OutlineWhenEditing, theme.fg_low);
    sc(ActiveSessionsColorIDs::CurrentItemBackground, theme.bg_med);
    sc(ActiveSessionsColorIDs::CurrentItemText, theme.fg_med);

    sc(TimelineColorIDs::Background, theme.bg_med);
    sc(TimelineColorIDs::SelectionHighlight, theme.fg_low);
    sc(TimelineColorIDs::VerticalSeparator, theme.fg_med);
    sc(TimelineColorIDs::Note, theme.fg_high);
    sc(TimelineColorIDs::Rest, theme.fg_low);

    sc(TimeSignatureColorIDs::Background, theme.background);
    sc(TimeSignatureColorIDs::Text, theme.fg_high);
    sc(TimeSignatureColorIDs::Outline, theme.fg_low);

    sc(MeasureColorIDs::Background, theme.bg_med);
    sc(MeasureColorIDs::Outline, theme.fg_low);
    sc(MeasureColorIDs::SelectionHighlight, theme.fg_med);

    sc(RestColorIDs::Background, theme.bg_low);
    sc(RestColorIDs::Text, theme.fg_low);
    sc(RestColorIDs::Outline, theme.fg_low);

    sc(NoteColorIDs::Foreground, theme.bg_low);
    sc(NoteColorIDs::IntervalLow, theme.fg_low);
    sc(NoteColorIDs::IntervalMid, theme.fg_med);
    sc(NoteColorIDs::IntervalHigh, theme.fg_high);
    sc(NoteColorIDs::IntervalText, theme.bg_high);
    sc(NoteColorIDs::OctaveText, theme.bg_high);

    sc(StatusBarColorIDs::Background, theme.background);
    sc(StatusBarColorIDs::InfoText, theme.fg_high);
    sc(StatusBarColorIDs::DebugText, theme.fg_high);
    sc(StatusBarColorIDs::WarningText, theme.fg_med);
    sc(StatusBarColorIDs::ErrorText, theme.fg_med);
    sc(StatusBarColorIDs::InputModeLetter, theme.fg_med);
    sc(StatusBarColorIDs::LibrarySequencerToggleLetter, theme.fg_med);
    sc(StatusBarColorIDs::Outline, theme.fg_low);

    sc(CommandBarColorIDs::Background, theme.bg_med);
    sc(CommandBarColorIDs::Text, theme.fg_high);
    sc(CommandBarColorIDs::GhostText, theme.fg_low);
    sc(CommandBarColorIDs::Outline, theme.fg_low);

    return laf;
}

} // namespace xen::gui