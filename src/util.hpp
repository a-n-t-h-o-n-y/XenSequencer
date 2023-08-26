#pragma once

#include <string>

namespace xen
{

/**
 * @brief Converts a string to lowercase.
 *
 * @param x The string to convert.
 * @return std::string The converted string.
 */
[[nodiscard]] auto to_lower(std::string x) -> std::string;

} // namespace xen