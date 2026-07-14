#pragma once

#include <string>
#include <string_view>

namespace xen
{

/**
 * Converts a string to lowercase.
 *
 * @param x The string to convert.
 * @return std::string The converted string.
 */
[[nodiscard]] auto to_lower(std::string_view x) -> std::string;

/**
 * Surrounds a string with single quotes.
 *
 * @param input The string to surround with single quotes.
 * @return std::string The string surrounded by single quotes.
 */
[[nodiscard]] auto single_quote(std::string const &input) -> std::string;

} // namespace xen
