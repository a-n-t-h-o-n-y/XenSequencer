#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "util.hpp"

namespace xen
{

template <typename T>
struct always_false : std::false_type
{
};

[[nodiscard]] inline auto parse_int(std::string const &x) -> std::optional<int>
{
    try
    {
        auto pos = std::size_t{0};

        auto const result = std::stoi(x, &pos);

        // This verifies the entire string was parsed.
        if (pos != x.size())
        {
            return std::nullopt;
        }

        return result;
    }
    catch (std::invalid_argument const &)
    {
        return std::nullopt;
    }
    catch (std::out_of_range const &)
    {
        return std::nullopt;
    }
}

template <typename T = std::size_t>
[[nodiscard]] inline auto parse_unsigned(std::string const &x) -> std::optional<T>
{
    static_assert(std::is_unsigned_v<T>, "T must be unsigned.");

    if (x.find('-') != std::string::npos)
    {
        return std::nullopt;
    }

    try
    {
        auto pos = std::size_t{0};
        auto result = T{0};

        if constexpr (sizeof(T) == sizeof(unsigned))
        {
            result = std::stoul(x, &pos);
        }
        else if (sizeof(T) == sizeof(unsigned short))
        {
            result = std::stoul(x, &pos);
        }
        else if constexpr (sizeof(T) == sizeof(unsigned long))
        {
            result = std::stoul(x, &pos);
        }
        else if constexpr (sizeof(T) == sizeof(unsigned long long))
        {
            result = std::stoull(x, &pos);
        }
        else
        {
            static_assert(always_false<T>::value, "Unsupported size_t.");
        }

        // This verifies the entire string was parsed.
        if (pos != x.size())
        {
            return std::nullopt;
        }

        return result;
    }
    catch (std::invalid_argument const &)
    {
        return std::nullopt;
    }
    catch (std::out_of_range const &)
    {
        return std::nullopt;
    }
}

template <typename T = float>
[[nodiscard]] inline auto parse_float(std::string const &x) -> std::optional<T>
{
    static_assert(std::is_floating_point_v<T>, "T must be floating point.");

    try
    {
        auto pos = std::size_t{0};
        auto result = T{0};

        if constexpr (sizeof(T) == sizeof(float))
        {
            result = std::stof(x, &pos);
        }
        else if constexpr (sizeof(T) == sizeof(double))
        {
            result = std::stod(x, &pos);
        }
        else if constexpr (sizeof(T) == sizeof(long double))
        {
            result = std::stold(x, &pos);
        }
        else
        {
            static_assert(always_false<T>::value, "Unsupported float.");
        }

        // This verifies the entire string was parsed.
        if (pos != x.size())
        {
            return std::nullopt;
        }

        return result;
    }
    catch (std::invalid_argument const &)
    {
        return std::nullopt;
    }
    catch (std::out_of_range const &)
    {
        return std::nullopt;
    }
}

[[nodiscard]] inline auto parse_bool(std::string const &x) -> std::optional<bool>
{
    auto const lower = to_lower(x);
    if (lower == "true")
    {
        return true;
    }
    else if (lower == "false")
    {
        return false;
    }
    else
    {
        return std::nullopt;
    }
}

[[nodiscard]] inline auto parse_string(std::string const &x) -> std::string
{
    return to_lower(x);
}

template <typename T>
[[nodiscard]] inline auto parse(std::string const &x) -> T
{
    if constexpr (std::is_same_v<T, int>)
    {
        auto const result = parse_int(x);
        if (!result.has_value())
        {
            throw std::invalid_argument{"Invalid integer: " + x};
        }
        return result.value();
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        auto const result = parse_float(x);
        if (!result.has_value())
        {
            throw std::invalid_argument{"Invalid float: " + x};
        }
        return result.value();
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        auto const result = parse_bool(x);
        if (!result.has_value())
        {
            throw std::invalid_argument{"Invalid bool: " + x};
        }
        return result.value();
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return parse_string(x);
    }
    else if constexpr (std::is_unsigned_v<T>)
    {
        auto const result = parse_unsigned<T>(x);
        if (!result.has_value())
        {
            throw std::invalid_argument{"Invalid unsigned: " + x};
        }
        return result.value();
    }
    else
    {
        static_assert(always_false<T>::value, "Unsupported type.");
    }
}

template <typename... T, std::size_t... I>
[[nodiscard]] inline auto extract_args_impl(std::vector<std::string> const &args,
                                            std::index_sequence<I...>)
{
    return std::tuple{parse<T>(args.at(I))...};
}

template <typename... T>
[[nodiscard]] inline auto extract_args(std::vector<std::string> const &args)
{
    if (args.size() != sizeof...(T))
    {
        throw std::invalid_argument{"Invalid number of arguments."};
    }
    return extract_args_impl<T...>(args, std::index_sequence_for<T...>{});
}

} // namespace xen