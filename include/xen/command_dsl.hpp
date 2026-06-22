#pragma once

#include <cstddef>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <xen/parse_args.hpp>

#include <xen/command.hpp>
#include <xen/command_catalog_types.hpp>
#include <xen/submission_effects.hpp>

namespace xen::catalog_detail
{

inline auto format_command_path(std::vector<std::string> const &path) -> std::string
{
    auto result = std::string{};
    auto separator = std::string{};
    for (auto const &token : path)
    {
        result += separator;
        result += token;
        separator = " ";
    }
    return result;
}

template <typename T>
struct ArgDef
{
    std::string type;
    std::string name;
    std::optional<T> default_value{};
    std::optional<std::string> default_text{};
    std::function<std::optional<std::string>(T const &)> validate{};
};

template <typename T>
struct ArgTraits;

template <>
struct ArgTraits<int>
{
    static constexpr auto type_name = "Int";
};

template <>
struct ArgTraits<std::size_t>
{
    static constexpr auto type_name = "Unsigned";
};

template <>
struct ArgTraits<float>
{
    static constexpr auto type_name = "Float";
};

template <>
struct ArgTraits<double>
{
    static constexpr auto type_name = "Float";
};

template <>
struct ArgTraits<bool>
{
    static constexpr auto type_name = "Bool";
};

template <>
struct ArgTraits<std::string>
{
    static constexpr auto type_name = "String";
};

template <>
struct ArgTraits<InputMode>
{
    static constexpr auto type_name = "InputMode";
};

template <>
struct ArgTraits<sequence::TimeSignature>
{
    static constexpr auto type_name = "TimeSignature";
};

template <>
struct ArgTraits<std::variant<int, Modulator>>
{
    static constexpr auto type_name = "Int|Modulator";
};

template <>
struct ArgTraits<std::variant<float, Modulator>>
{
    static constexpr auto type_name = "Float|Modulator";
};

inline auto format_float(double value) -> std::string
{
    auto stream = std::ostringstream{};
    stream << std::setprecision(6) << std::defaultfloat << value;
    return stream.str();
}

template <typename T>
auto format_default_value(T const &value) -> std::string
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        return value;
    }
    else if constexpr (std::is_same_v<T, std::size_t>)
    {
        return std::to_string(value);
    }
    else if constexpr (std::is_same_v<T, int>)
    {
        return std::to_string(value);
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        return value ? "true" : "false";
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        return format_float(static_cast<double>(value));
    }
    else if constexpr (std::is_same_v<T, sequence::TimeSignature>)
    {
        return std::to_string(value.numerator) + "/" +
               std::to_string(value.denominator);
    }
    else if constexpr (std::is_same_v<T, InputMode>)
    {
        return to_string(value);
    }
    else if constexpr (std::is_same_v<T, Modulator>)
    {
        return "{}";
    }
    else if constexpr (std::is_same_v<T, std::variant<int, Modulator>>)
    {
        return std::visit(
            [](auto const &variant_value) {
                return format_default_value(variant_value);
            },
            value);
    }
    else if constexpr (std::is_same_v<T, std::variant<float, Modulator>>)
    {
        return std::visit(
            [](auto const &variant_value) {
                return format_default_value(variant_value);
            },
            value);
    }
    else
    {
        static_assert([] { return false; }(), "Unsupported default value type.");
    }
}

template <typename T>
auto required_arg(std::string type, std::string name) -> ArgDef<T>
{
    return ArgDef<T>{
        .type = std::move(type),
        .name = std::move(name),
    };
}

template <typename T>
auto required_arg(std::string name) -> ArgDef<T>
{
    return required_arg<T>(ArgTraits<T>::type_name, std::move(name));
}

template <typename T>
auto optional_arg(std::string type, std::string name, T default_value,
                  std::optional<std::string> default_text = std::nullopt) -> ArgDef<T>
{
    if (!default_text.has_value())
    {
        default_text = format_default_value(default_value);
    }

    return ArgDef<T>{
        .type = std::move(type),
        .name = std::move(name),
        .default_value = std::move(default_value),
        .default_text = std::move(default_text),
    };
}

template <typename T>
auto optional_arg(std::string name, T default_value,
                  std::optional<std::string> default_text = std::nullopt) -> ArgDef<T>
{
    return optional_arg<T>(ArgTraits<T>::type_name, std::move(name),
                           std::move(default_value), std::move(default_text));
}

template <typename T, typename Validator>
auto constrained(ArgDef<T> argument, Validator validator, std::string error_message)
    -> ArgDef<T>
{
    argument.validate = [validator = std::move(validator),
                         error_message = std::move(error_message)](
                            T const &value) -> std::optional<std::string> {
        if (validator(value))
        {
            return std::nullopt;
        }
        return error_message;
    };
    return argument;
}

template <typename T>
auto parse_arg(std::vector<std::string> const &words, std::size_t index,
               ArgDef<T> const &arg) -> T
{
    auto const validate = [&](T const &value) {
        if (arg.validate)
        {
            if (auto const error = arg.validate(value); error.has_value())
            {
                throw std::invalid_argument{*error};
            }
        }
    };

    if (index >= words.size())
    {
        if (arg.default_value.has_value())
        {
            auto value = *arg.default_value;
            validate(value);
            return value;
        }

        throw CatalogBindException{CatalogBindErrorKind::MissingArgument, arg.name, ""};
    }

    try
    {
        auto result = parse<T>(words[index]);
        validate(result);
        return result;
    }
    catch (std::exception const &e)
    {
        throw CatalogBindException{CatalogBindErrorKind::InvalidArgument, arg.name,
                                   e.what()};
    }
}

