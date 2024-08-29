#pragma once

#include <string>
#include <utility>
#include <type_traits>
#include <cstddef>
#include <tuple>

#include <xen/message_level.hpp>
#include <xen/parse_args.hpp>
#include <xen/signature.hpp>
#include <xen/state.hpp>

namespace xen
{

/**
 * Invoke a function with arguments parsed from a vector of strings.
 *
 * @param fn The function to invoke.
 * @param ps The PluginState to pass to the function.
 * @param args The arguments to parse.
 * @param arg_infos The argument infos describing the expected parameters.
 * @return auto The return value of the function.
 */
template <typename Fn, typename... Args>
auto invoke_with_args(Fn fn, PluginState &ps, std::string const &args,
                      ArgInfos<Args...> const &arg_infos)
    -> std::pair<MessageLevel, std::string>
{
    static_assert(std::is_invocable_r_v<std::pair<MessageLevel, std::string>, Fn,
                                        PluginState &, Args...>,
                  "Invalid function type.");

    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        auto const splits = split_args(args);
        return fn(ps, get_argument_value<I>(splits, std::get<I>(arg_infos))...);
    }(std::index_sequence_for<Args...>{});
}

/**
 * Invoke a function with arguments parsed from a vector of strings.
 *
 * @details Specialized for functions that take a pattern as the second argument.
 * @param fn The function to invoke.
 * @param ps The PluginState to pass to the function.
 * @param pattern The pattern to pass to the function.
 * @param args The arguments to parse.
 * @param arg_infos The argument infos describing the expected parameters.
 * @return auto The return value of the function.
 */
template <typename Fn, typename... Args>
auto invoke_with_args(Fn fn, PluginState &ps, sequence::Pattern const &pattern,
                      std::string const &args, ArgInfos<Args...> const &arg_infos)
    -> std::pair<MessageLevel, std::string>
{
    static_assert(
        std::is_invocable_r_v<std::pair<MessageLevel, std::string>, Fn, PluginState &,
                              sequence::Pattern const &, Args...>,
        "Invalid function type.");

    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        auto const splits = split_args(args);
        return fn(ps, pattern,
                  get_argument_value<I>(splits, std::get<I>(arg_infos))...);
    }(std::index_sequence_for<Args...>{});
}

} // namespace xen
