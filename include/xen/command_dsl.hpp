#pragma once

#include <algorithm>
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
#include <xen/command_transaction.hpp>

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
    std::vector<CatalogArgumentConstraint> constraints{};
};

template <typename T>
struct ArgTraits;

template <>
struct ArgTraits<int>
{
    static constexpr auto type_name = "integer";
};

template <>
struct ArgTraits<std::size_t>
{
    static constexpr auto type_name = "unsigned_integer";
};

template <>
struct ArgTraits<float>
{
    static constexpr auto type_name = "number";
};

template <>
struct ArgTraits<double>
{
    static constexpr auto type_name = "number";
};

template <>
struct ArgTraits<bool>
{
    static constexpr auto type_name = "boolean";
};

template <>
struct ArgTraits<std::string>
{
    static constexpr auto type_name = "string";
};

template <>
struct ArgTraits<sequence::TimeSignature>
{
    static constexpr auto type_name = "time_signature";
};

template <>
struct ArgTraits<std::variant<int, Modulator>>
{
    static constexpr auto type_name = "integer | modulator";
};

template <>
struct ArgTraits<std::variant<float, Modulator>>
{
    static constexpr auto type_name = "number | modulator";
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
auto constrained(ArgDef<T> argument, CatalogArgumentConstraint metadata,
                 Validator validator, std::string error_message) -> ArgDef<T>
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
    argument.constraints.push_back(std::move(metadata));
    return argument;
}

template <typename T>
auto ranged(ArgDef<T> argument, double minimum, double maximum,
            std::string error_message) -> ArgDef<T>
{
    auto const min_value = static_cast<T>(minimum);
    auto const max_value = static_cast<T>(maximum);
    return constrained(
        std::move(argument),
        CatalogArgumentConstraint{
            .kind = "range", .minimum = minimum, .maximum = maximum, .values = {}},
        [min_value, max_value](T const &value) {
            return value >= min_value && value <= max_value;
        },
        std::move(error_message));
}

template <typename T>
auto minimum(ArgDef<T> argument, double minimum_value, std::string error_message)
    -> ArgDef<T>
{
    auto const min_value = static_cast<T>(minimum_value);
    return constrained(
        std::move(argument),
        CatalogArgumentConstraint{.kind = "minimum",
                                  .minimum = minimum_value,
                                  .maximum = std::nullopt,
                                  .values = {}},
        [min_value](T const &value) { return value >= min_value; },
        std::move(error_message));
}

inline auto positive_float(ArgDef<float> argument, std::string error_message)
    -> ArgDef<float>
{
    return constrained(
        std::move(argument),
        CatalogArgumentConstraint{
            .kind = "minimum", .minimum = 0.0, .maximum = std::nullopt, .values = {}},
        [](float value) { return value > 0.f; }, std::move(error_message));
}

template <typename T>
auto one_of(ArgDef<T> argument, std::vector<T> values, std::string error_message)
    -> ArgDef<T>
{
    auto display_values = std::vector<std::string>{};
    display_values.reserve(values.size());
    for (auto const &value : values)
    {
        display_values.push_back(format_default_value(value));
    }

    return constrained(
        std::move(argument),
        CatalogArgumentConstraint{.kind = "one_of",
                                  .minimum = std::nullopt,
                                  .maximum = std::nullopt,
                                  .values = std::move(display_values)},
        [values = std::move(values)](T const &value) {
            return std::ranges::contains(values, value);
        },
        std::move(error_message));
}

template <typename Scalar>
auto scalar_or_modulator_range(ArgDef<std::variant<Scalar, Modulator>> argument,
                               double minimum, double maximum,
                               std::string error_message)
    -> ArgDef<std::variant<Scalar, Modulator>>
{
    auto const min_value = static_cast<Scalar>(minimum);
    auto const max_value = static_cast<Scalar>(maximum);
    return constrained(
        std::move(argument),
        CatalogArgumentConstraint{
            .kind = "range", .minimum = minimum, .maximum = maximum, .values = {}},
        [min_value, max_value](std::variant<Scalar, Modulator> const &value) {
            if (auto const scalar = std::get_if<Scalar>(&value); scalar != nullptr)
            {
                return *scalar >= min_value && *scalar <= max_value;
            }
            return true;
        },
        std::move(error_message));
}

