#pragma once

#include <string>

namespace xen
{

/**
 * Generate guide text that completes the `partial_command` and lists argument info if
 * applicable.
 *
 * @param partial_command The partial command string to autocomplete.
 * @return std::string The guide text, does not duplicate partial_command text.
 */
[[nodiscard]] auto generate_guide_text(std::string const &partial_command)
    -> std::string;

/**
 * Generate the missing part of the last word in `partial_command` from the command
 * catalog.
 *
 * @param partial_command The partial command string to autocomplete.
 * @return std::string The missing part of the last word, or an empty string if there is
 * no match.
 */
[[nodiscard]] auto complete_id(std::string const &partial_command) -> std::string;

} // namespace xen
