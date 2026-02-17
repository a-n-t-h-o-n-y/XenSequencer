#pragma once

#include <cstddef>
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

#include "command_catalog_metadata_internal.hpp"

namespace xen::catalog_detail
{

template <typename T>
struct ArgDef
{
    std::string type;
    std::string name;
    std::optional<T> default_value{};
    std::optional<std::string> default_text{};
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
auto optional_arg(std::string type, std::string name, T default_value,
                  std::optional<std::string> default_text = std::nullopt)
    -> ArgDef<T>
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
auto parse_arg(std::vector<std::string> const &words, std::size_t index,
               ArgDef<T> const &arg) -> T
{
    if (index >= words.size())
    {
        if (arg.default_value.has_value())
        {
            return *arg.default_value;
        }

        throw CatalogBindException{CatalogBindErrorKind::MissingArgument, arg.name,
                                   ""};
    }

    try
    {
        return parse<T>(words[index]);
    }
    catch (std::exception const &e)
    {
        throw CatalogBindException{CatalogBindErrorKind::InvalidArgument, arg.name,
                                   e.what()};
    }
}

template <typename... Ts, std::size_t... Is>
auto parse_args_impl(std::tuple<ArgDef<Ts>...> const &arg_defs,
                     std::vector<std::string> const &words,
                     std::size_t arg_offset,
                     std::index_sequence<Is...>) -> std::tuple<Ts...>
{
    return std::tuple<Ts...>{
        parse_arg(words, arg_offset + Is, std::get<Is>(arg_defs))...,
    };
}

template <typename... Ts>
auto parse_args_tuple(std::tuple<ArgDef<Ts>...> const &arg_defs,
                      std::vector<std::string> const &words,
                      std::size_t arg_offset) -> std::tuple<Ts...>
{
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

template <typename Binder, typename... Ts>
auto make_spec(std::vector<std::string> path, bool accepts_pattern_prefix,
               std::string description, std::tuple<ArgDef<Ts>...> arg_defs,
               Binder binder) -> CommandSpec
{
    auto metadata = CatalogCommandMetadata{};
    metadata.path = std::move(path);
    metadata.accepts_pattern_prefix = accepts_pattern_prefix;
    metadata.arguments = to_metadata_args(arg_defs);
    metadata.description = std::move(description);

    auto bind_fn =
        [arg_defs = std::move(arg_defs),
         binder = std::move(binder)](CommandInvocation const &invocation,
                                     std::size_t arg_offset) {
            auto const parsed_args =
                parse_args_tuple(arg_defs, invocation.input.words, arg_offset);

            auto action = std::apply(
                [&](auto const &...values) { return binder(invocation, values...); },
                parsed_args);

            return CommandAction{std::move(action)};
        };

    return CommandSpec{
        .metadata = std::move(metadata),
        .bind = std::move(bind_fn),
    };
}

} // namespace xen::catalog_detail
