#pragma once

#include <concepts>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <sequence/pattern.hpp>
#include <sequence/utility.hpp>

#include <xen/message_type.hpp>
#include <xen/parse_args.hpp>
#include <xen/signature.hpp>
#include <xen/string_manip.hpp>
#include <xen/utility.hpp>
#include <xen/xen_timeline.hpp>

namespace xen
{

/**
 * @brief Normalize a command string so it can be parsed easily.
 */
[[nodiscard]] inline auto normalize_command_string(std::string const &command_str)
    -> std::string
{
    return to_lower(minimize_spaces(command_str));
}

[[nodiscard]] inline auto normalize_id(std::string const &x) -> std::string
{
    return to_lower(x);
}

[[nodiscard]] inline auto normalize_id(std::string_view x) -> std::string
{
    return to_lower(std::string{x});
}

template <typename T>
[[nodiscard]] inline auto normalize_id(T const &x) -> T
{
    return x;
}

template <typename ID_t, typename Fn, typename... Args>
struct Command
{
    static_assert(!std::is_same_v<ID_t, char const *>,
                  "ID_t cannot be a string literal.");
    using ID_type = ID_t;

    Signature<ID_t, Args...> signature;
    Fn fn;
};

template <typename ID_t, typename... Commands>
struct CommandList
{
    using ID_type = ID_t;

    ArgInfo<ID_t> id_info;
    std::tuple<Commands...> commands;
};

template <typename ID_t, typename ChildID_t, typename... Commands>
struct CommandGroup
{
    static_assert(!std::is_same_v<ID_t, char const *>,
                  "ID_t cannot be a string literal.");
    using ID_type = ID_t;

    ID_t id;
    CommandList<ChildID_t, Commands...> commands;
};

template <typename Command_t>
struct PatternPrefix
{
    Command_t command;
};

} // namespace xen

#include <xen/invoke.hpp>

namespace xen
{

// -----------------------------------------------------------------------------

/**
 * Convinience function for creating a command.
 */
template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] constexpr auto cmd(ID_t id, Fn fn, ArgInfo<Args>... arg_infos)
{
    if constexpr (std::is_same_v<ID_t, char const *>)
    {
        return Command<std::string_view, Fn, Args...>{
            Signature<std::string_view, Args...>{
                id, ArgInfos<Args...>{std::move(arg_infos)...}},
            std::move(fn)};
    }
    else
    {
        return Command<ID_t, Fn, Args...>{
            Signature<ID_t, Args...>{std::move(id),
                                     ArgInfos<Args...>{std::move(arg_infos)...}},
            std::move(fn)};
    }
}

/**
 * Convinience function for creating a command list.
 */
template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] constexpr auto cmd_group(ID_t id, ArgInfo<ChildID_t> child_id_info,
                                       Commands... commands)
{
    if constexpr (std::is_same_v<ID_t, char const *>)
    {
        return CommandGroup<std::string_view, ChildID_t, Commands...>{
            id, CommandList<ChildID_t, Commands...>{
                    std::move(child_id_info), std::tuple{std::move(commands)...}}};
    }
    else
    {
        return CommandGroup<ID_t, ChildID_t, Commands...>{
            std::move(id),
            CommandList<ID_t, Commands...>{std::move(child_id_info),
                                           std::tuple{std::move(commands)...}}};
    }
}

/**
 * Convinience function for creating a PatternPrefix.
 */
template <typename Command_t>
[[nodiscard]] constexpr auto pattern(Command_t command) -> PatternPrefix<Command_t>
{
    return {std::move(command)};
}

// -----------------------------------------------------------------------------

/**
 * Check if a Command matches an ID.
 */
template <typename ID_t, typename Fn, typename... Args, typename T>
    requires std::equality_comparable_with<T, ID_t>