template <typename... Ts, std::size_t... Is>
auto parse_args_impl(std::tuple<ArgDef<Ts>...> const &arg_defs,
                     std::vector<std::string> const &words, std::size_t arg_offset,
                     std::index_sequence<Is...>) -> std::tuple<Ts...>
{
    return std::tuple<Ts...>{
        parse_arg(words, arg_offset + Is, std::get<Is>(arg_defs))...,
    };
}

template <typename... Ts>
auto parse_args_tuple(std::tuple<ArgDef<Ts>...> const &arg_defs,
                      std::vector<std::string> const &words, std::size_t arg_offset)
    -> std::tuple<Ts...>
{
    if (words.size() > arg_offset + sizeof...(Ts))
    {
        throw CatalogBindException{
            CatalogBindErrorKind::UnexpectedArgument,
            words[arg_offset + sizeof...(Ts)],
            "",
        };
    }

    return parse_args_impl(arg_defs, words, arg_offset,
                           std::index_sequence_for<Ts...>{});
}

template <typename... Ts>
auto to_metadata_args(std::tuple<ArgDef<Ts>...> const &arg_defs)
    -> std::vector<CatalogArgumentMetadata>
{
    auto metadata = std::vector<CatalogArgumentMetadata>{};
    metadata.reserve(sizeof...(Ts));

    std::apply(
        [&](auto const &...defs) {
            (metadata.push_back(CatalogArgumentMetadata{
                 .type = defs.type,
                 .name = defs.name,
                 .default_value = defs.default_text,
             }),
             ...);
        },
        arg_defs);

    return metadata;
}

template <typename Handler, typename... Ts>
auto command_with_options(std::vector<std::string> path, bool accepts_pattern_prefix,
                          std::string description, CommandPolicy policy,
                          std::tuple<ArgDef<Ts>...> arg_defs, Handler handler)
    -> CommandDefinition
{
    auto metadata = CatalogCommandMetadata{
        .path = std::move(path),
        .accepts_pattern_prefix = accepts_pattern_prefix,
        .arguments = to_metadata_args(arg_defs),
        .description = std::move(description),
    };

    constexpr auto uses_submission_effects =
        std::is_invocable_r_v<std::pair<MessageLevel, std::string>, Handler,
                              PluginState &, SubmissionEffects &,
                              CommandInvocation const &, Ts const &...>;
    auto const command_path = format_command_path(metadata.path);
    auto bind = [arg_defs = std::move(arg_defs), handler = std::move(handler),
                 accepts_pattern_prefix, policy,
                 command_path](CommandInvocation const &invocation,
                               std::size_t arg_offset) -> BoundStep {
        if (invocation.input.has_pattern_prefix && !accepts_pattern_prefix)
        {
            throw CatalogBindException{
                CatalogBindErrorKind::PatternPrefixNotAllowed,
                command_path,
                "",
            };
        }

        auto parsed_args =
            parse_args_tuple(arg_defs, invocation.input.words, arg_offset);

        return ExecutableCommand{
            .canonical = invocation.canonical_segment,
            .invocation = invocation,
            .policy = policy,
            .execute =
                [handler, invocation, parsed_args = std::move(parsed_args)](
                    PluginState &state, SubmissionEffects &effects) {
                    return std::apply(
                        [&](auto const &...values)
                            -> std::pair<MessageLevel, std::string> {
                            if constexpr (std::is_invocable_r_v<
                                              std::pair<MessageLevel, std::string>,
                                              Handler, PluginState &,
                                              SubmissionEffects &,
                                              CommandInvocation const &, Ts const &...>)
                            {
                                return handler(state, effects, invocation, values...);
                            }
                            else
                            {
                                return handler(state, invocation, values...);
                            }
                        },
                        parsed_args);
                },
        };
    };

    return CommandDefinition{
        .metadata = std::move(metadata),
        .policy = policy,
        .uses_submission_effects = uses_submission_effects,
        .bind = std::move(bind),
    };
}

template <typename Handler, typename... Ts>
auto command(std::vector<std::string> path, bool accepts_pattern_prefix,
             std::string description, CommandPolicy policy,
             std::tuple<ArgDef<Ts>...> arg_defs, Handler handler) -> CommandDefinition
{
    return command_with_options(std::move(path), accepts_pattern_prefix,
                                std::move(description), policy, std::move(arg_defs),
                                std::move(handler));
}

inline auto replay_command(std::vector<std::string> path, std::string description,
                           CommandPolicy policy) -> CommandDefinition
{
    auto metadata = CatalogCommandMetadata{
        .path = std::move(path),
        .description = std::move(description),
    };

    auto bind = [](CommandInvocation const &invocation,
                   std::size_t arg_offset) -> BoundStep {
        if (invocation.input.has_pattern_prefix)
        {
            throw CatalogBindException{
                CatalogBindErrorKind::PatternPrefixNotAllowed,
                invocation.input.words.front(),
                "",
            };
        }
        if (invocation.input.words.size() > arg_offset)
        {
            throw CatalogBindException{
                CatalogBindErrorKind::UnexpectedArgument,
                invocation.input.words[arg_offset],
                "",
            };
        }
        return RepeatPrevious{};
    };

    return CommandDefinition{
        .metadata = std::move(metadata),
        .policy = policy,
        .uses_submission_effects = false,
        .bind = std::move(bind),
    };
}

} // namespace xen::catalog_detail

namespace xen
{
namespace command_dsl = catalog_detail;
}
