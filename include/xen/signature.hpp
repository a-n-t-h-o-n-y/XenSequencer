#pragma once

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>

#include <sequence/pattern.hpp>
#include <sequence/time_signature.hpp>

#include <xen/input_mode.hpp>
#include <xen/modulator.hpp>
#include <xen/utility.hpp>

namespace xen
{

/**
 * A struct that contains information about a command argument.
 *
 * @details Does not contain the actual value of the argument.
 * @tparam T The type of the argument.
 */
template <typename T>
struct ArgInfo
{
    std::string_view name;
    std::optional<T> default_value = std::nullopt;
};

template <typename U>
struct ArgType;

template <typename T>
struct ArgType<ArgInfo<T>>
{
    using type = T;
};

template <typename U>
using arg_type_t = typename ArgType<U>::type;

template <typename... Args>
using ArgInfos = std::tuple<ArgInfo<Args>...>;

template <typename... Args>
struct Signature
{
    std::string_view id;
    ArgInfos<Args...> args = {};
};

template <typename... Args>
struct PatternedSignature
{
    std::string_view id;
    ArgInfos<Args...> args = {};
};

/**
 * Create a Signature with the given id and arguments.
 * @details This exists because Apple clang 14 does not support deduction guides for
 * aggregate initialization.
 * @param id The id of the signature.
 * @param args The ArgInfos.
 * @return Signature object.
 */
template <typename... Args>
[[nodiscard]] auto signature(std::string_view id, Args &&...args)
{
    return Signature<arg_type_t<Args>...>{
        .id = id,
        .args = std::tuple{std::forward<Args>(args)...},
    };
}

/**
 * Create a PatternedSignature if passed a sequence::Pattern ArgInfo as first param.
 * @details This exists because Apple clang 14 does not support deduction guides for
 * aggregate initialization.
 * @param id The id of the signature.
 * @param pattern The pattern argument, its ID is not used.
 * @param args All other ArgInfos.
 * @return PatternedSignature object.
 */
template <typename... Args>
[[nodiscard]] auto signature(std::string_view id, ArgInfo<sequence::Pattern>,
                             Args &&...args)
{
    // ^^ This must take pattern arg by value, otherwise other overload is picked up.
    return PatternedSignature<arg_type_t<Args>...>{
        .id = id,
        .args = std::tuple{std::forward<Args>(args)...},
    };
}

/**
 * Create a PatternedSignature with the given id and arguments.
 * @details Overload provided instead of optional<T> for implicit construction at call
 * site.
 * @tparam T The type of the argument.
 * @param name The name of the argument.
 * @param default_value The default value of the argument.
 * @return ArgInfo<T>
 */
template <typename T>
[[nodiscard]] auto arg(std::string_view name, T &&default_value) -> ArgInfo<T>
{
    return ArgInfo<T>{
        .name = name,
        .default_value = std::forward<T>(default_value),
    };
}

/**
 * Create an ArgInfo with the given name and no default value.
 * @tparam T The type of the argument.
 * @param name The name of the argument.
 * @return ArgInfo<T>
 */
template <typename T>
[[nodiscard]] auto arg(std::string_view name) -> ArgInfo<T>
{
    return ArgInfo<T>{.name = name};
}

/**
 * Holds display information about a command signature.
 *
 * @details Used to display pieces of the command as it is typed into the CommandBar.
 * Pattern is not used because it is never displayed as part of the guide text.
 */
struct SignatureDisplay
{
    std::string id;
    std::vector<std::string> arguments;
    bool pattern_arg = false;
};

// -----------------------------------------------------------------------------

// Forward Declartion for generate_variant_type_names()
template <typename T>
[[nodiscard]]
auto type_name() -> std::string;

template <typename VariantType>
[[nodiscard]]
auto generate_variant_type_names() -> std::string
{
    return []<std::size_t... Is>(std::index_sequence<Is...>) -> std::string {
        // sequence of type name strings with | divider
        auto oss = std::ostringstream{};
        ((oss << type_name<std::variant_alternative_t<Is, VariantType>>() << '|'), ...);

        auto result = oss.str();
        if (!result.empty()) // Remove the last '|'
        {
            result.pop_back();
        }
        return result;
    }(std::make_index_sequence<std::variant_size_v<VariantType>>{});
}

/**
 * Stringify the given template type parameter.
 *
 * @tparam T The type to stringify.
 * @return std::string The type name display.
 */
template <typename T>
[[nodiscard]]
auto type_name() -> std::string
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
    else if constexpr (std::is_same_v<T, juce::File>)
    {
        return "Filepath";
    }
    else if constexpr (std::is_same_v<T, Modulator>)
    {
        return "Modulator";
    }
    else if constexpr (utility::is_variant_v<T>)
    {
        return generate_variant_type_names<T>();
    }
    else
    {
        static_assert([] { return false; }(), "Unsupported type.");
    }
}

template <typename T>
[[nodiscard]]
auto arg_to_string(T const &arg) -> std::string
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        return '\"' + arg + '\"';
    }
    else if constexpr (std::is_same_v<T, Modulator>)
    {
        (void)arg;
        return "[](float){???}";
    }
    else if constexpr (utility::is_variant_v<T>)
    {
        return std::visit([](auto const &v) { return arg_to_string(v); }, arg);
    }
    else
    {
        auto os = std::ostringstream{};
        os << arg;
        return os.str();
    }
}

/**
 * Stringify the given argument info.
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
        os << "=" << arg_to_string(arg.default_value.value());
    }

    return os.str();
}

template <typename... Args, std::size_t... Is>
[[nodiscard]] auto generate_display(std::string_view id,
                                    ArgInfos<Args...> const &arg_infos,
                                    std::index_sequence<Is...>) -> SignatureDisplay
{
    auto oss = std::ostringstream{};
    oss << id;

    auto display = SignatureDisplay{.id = oss.str(), .arguments = {}};

    [[maybe_unused]] auto add_arg = [&display](auto const &str) {
        display.arguments.push_back('[' + str + ']');
    };

    (add_arg(arg_info_to_string(std::get<Is>(arg_infos))), ...);

    return display;
}

/**
 * Generate a signature display for the given Signature.
 *
 * @tparam Args The types of the arguments.
 * @param signature The signature to generate the display for.
 * @return SignatureDisplay
 */
template <typename... Args>
[[nodiscard]] auto generate_display(Signature<Args...> const &signature)
    -> SignatureDisplay
{
    return generate_display(signature.id, signature.args,
                            std::index_sequence_for<Args...>());
}

/**
 * Generate a signature display for the given PatternedSignature.
 *
 * @tparam Args The types of the arguments.
 * @param signature The signature to generate the display for.
 * @return SignatureDisplay
 */
template <typename... Args>
[[nodiscard]] auto generate_display(PatternedSignature<Args...> const &signature)
    -> SignatureDisplay
{
    auto result = generate_display(signature.id, signature.args,
                                   std::index_sequence_for<Args...>());
    result.pattern_arg = true;
    return result;
}

} // namespace xen
