#pragma once

#include <cmath>
#include <concepts>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace xen::numeric
{

template <std::integral To, std::integral From>
[[nodiscard]] auto checked_cast(From value, std::string_view message) -> To
{
    if (!std::in_range<To>(value))
    {
        throw std::overflow_error{std::string{message}};
    }
    return static_cast<To>(value);
}

[[nodiscard]] inline auto checked_add(int lhs, int rhs, std::string_view message) -> int
{
    auto const result = static_cast<long long>(lhs) + static_cast<long long>(rhs);
    if (!std::in_range<int>(result))
    {
        throw std::overflow_error{std::string{message}};
    }
    return static_cast<int>(result);
}

[[nodiscard]] inline auto checked_mul(int lhs, int rhs, std::string_view message) -> int
{
    auto const result = static_cast<long long>(lhs) * static_cast<long long>(rhs);
    if (!std::in_range<int>(result))
    {
        throw std::overflow_error{std::string{message}};
    }
    return static_cast<int>(result);
}

[[nodiscard]] inline auto floor_to_int(float value, std::string_view message) -> int
{
    if (!std::isfinite(value))
    {
        throw std::invalid_argument{std::string{message}};
    }

    auto const floored = std::floor(static_cast<double>(value));
    if (floored < static_cast<double>(std::numeric_limits<int>::min()) ||
        floored > static_cast<double>(std::numeric_limits<int>::max()))
    {
        throw std::overflow_error{std::string{message}};
    }
    return static_cast<int>(floored);
}

} // namespace xen::numeric
