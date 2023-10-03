#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <sequence/time_signature.hpp>

#include <xen/input_mode.hpp>
#include <xen/signature.hpp>
#include <xen/string_manip.hpp>
#include <xen/utility.hpp>

namespace xen
{

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
[[nodiscard]] auto parse_unsigned(std::string const &x) -> std::optional<T>
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
            static_assert(xen::always_false<T>::value, "Unsupported size_t.");
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
[[nodiscard]] auto parse_float(std::string const &x) -> std::optional<T>
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
            static_assert(xen::always_false<T>::value, "Unsupported float.");
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

/**
 * @brief Parses a string into a TimeSignature.
 *
 * @param x The input string, formatted as "x/y" or "x".
 * @return A TimeSignature object containing the parsed numerator and denominator.
 * @throws std::invalid_argument if the string is not in the correct format.
 */
[[nodiscard]] inline auto parse_time_signature(std::string const &x)
    -> sequence::TimeSignature
{
    auto ss = std::istringstream{x};
    auto ts = sequence::TimeSignature{};
    ss >> ts;
    if (!ss.eof())
    {
        throw std::invalid_argument{"Invalid time signature format: " + x};
    }
    return ts;
}

/**
 * @brief Parses a string into a type T object.
 */
template <typename T>
[[nodiscard]] auto parse(std::string const &x) -> T
{
    if constexpr (std::is_floating_point_v<T>)
    {
        auto const result = parse_float<T>(x);
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
    else if constexpr (std::is_same_v<T, int>)
    {
        auto const result = parse_int(x);
        if (!result.has_value())
        {
            throw std::invalid_argument{"Invalid integer: " + x};
        }
        return result.value();
    }
    else if constexpr (std::is_same_v<T, unsigned short> ||
                       std::is_same_v<T, unsigned int> ||
                       std::is_same_v<T, std::size_t>)
    {
        auto const result = parse_unsigned<T>(x);
        if (!result.has_value())
        {
            throw std::invalid_argument{"Invalid unsigned: " + x};
        }
        return result.value();
    }
    else if constexpr (std::is_same_v<T, sequence::TimeSignature>)
    {
        return parse_time_signature(x);
    }
    else if constexpr (std::is_same_v<T, InputMode>)
    {
        return parse_input_mode(x);
    }
    else if constexpr (std::is_same_v<T, std::filesystem::path>)
    {
        return std::filesystem::path{x};
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return x;
    }
    else if constexpr (std::is_same_v<T, std::string_view>)
    {
        return x;
    }
    else
    {
        static_assert(xen::always_false<T>::value, "Unsupported type.");
    }
}

/**
 * @brief Splits a string into arguments, considering quotes.
 *
 * @param s The string to split.
 */
[[nodiscard]] auto split_args(std::string const &s) -> std::vector<std::string>;

/**
 * @brief Extracts argument at index I from the given argument list.
 *
 * @tparam I The index of the argument to extract.
 * @param args The argument list as strings.
 * @param arg_infos The argument infos.
 * @return auto The extracted argument.
 *
 * @throws std::invalid_argument if the argument is missing and no default value is
 * provided.
 */
template <std::size_t I, typename T>
[[nodiscard]] auto get_argument_value(std::vector<std::string> const &args,
                                      ArgInfo<T> const &arg_info) -> T
{
    if (I < args.size())
    {
        return parse<T>(args[I]);
    }
    else if (arg_info.default_value.has_value())
    {
        return arg_info.default_value.value();
    }
    else
    {
        throw std::invalid_argument("Missing argument and no default value");
    }
}

} // namespace xen