template <typename Scalar>
auto scalar_or_modulator_minimum(ArgDef<std::variant<Scalar, Modulator>> argument,
                                 double minimum_value, std::string error_message)
    -> ArgDef<std::variant<Scalar, Modulator>>
{
    auto const min_value = static_cast<Scalar>(minimum_value);
    return constrained(
        std::move(argument),
        CatalogArgumentConstraint{.kind = "minimum",
                                  .minimum = minimum_value,
                                  .maximum = std::nullopt,
                                  .values = {}},
        [min_value](std::variant<Scalar, Modulator> const &value) {
            if (auto const scalar = std::get_if<Scalar>(&value); scalar != nullptr)
            {
                return *scalar > min_value;
            }
            return true;
        },
        std::move(error_message));
}

inline auto note_pitch_arg(std::string name, int default_value)
{
    return optional_arg<int>("pitch", std::move(name), default_value);
}

inline auto note_pitch_or_modulator_arg(std::string name,
                                        std::variant<int, Modulator> default_value)
{
    return optional_arg<std::variant<int, Modulator>>(
        "pitch | modulator", std::move(name), std::move(default_value));
}

inline auto pitch_offset_arg(std::string name, int default_value)
{
    return optional_arg<int>("pitch_offset", std::move(name), default_value);
}

inline auto octave_arg(std::string name, int default_value)
{
    return optional_arg<int>("octave", std::move(name), default_value);
}

inline auto octave_offset_arg(std::string name, int default_value)
{
    return optional_arg<int>("octave_offset", std::move(name), default_value);
}

inline auto unit_interval_arg(std::string type, std::string name, float default_value)
{
    return ranged(optional_arg<float>(std::move(type), std::move(name), default_value),
                  0.0, 1.0, "Must be in range [0, 1].");
}

inline auto unit_interval_modulator_arg(std::string type, std::string name,
                                        std::variant<float, Modulator> default_value)
{
    return scalar_or_modulator_range(
        optional_arg<std::variant<float, Modulator>>(std::move(type), std::move(name),
                                                     std::move(default_value)),
        0.0, 1.0, "Must be in range [0, 1].");
}

inline auto unit_interval_delta_arg(std::string type, std::string name,
                                    float default_value)
{
    return ranged(optional_arg<float>(std::move(type), std::move(name), default_value),
                  -1.0, 1.0, "Must be in range [-1, 1].");
}

inline auto velocity_arg(std::string name, float default_value)
{
    return unit_interval_arg("velocity", std::move(name), default_value);
}

inline auto velocity_or_modulator_arg(std::string name,
                                      std::variant<float, Modulator> default_value)
{
    return unit_interval_modulator_arg("velocity | modulator", std::move(name),
                                       std::move(default_value));
}

inline auto velocity_offset_arg(std::string name, float default_value)
{
    return unit_interval_delta_arg("velocity_offset", std::move(name), default_value);
}

inline auto delay_arg(std::string name, float default_value)
{
    return unit_interval_arg("delay", std::move(name), default_value);
}

inline auto delay_or_modulator_arg(std::string name,
                                   std::variant<float, Modulator> default_value)
{
    return unit_interval_modulator_arg("delay | modulator", std::move(name),
                                       std::move(default_value));
}

inline auto delay_offset_arg(std::string name, float default_value)
{
    return unit_interval_delta_arg("delay_offset", std::move(name), default_value);
}

inline auto gate_arg(std::string name, float default_value)
{
    return unit_interval_arg("gate", std::move(name), default_value);
}

inline auto gate_or_modulator_arg(std::string name,
                                  std::variant<float, Modulator> default_value)
{
    return unit_interval_modulator_arg("gate | modulator", std::move(name),
                                       std::move(default_value));
}

inline auto gate_offset_arg(std::string name, float default_value)
{
    return unit_interval_delta_arg("gate_offset", std::move(name), default_value);
}

inline auto frequency_hz_arg(std::string name, float default_value)
{
    return ranged(optional_arg<float>("frequency_hz", std::move(name), default_value),
                  20.0, 20000.0, "Must be in range [20, 20000] Hz.");
}

inline auto transpose_key_arg(std::string name, int default_value)
{
    return ranged(optional_arg<int>("transpose_key", std::move(name), default_value),
                  -127.0, 127.0, "Must be in range [-127, 127].");
}

inline auto positive_weight_arg(std::string type, std::string name)
{
    return positive_float(required_arg<float>(std::move(type), std::move(name)),
                          "Must be greater than 0.");
}

