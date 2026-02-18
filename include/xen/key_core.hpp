#pragma once

#include <map>
#include <string>

#include <juce_core/juce_core.h>

namespace xen
{

[[nodiscard]] auto export_merged_keymap(
    juce::File const &default_keys, juce::File const &user_keys)
    -> std::map<std::string, std::map<std::string, std::string>>;

} // namespace xen