[[nodiscard]] auto is_match(Command<ID_t, Fn, Args...> const &command,
                            std::string const &command_str,
                            std::optional<T> const &default_id) -> bool
{
    // Parse out ID from command_str, using default value if no ID is given.
    auto const id_str = get_first_word(command_str);
    if (id_str.empty())
    {
        if (default_id.has_value())
        {
            return default_id.value() == command.signature.id;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return normalize_id(parse<ID_t>(id_str)) == normalize_id(command.signature.id);
    }
}

/**
 * Check if a CommandGroup matches an ID.
 */
template <typename ID_t, typename ChildID_t, typename... Args, typename T>
    requires std::equality_comparable_with<T, ID_t>
[[nodiscard]] auto is_match(CommandGroup<ID_t, ChildID_t, Args...> const &command_group,
                            std::string const &command_str,
                            std::optional<T> const &default_id) -> bool
{
    // Parse out ID from command_str, using default value if no ID is given.
    auto const id_str = get_first_word(command_str);
    if (id_str.empty())
    {
        if (default_id.has_value())
        {
            return default_id.value() == command_group.id;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return normalize_id(parse<ID_t>(id_str)) == normalize_id(command_group.id);
    }
}

/**
 * Check if a PatternPrefix's Command matches an ID.
 */
template <typename Command_t, typename T>
[[nodiscard]] auto is_match(PatternPrefix<Command_t> const &pattern,
                            std::string command_str, std::optional<T> const &default_id)
    -> bool
{
    return sequence::contains_valid_pattern(command_str) &&
           is_match(pattern.command, sequence::pop_pattern_chars(command_str),
                    default_id);
}

// -----------------------------------------------------------------------------

template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] auto get_signature_display(Command<ID_t, Fn, Args...> const &command)
    -> SignatureDisplay
{
    return generate_display(command.signature);
}

template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto get_signature_display(
    CommandGroup<ID_t, ChildID_t, Commands...> const &command_group) -> SignatureDisplay
{
    return generate_display(Signature<ID_t, ChildID_t>{
        command_group.id, ArgInfos<ChildID_t>{command_group.commands.id_info}});
}

template <typename Command_t>
[[nodiscard]] auto get_signature_display(PatternPrefix<Command_t> const &pattern)
    -> SignatureDisplay
{
    return get_signature_display(pattern.command);
}

// -----------------------------------------------------------------------------

/**
 * @brief Execute a Command object.
 *
 * @param command The command to execute.
 * @param tl The timeline to execute the command on.
 * @param command_str The command string to parse, this will only consist of arguments
 * to the command, the name will be stripped off before this function is called.
 * @return std::pair<MessageType, std::string> The message type and message string.
 *
 * @exception std::invalid_argument Thrown when the command string does not match the
 * command's signature.
 */
template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] auto execute(Command<ID_t, Fn, Args...> const &command, XenTimeline &tl,
                           std::string command_str)
    -> std::pair<MessageType, std::string>
{
    try
    {
        command_str = pop_first_word(command_str);
        return invoke_with_args(command.fn, tl, command_str, command.signature.args);
    }
    catch (std::exception const &e)
    {
        return {MessageType::Error, e.what()};
    }
}

/**
 * @brief Execute a Command object that takes a PatternPrefix.
 *
 * @param command The command to execute.
 * @param tl The timeline to execute the command on.
 * @param command_str The command string to parse, this will only consist of arguments
 * to the command, the name will be stripped off before this function is called.
 * @param pattern The pattern to use for iteration over Sequences.
 * @return std::pair<MessageType, std::string> The message type and message string.
 *
 * @exception std::invalid_argument Thrown when the command string does not match the
 * command's signature.
 */
template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] auto execute(Command<ID_t, Fn, Args...> const &command, XenTimeline &tl,
                           std::string command_str, sequence::Pattern pattern)
    -> std::pair<MessageType, std::string>
{
    try
    {
        command_str = pop_first_word(command_str);
        return invoke_with_args(command.fn, tl, pattern, command_str,
                                command.signature.args);
    }
    catch (std::exception const &e)
    {
        return {MessageType::Error, e.what()};
    }
}