inline auto positive_weight_or_modulator_arg(std::string name)
{
    return scalar_or_modulator_minimum(required_arg<std::variant<float, Modulator>>(
                                           "cell_weight | modulator", std::move(name)),
                                       0.0, "Must be greater than 0.");
}

inline auto repeat_count_arg(std::string name, std::size_t default_value)
{
    return minimum(
        optional_arg<std::size_t>("repeat_count", std::move(name), default_value), 1.0,
        "Must be at least 1.");
}

inline auto scale_mode_arg(std::string name)
{
    return minimum(required_arg<std::size_t>("scale_mode", std::move(name)), 1.0,
                   "Must be at least 1.");
}

inline auto direction_arg(std::string name, int default_value)
{
    return one_of(optional_arg<int>("direction", std::move(name), default_value),
                  std::vector<int>{-1, 1}, "Must be -1 or 1.");
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
                 .kind = defs.type,
                 .display_name = defs.name,
                 .required = !defs.default_text.has_value(),
                 .default_value = defs.default_text,
                 .constraints = defs.constraints,
             }),
             ...);
        },
        arg_defs);

    return metadata;
}

template <typename Handler, typename... Ts>
auto command_with_options(std::vector<std::string> path, bool accepts_pattern_prefix,
                          std::string description, std::vector<std::string> keywords,
                          CommandPolicy policy, std::tuple<ArgDef<Ts>...> arg_defs,
                          Handler handler) -> CommandDefinition
{
    auto metadata = CatalogCommandMetadata{
        .path = std::move(path),
        .keywords = std::move(keywords),
        .accepts_pattern_prefix = accepts_pattern_prefix,
        .target = policy.target,
        .arguments = to_metadata_args(arg_defs),
        .description = std::move(description),
    };

    auto const uses_submission_effects = policy.files != FileAccess::None;
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
                [handler, invocation, parsed_args = std::move(parsed_args),
                 policy](CommandTransaction &transaction,
                         CommandExecutionContext &execution_context) {
                    auto context =
                        transaction.make_handler_context(policy, execution_context);
                    return std::apply(
                        [&](auto const &...values) -> CommandApplicationResult {
                            if constexpr (std::is_invocable_r_v<
                                              CommandApplicationResult, Handler,
                                              CommandHandlerContext &,
                                              CommandInvocation const &, Ts const &...>)
                            {
                                return handler(context, invocation, values...);
                            }
                            else if constexpr (std::is_invocable_r_v<
                                                   CommandStatus, Handler,
                                                   CommandHandlerContext &,
                                                   CommandInvocation const &,
                                                   Ts const &...>)
                            {
                                return CommandApplicationResult{
                                    .status = handler(context, invocation, values...),
                                    .suggested_selection = std::nullopt,
                                };
                            }
                            else
                            {
                                static_assert([] { return false; }(),
                                              "Unsupported command handler signature.");
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
                                std::move(description), {}, policy, std::move(arg_defs),
                                std::move(handler));
}

template <typename Handler, typename... Ts>
auto command(std::vector<std::string> path, bool accepts_pattern_prefix,
             std::string description, std::vector<std::string> keywords,
             CommandPolicy policy, std::tuple<ArgDef<Ts>...> arg_defs, Handler handler)
    -> CommandDefinition
{
    return command_with_options(std::move(path), accepts_pattern_prefix,
                                std::move(description), std::move(keywords), policy,
                                std::move(arg_defs), std::move(handler));
}

inline auto replay_command(std::vector<std::string> path, std::string description,
                           CommandPolicy policy, std::vector<std::string> keywords = {})
    -> CommandDefinition
{
    auto metadata = CatalogCommandMetadata{
        .path = std::move(path),
        .keywords = std::move(keywords),
        .target = policy.target,
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

inline auto history_navigation_command(std::vector<std::string> path,
                                       std::string description, CommandPolicy policy,
                                       HistoryNavigationDirection direction,
                                       std::vector<std::string> keywords = {})
    -> CommandDefinition
{
    auto metadata = CatalogCommandMetadata{
        .path = std::move(path),
        .keywords = std::move(keywords),
        .target = policy.target,
        .description = std::move(description),
    };

    auto bind = [policy, direction](CommandInvocation const &invocation,
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

        return ExecutableHistoryNavigation{
            .canonical = invocation.canonical_segment,
            .policy = policy,
            .direction = direction,
        };
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
