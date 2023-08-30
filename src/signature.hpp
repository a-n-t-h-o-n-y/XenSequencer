#pragma once

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "util.hpp"

namespace xen
{

/**
 * @brief Holds display information about a command signature.
 *
 * Used to display pieces of the command as it is typed into the CommandBar.
 */
struct SignatureDisplay
{
    std::string name;
    std::vector<std::string> arguments;
};

/**
 * @brief A struct that contains information about a command argument.
 *
 * Does not contain the actual value of the argument.
 *
 * @tparam T The type of the argument.
 */
template <typename T>
struct ArgInfo
{
    std::string name;
    std::optional<T> default_value = std::nullopt;
};

template <typename... Args>
using ArgInfos = std::tuple<ArgInfo<Args>...>;

/**
 * @brief Stringify the given template type parameter.
 *
 * @tparam T The type to stringify.
 * @return std::string The type as string.
 */
template <typename T>
[[nodiscard]] auto type_name() -> std::string
{
    if constexpr (std::is_floating_point_v<T>)
    {
        return "float";
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        return "bool";
    }
    else if constexpr (std::is_same_v<T, int>)
    {
        return "int";
    }
    else if constexpr (std::is_same_v<T, unsigned short> ||
                       std::is_same_v<T, unsigned int> ||
                       std::is_same_v<T, std::size_t>)
    {
        return "unsigned";
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return "string";
    }
    else
    {
        static_assert(always_false<T>::value, "Unsupported type.");
    }
}

/**
 * @brief Stringify the given argument info.
 *
 * @tparam T The type of the argument.
 * @param arg The argument info.
 * @return std::string The argument info as string.
 */
template <typename T>
auto arg_info_to_string(ArgInfo<T> const &arg) -> std::string
{
    auto os = std::ostringstream{};
    os << arg.name;

    // Default Value
    if (arg.default_value.has_value())
    {
        os << "=";
        if constexpr (std::is_same_v<T, std::string>)
        {
            os << '\"' << arg.default_value.value() << '\"';
        }
        else
        {
            os << arg.default_value.value();
        }
    }

    return os.str();
}

template <typename... Args, std::size_t... I>
auto generate_signature(std::string name, ArgInfos<Args...> const &arg_infos,
                        std::index_sequence<I...>) -> SignatureDisplay
{
    auto display = SignatureDisplay{name, {}};

    auto add_arg = [&display](auto const &str) {
        display.arguments.push_back('[' + str + ']');
    };

    (add_arg(arg_info_to_string(std::get<I>(arg_infos))), ...);

    return display;
}

/**
 * @brief Generate a signature display for the given command name and arg infos.
 *
 * @tparam Args The types of the arguments.
 * @param name The name of the command.
 * @param arg_infos The argument infos.
 * @return SignatureDisplay The generated signature display.
 */
template <typename... Args>
auto generate_signature(std::string const &name, ArgInfos<Args...> const &arg_infos)
    -> SignatureDisplay
{
    return generate_signature(name, arg_infos, std::index_sequence_for<Args...>());
}

} // namespace xen