/**
 * @brief Execute a CommandGroup object.
 *
 * This will pop off the first word of the command_str and use it to find the matching
 * command in the CommandGroup, then it will forward the rest of the command_str to the
 * command.
 *
 * @param command_group The command group to execute.
 * @param tl The timeline to execute the command on.
 * @param command_str The command string to parse.
 * @return std::pair<MessageType, std::string> The message type and message string.
 *
 * @exception std::invalid_argument Thrown when the command string does not match the
 * command's signature.
 * @exception std::runtime_error Thrown when no command with the given ID is found.
 */
template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto execute(
    CommandGroup<ID_t, ChildID_t, Commands...> const &command_group, XenTimeline &tl,
    std::string command_str) -> std::pair<MessageType, std::string>
{
    try
    {
        // Don't pop off first word if CommandGroup ID is empty.
        if constexpr (std::is_same_v<ID_t, std::string_view>)
        {
            if (!command_group.id.empty())
            {
                command_str = pop_first_word(command_str);
            }
        }
        else
        {
            command_str = pop_first_word(command_str);
        }

        return apply_if<std::pair<MessageType, std::string>>(
            [&](auto const &command) {
                return is_match(command, command_str,
                                command_group.commands.id_info.default_value);
            },
            [&](auto const &command) { return execute(command, tl, command_str); },
            command_group.commands.commands);
    }
    catch (ErrorNoMatch const &)
    {
        return {MessageType::Error, "Command Not Found: " + command_str};
    }
    catch (std::exception const &e)
    {
        return {MessageType::Error, e.what()};
    }
}

/**
 * @brief Execute a CommandGroup object that takes a PatternPrefix.
 *
 * This will pop off the first word of the command_str and use it to find the matching
 * command in the CommandGroup, then it will forward the rest of the command_str to the
 * command.
 *
 * @param command_group The command group to execute.
 * @param tl The timeline to execute the command on.
 * @param command_str The command string to parse.
 * @param pattern The pattern to use for iteration over Sequences.
 * @return std::pair<MessageType, std::string> The message type and message string.
 *
 * @exception std::invalid_argument Thrown when the command string does not match the
 * command's signature.
 * @exception std::runtime_error Thrown when no command with the given ID is found.
 */
template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto execute(
    CommandGroup<ID_t, ChildID_t, Commands...> const &command_group, XenTimeline &tl,
    std::string command_str, sequence::Pattern pattern)
    -> std::pair<MessageType, std::string>
{
    try
    {
        if constexpr (std::is_same_v<ID_t, std::string_view>)
        {
            if (!command_group.id.empty())
            {
                command_str = pop_first_word(command_str);
            }
        }
        else
        {
            command_str = pop_first_word(command_str);
        }
        return apply_if<std::pair<MessageType, std::string>>(
            [&](auto const &command) {
                return is_match(command, command_str,
                                command_group.commands.id_info.default_value);
            },
            [&](auto const &command) {
                return execute(command, tl, command_str, std::move(pattern));
            },
            command_group.commands.commands);
    }
    catch (ErrorNoMatch const &)
    {
        return {MessageType::Error, "Command Not Found: " + command_str};
    }
    catch (std::exception const &e)
    {
        return {MessageType::Error, e.what()};
    }
}

/**
 * @brief Execute a PatternPrefix object.
 *
 * This will parse a Pattern from the front of the command string and pass the Pattern
 * and the remaining command string on to its child command. A pattern string can be
 * empty and will default to Pattern{0, {1}}.
 *
 * @param pattern The pattern prefix to execute.
 * @param tl The timeline to execute the command on.
 * @param command_str The command string to parse.
 * @return std::pair<MessageType, std::string> The message type and message string.
 *
 * @exception std::invalid_argument Thrown when the pattern string is invalid.
 */
template <typename Command_t>
[[nodiscard]] auto execute(PatternPrefix<Command_t> const &pattern_cmd, XenTimeline &tl,
                           std::string command_str)
    -> std::pair<MessageType, std::string>
{
    try
    {
        auto pattern = sequence::parse_pattern(command_str);
        command_str = sequence::pop_pattern_chars(command_str);
        return execute(pattern_cmd.command, tl, std::move(command_str),
                       std::move(pattern));
    }
    catch (std::invalid_argument const &e)
    {
        return {MessageType::Error, e.what()};
    }
}

} // namespace xen
