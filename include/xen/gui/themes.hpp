#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <string_view>

namespace xen::gui
{

/**
 * Lookup a color theme by name.
 *
 * @details Names are located in src/gui/themes.cpp
 *
 * @param name The name of the theme to lookup.
 * @return std::unique_ptr<juce::LookAndFeel> A pointer to the LookAndFeel object that
 * represents the theme.
 * @throws std::runtime_error If the theme is not found, \p name must be exact.
 */
[[nodiscard]] auto find_theme(std::string_view name)
    -> std::unique_ptr<juce::LookAndFeel>;

} // namespace xen::gui