#pragma once

#include <memory>
#include <string_view>

#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

/**
 * Color values for semantic color names.
 * @see https://github.com/hundredrabbits/Themes
 */
struct Theme
{
    std::uint32_t background;
    std::uint32_t fg_high;
    std::uint32_t fg_med;
    std::uint32_t fg_low;
    std::uint32_t fg_inv;
    std::uint32_t bg_high;
    std::uint32_t bg_med;
    std::uint32_t bg_low;
    std::uint32_t bg_inv;
};

/**
 * Find a theme by name.
 *
 * @details Theme names are located in src/gui/themes.cpp
 * @param name The name of the theme to lookup.
 * @return Theme The theme that matches the name.
 * @throws std::runtime_error If the theme is not found, \p name must be an exact match.
 */
[[nodiscard]] auto find_theme(std::string_view name) -> Theme;

/**
 * Convert a Theme into a juce::LookAndFeel object.
 *
 * @param theme The theme to convert.
 * @return std::unique_ptr<juce::LookAndFeel> A owner pointer to the LookAndFeel object
 * that represents the theme.
 */
[[nodiscard]] auto make_laf(Theme const &theme) -> std::unique_ptr<juce::LookAndFeel>;

} // namespace xen::gui