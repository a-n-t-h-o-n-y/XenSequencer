#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <embed_fonts.hpp>

// TODO search and replace
// version.setFont({
//     juce::Font::getDefaultMonospacedFontName(),
//     16.f,
//     juce::Font::plain,
// });

namespace xen::gui::fonts
{

[[nodiscard]] inline auto source_code_pro() -> auto const &
{
    static const struct
    {
        juce::Font extra_light;
        juce::Font light;
        juce::Font regular;
        juce::Font medium;
        juce::Font semi_bold;
        juce::Font bold;
        juce::Font extra_bold;
        juce::Font black;
    } font{
        .extra_light = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::SourceCodeProExtraLight_ttf,
            embed_fonts::SourceCodeProExtraLight_ttfSize),
        .light = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::SourceCodeProLight_ttf,
            embed_fonts::SourceCodeProLight_ttfSize),
        .regular = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::SourceCodeProRegular_ttf,
            embed_fonts::SourceCodeProRegular_ttfSize),
        .medium = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::SourceCodeProMedium_ttf,
            embed_fonts::SourceCodeProMedium_ttfSize),
        .semi_bold = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::SourceCodeProSemiBold_ttf,
            embed_fonts::SourceCodeProSemiBold_ttfSize),
        .bold = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::SourceCodeProBold_ttf, embed_fonts::SourceCodeProBold_ttfSize),
        .extra_bold = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::SourceCodeProExtraBold_ttf,
            embed_fonts::SourceCodeProExtraBold_ttfSize),
        .black = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::SourceCodeProBlack_ttf,
            embed_fonts::SourceCodeProBlack_ttfSize),
    };
    return font;
}

[[nodiscard]] inline auto roboto_mono() -> auto const &
{
    static const struct
    {
        juce::Font thin;
        juce::Font extra_light;
        juce::Font light;
        juce::Font regular;
        juce::Font medium;
        juce::Font semi_bold;
        juce::Font bold;
    } font{
        .thin = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::RobotoMonoThin_ttf, embed_fonts::RobotoMonoThin_ttfSize),
        .extra_light = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::RobotoMonoExtraLight_ttf,
            embed_fonts::RobotoMonoExtraLight_ttfSize),
        .light = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::RobotoMonoLight_ttf, embed_fonts::RobotoMonoLight_ttfSize),
        .regular = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::RobotoMonoRegular_ttf, embed_fonts::RobotoMonoRegular_ttfSize),
        .medium = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::RobotoMonoMedium_ttf, embed_fonts::RobotoMonoMedium_ttfSize),
        .semi_bold = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::RobotoMonoSemiBold_ttf,
            embed_fonts::RobotoMonoSemiBold_ttfSize),
        .bold = juce::Typeface::createSystemTypefaceFor(
            embed_fonts::RobotoMonoBold_ttf, embed_fonts::RobotoMonoBold_ttfSize),
    };
    return font;
}

[[nodiscard]] inline auto monospaced() -> auto const &
{
    // return source_code_pro();
    return roboto_mono();
}

[[nodiscard]] inline auto symbols() -> juce::Font const &
{
    static auto const font = juce::Font{
        juce::Typeface::createSystemTypefaceFor(
            embed_fonts::NotoSansSymbols2Regular_ttf,
            embed_fonts::NotoSansSymbols2Regular_ttfSize),
    };
    return font;
}

} // namespace xen::gui::fonts