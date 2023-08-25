#pragma once

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>

namespace xen
{

[[nodiscard]] inline auto to_lower(std::string const &x) -> std::string
{
    auto result = std::string{};
    std::transform(std::cbegin(x), std::cend(x), std::back_inserter(result), ::tolower);
    return result;
}

[[nodiscard]] inline auto parse_int(std::string const &x) -> std::optional<int>
{
    try
    {
        return std::stoi(x);
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

[[nodiscard]] inline auto parse_float(std::string const &x) -> std::optional<float>
{
    try
    {
        return std::stof(x);
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
struct always_false : std::false_type
{
};

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
    else
    {
        static_assert(always_false<T>::value, "Unsupported type.");
    }
}

template <typename... T, std::size_t... I>
[[nodiscard]] inline auto validate_args_impl(std::vector<std::string> const &args,
                                             std::index_sequence<I...>)
{
    return std::tuple{parse<T>(args.at(I))...};
}

template <typename... T>
[[nodiscard]] inline auto validate_args(std::vector<std::string> const &args)
{
    if (args.size() != sizeof...(T))
    {
        throw std::invalid_argument{"Invalid number of arguments."};
    }
    return validate_args_impl<T...>(args, std::index_sequence_for<T...>{});
}

} // namespace xen