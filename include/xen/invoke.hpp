#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <xen/message_level.hpp>
#include <xen/parse_args.hpp>
#include <xen/signature.hpp>
#include <xen/xen_timeline.hpp>

namespace xen
{

/**
 * @brief Invoke a function with arguments parsed from a vector of strings.
 *
 * @param fn The function to invoke.
 * @param tl The timeline to pass to the function.
 * @param args The arguments to parse.
 * @param arg_infos The argument infos describing the expected parameters.
 * @return auto The return value of the function.
 */
template <typename Fn, typename... Args>
auto invoke_with_args(Fn fn, XenTimeline &tl, std::string const &args,
                      ArgInfos<Args...> const &arg_infos)
    -> std::pair<MessageLevel, std::string>
{
    static_assert(std::is_invocable_r_v<std::pair<MessageLevel, std::string>, Fn,
                                        XenTimeline &, Args...>,
                  "Invalid function type.");

    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        auto const splits = split_args(args);
        return fn(tl, get_argument_value<I>(splits, std::get<I>(arg_infos))...);
    }(std::index_sequence_for<Args...>{});
}

/**
 * @brief Invoke a function with arguments parsed from a vector of strings.
 *
 * Specialized for functions that take a pattern as the second argument.
 *
 * @param fn The function to invoke.
 * @param tl The timeline to pass to the function.
 * @param pattern The pattern to pass to the function.
 * @param args The arguments to parse.
 * @param arg_infos The argument infos describing the expected parameters.
 * @return auto The return value of the function.
 */
template <typename Fn, typename... Args>
auto invoke_with_args(Fn fn, XenTimeline &tl, sequence::Pattern const &pattern,
                      std::string const &args, ArgInfos<Args...> const &arg_infos)
    -> std::pair<MessageLevel, std::string>
{
    static_assert(
        std::is_invocable_r_v<std::pair<MessageLevel, std::string>, Fn, XenTimeline &,
                              sequence::Pattern const &, Args...>,
        "Invalid function type.");

    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        auto const splits = split_args(args);
        return fn(tl, pattern,
                  get_argument_value<I>(splits, std::get<I>(arg_infos))...);
    }(std::index_sequence_for<Args...>{});
}

} // namespace xen
