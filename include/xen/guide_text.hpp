#pragma once

#include <string>

#include <xen/xen_command_tree.hpp>

namespace xen
{

// TODO
// if possible, you should have a function that just does the autocomplete of the
// last typed in word, then the guide text implementation can use this, and it should
// only attempt to autocomplete a single word, though i don't think its possible to do
// more. Then another type of function that generates ArgInfo display, then the generate
// guide text and autocomplete functions just use these in their implementation
// in order to do this you need a function that can autocomplete, then pop off a single
// word so it can be sent to the arginfo one. But what if the last word is already
// complete? plus this has to dive in through multiple recursion layers until it gets to
// the end of the command string.

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