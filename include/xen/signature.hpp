#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <xen/utility.hpp>

namespace xen
{

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
    std::string_view name;
    std::optional<T> default_value = std::nullopt;
};

template <typename... Args>
using ArgInfos = std::tuple<ArgInfo<Args>...>;

template <typename ID_t, typename... Args>
struct Signature
{
    ID_t id;
    ArgInfos<Args...> args;
};

/**
 * @brief Holds display information about a command signature.
 *
 * Used to display pieces of the command as it is typed into the CommandBar.
 */
struct SignatureDisplay
{
    std::string id;
    std::vector<std::string> arguments;
};

// -----------------------------------------------------------------------------

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
        return "Float";
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        return "Bool";
    }
    else if constexpr (std::is_same_v<T, int>)
    {
        return "Int";
    }
    else if constexpr (std::is_same_v<T, unsigned short> ||
                       std::is_same_v<T, unsigned int> ||
                       std::is_same_v<T, std::size_t>)
    {
        return "Unsigned";
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return "String";
    }
    else if constexpr (std::is_same_v<T, sequence::TimeSignature>)
    {
        return "TimeSignature";
    }
    else if constexpr (std::is_same_v<T, InputMode>)
    {
        return "InputMode";
    }
    else if constexpr (std::is_same_v<T, std::filesystem::path>)
    {
        return "Filepath";
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
[[nodiscard]] auto arg_info_to_string(ArgInfo<T> const &arg) -> std::string
{
    auto os = std::ostringstream{};
    os << type_name<T>() << ": " << arg.name;

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

template <typename ID_t, typename... Args, std::size_t... I>
[[nodiscard]] auto generate_display(ID_t const &id, ArgInfos<Args...> const &arg_infos,
                                    std::index_sequence<I...>) -> SignatureDisplay
{
    auto oss = std::ostringstream{};
    oss << id;

    auto display = SignatureDisplay{oss.str(), {}};

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
 * @param id The id of the command.
 * @param arg_infos The argument infos.
 * @return SignatureDisplay The generated signature display.
 */
template <typename ID_t, typename... Args>
[[nodiscard]] auto generate_display(Signature<ID_t, Args...> const &signature)
    -> SignatureDisplay
{
    return generate_display(signature.id, signature.args,
                            std::index_sequence_for<Args...>());
}

} // namespace xen
