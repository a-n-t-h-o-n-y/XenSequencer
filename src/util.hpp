#pragma once

#include <algorithm>
#include <string>

namespace xen
{

/**
 * @brief Converts a string to lowercase.
 *
 * @param x The string to convert.
 * @return std::string The converted string.
 */
[[nodiscard]] inline auto to_lower(std::string x) -> std::string
{
    std::transform(std::cbegin(x), std::cend(x), std::begin(x), ::tolower);
    return x;
}

} // namespace xen