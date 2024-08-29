#pragma once

#include <string>

#include <xen/xen_command_tree.hpp>

namespace xen
{

/**
 * Generate guide text that completes the `partial_command` and lists argument info if
 * applicable.
 *
 * @param command_tree The command tree to use for autocompletion.
 * @param partial_command The partial command string to autocomplete.
 * @return std::string The guide text, does not duplicate partial_command text.
 */
[[nodiscard]] auto generate_guide_text(XenCommandTree const &command_tree,
                                       std::string const &partial_command)
    -> std::string;

/**
 * Generate the missing part of the last word in `partial_command` from the command
 * tree.
 *
 * @param command_tree The command tree to use for autocompletion.
 * @param partial_command The partial command string to autocomplete.
 * @return std::string The missing part of the last word, or an empty string if there is
 * no match.
 */
[[nodiscard]] auto complete_id(XenCommandTree const &command_tree,
                               std::string const &partial_command) -> std::string;

} // namespace xen