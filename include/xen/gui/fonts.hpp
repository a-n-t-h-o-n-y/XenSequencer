#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <embed_fonts.hpp>

namespace xen::gui::fonts
{

/**
 * Take embedded binary data and create a juce::Font.
 */
[[nodiscard]] inline auto binary_to_font(char const *data, int size) -> juce::Font
{

    return juce::Font{
        juce::FontOptions{
            juce::Typeface::createSystemTypefaceFor(data, (std::size_t)size),
        },
    };
}

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
        .extra_light = binary_to_font(embed_fonts::SourceCodeProExtraLight_ttf,
                                      embed_fonts::SourceCodeProExtraLight_ttfSize),
        .light = binary_to_font(embed_fonts::SourceCodeProLight_ttf,
                                embed_fonts::SourceCodeProLight_ttfSize),
        .regular = binary_to_font(embed_fonts::SourceCodeProRegular_ttf,
                                  embed_fonts::SourceCodeProRegular_ttfSize),
        .medium = binary_to_font(embed_fonts::SourceCodeProMedium_ttf,
                                 embed_fonts::SourceCodeProMedium_ttfSize),
        .semi_bold = binary_to_font(embed_fonts::SourceCodeProSemiBold_ttf,
                                    embed_fonts::SourceCodeProSemiBold_ttfSize),
        .bold = binary_to_font(embed_fonts::SourceCodeProBold_ttf,
                               embed_fonts::SourceCodeProBold_ttfSize),
        .extra_bold = binary_to_font(embed_fonts::SourceCodeProExtraBold_ttf,
                                     embed_fonts::SourceCodeProExtraBold_ttfSize),
        .black = binary_to_font(embed_fonts::SourceCodeProBlack_ttf,
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
        .thin = binary_to_font(embed_fonts::RobotoMonoThin_ttf,
                               embed_fonts::RobotoMonoThin_ttfSize),
        .extra_light = binary_to_font(embed_fonts::RobotoMonoExtraLight_ttf,
                                      embed_fonts::RobotoMonoExtraLight_ttfSize),
        .light = binary_to_font(embed_fonts::RobotoMonoLight_ttf,
                                embed_fonts::RobotoMonoLight_ttfSize),
        .regular = binary_to_font(embed_fonts::RobotoMonoRegular_ttf,
                                  embed_fonts::RobotoMonoRegular_ttfSize),
        .medium = binary_to_font(embed_fonts::RobotoMonoMedium_ttf,
                                 embed_fonts::RobotoMonoMedium_ttfSize),
        .semi_bold = binary_to_font(embed_fonts::RobotoMonoSemiBold_ttf,
                                    embed_fonts::RobotoMonoSemiBold_ttfSize),
        .bold = binary_to_font(embed_fonts::RobotoMonoBold_ttf,
                               embed_fonts::RobotoMonoBold_ttfSize),
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
    static auto const font =
        binary_to_font(embed_fonts::NotoSansSymbols2Regular_ttf,
                       embed_fonts::NotoSansSymbols2Regular_ttfSize);
    return font;
}

} // namespace xen::gui::fonts