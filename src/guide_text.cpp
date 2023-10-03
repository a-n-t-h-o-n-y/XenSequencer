#include <xen/guide_text.hpp>

#include <algorithm>
#include <cassert>
#include <sstream>
#include <string>

#include <sequence/pattern.hpp>

#include <xen/command.hpp>
#include <xen/signature.hpp>
#include <xen/string_manip.hpp>
#include <xen/utility.hpp>
#include <xen/xen_command_tree.hpp>

namespace
{

using namespace xen;

// --------------------------------------------------------------------------

template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] auto is_complete_match(Command<ID_t, Fn, Args...> const &command,
                                     std::string const &command_str) -> bool
{
    auto const input_id = get_first_word(command_str);
    auto oss = std::ostringstream{};
    oss << command.signature.id;
    return to_lower(input_id) == to_lower(oss.str());
}

template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto is_complete_match(
    CommandGroup<ID_t, ChildID_t, Commands...> const &command_group,
    std::string const &command_str) -> bool
{
    auto const input_id = get_first_word(command_str);
    auto oss = std::ostringstream{};
    oss << command_group.id;
    return to_lower(input_id) == to_lower(oss.str());
}

template <typename Command_t>
[[nodiscard]] auto is_complete_match(PatternPrefix<Command_t> const &pattern,
                                     std::string const &command_str) -> bool
{
    return sequence::contains_valid_pattern(command_str) &&
           is_complete_match(pattern.command, sequence::pop_pattern_chars(command_str));
}

template <typename... Args>
[[nodiscard]] auto has_complete_match(std::string const &partial_command,
                                      std::tuple<Args...> const &commands) -> bool
{
    return std::apply(
               [&](auto &&...args) {
                   int count = 0;
                   ((count += is_complete_match(args, partial_command)), ...);
                   return count;
               },
               commands) == 1;
}

// --------------------------------------------------------------------------

/**
 * @brief Checks if a string is a prefix of another string.
 *
 * An empty string is not a prefix of another.
 *
 * @param candidate The potential prefix of `target`.
 * @param target The string to check against.
 * @return bool True if `candidate` is a prefix of `target`, otherwise false.
 */
[[nodiscard]] auto is_prefix(std::string const &candidate, std::string const &target)
    -> bool
{
    if (candidate.empty())
    {
        return false;
    }
    else
    {
        return target.compare(0, candidate.size(), candidate) == 0;
    }
}

// --------------------------------------------------------------------------

template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] auto is_partial_match(Command<ID_t, Fn, Args...> const &command,
                                    std::string const &command_str) -> bool
{
    // Don't match if there is a trailing space
    if (command_str.empty() || std::isspace(command_str.back()))
    {
        return false;
    }

    auto oss = std::ostringstream{};
    oss << command.signature.id;
    return is_prefix(to_lower(get_first_word(command_str)), to_lower(oss.str()));
}

template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto is_partial_match(
    CommandGroup<ID_t, ChildID_t, Commands...> const &command_group,
    std::string const &command_str) -> bool
{
    // Don't match if there is a trailing space
    if (command_str.empty() || std::isspace(command_str.back()))
    {
        return false;
    }

    auto oss = std::ostringstream{};
    oss << command_group.id;
    return is_prefix(to_lower(get_first_word(command_str)), to_lower(oss.str()));
}

template <typename Command_t>
[[nodiscard]] auto is_partial_match(PatternPrefix<Command_t> const &pattern,
                                    std::string const &command_str) -> bool
{
    return sequence::contains_valid_pattern(command_str) &&
           is_partial_match(pattern.command, sequence::pop_pattern_chars(command_str));
}

template <typename... Args>
[[nodiscard]] auto has_unique_partial_match(std::string const &partial_command,
                                            std::tuple<Args...> const &commands) -> bool
{
    return std::apply(
               [&](auto &&...args) {
                   int count = 0;
                   ((count += is_partial_match(args, partial_command)), ...);
                   return count;
               },
               commands) == 1;
}

// --------------------------------------------------------------------------

template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] auto pop_first_token(Command<ID_t, Fn, Args...> const &,
                                   std::string const &command_str) -> std::string
{
    return pop_first_word(command_str);
}

template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto pop_first_token(CommandGroup<ID_t, ChildID_t, Commands...> const &,
                                   std::string const &command_str) -> std::string
{
    return pop_first_word(command_str);
}

template <typename Command_t>
[[nodiscard]] auto pop_first_token(PatternPrefix<Command_t> const &,
                                   std::string const &command_str) -> std::string
{
    return sequence::pop_pattern_chars(command_str);
}

// --------------------------------------------------------------------------

template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] auto complete_id(Command<ID_t, Fn, Args...> const &command,
                               std::string const &partial_id) -> std::string
{
    auto oss = std::ostringstream{};
    oss << command.signature.id;
    return oss.str().substr(strip(partial_id).size());
}

template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto complete_id(
    CommandGroup<ID_t, ChildID_t, Commands...> const &command_group,
    std::string const &partial_id) -> std::string
{
    auto oss = std::ostringstream{};
    oss << command_group.id;
    return oss.str().substr(strip(partial_id).size());
}

template <typename Command_t>
[[nodiscard]] auto complete_id(PatternPrefix<Command_t> const &, std::string const &)
    -> std::string
{
    return std::string{};
}

// --------------------------------------------------------------------------

// Forward Delcarations
template <typename Command_t>
[[nodiscard]] auto generate_guide_text(PatternPrefix<Command_t> const &pattern,
                                       std::string const &command_str) -> std::string;

template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto generate_guide_text(
    CommandGroup<ID_t, ChildID_t, Commands...> const &command_group,
    std::string const &command_str) -> std::string;

// --------------------------------------------------------------------------

template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] auto generate_guide_text(Command<ID_t, Fn, Args...> const &command,
                                       std::string const &command_str) -> std::string
{
    // Display all ArgInfos beyond the current number of input args.
    auto const input_arg_count = word_count(command_str);
    auto const display = generate_display(command.signature);

    auto oss = std::ostringstream{};
    auto prefix = std::string{
        (!command_str.empty() && std::isspace(command_str.back())) ? "" : " "};
    for (auto i = input_arg_count; i < display.arguments.size(); ++i)
    {
        oss << prefix << display.arguments[i];
        prefix = " ";
    }

    return oss.str();
}

template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto generate_guide_text(
    CommandGroup<ID_t, ChildID_t, Commands...> const &command_group,
    std::string const &command_str) -> std::string
{
    if (strip(command_str).empty())
    {
        auto prefix = std::string{
            (!command_str.empty() && std::isspace(command_str.back())) ? "" : " "};
        return prefix + '[' + arg_info_to_string(command_group.commands.id_info) + ']';
    }

    if (has_complete_match(command_str, command_group.commands.commands))
    {
        return apply_if<std::string>(
            [&](auto const &child_command) {
                return ::is_complete_match(child_command, command_str);
            },
            [&](auto const &child_command) {
                return generate_guide_text(child_command,
                                           pop_first_token(child_command, command_str));
            },
            command_group.commands.commands);
    }
    else if (has_unique_partial_match(command_str, command_group.commands.commands))
    {
        return apply_if<std::string>(
            [&](auto const &child_command) {
                return ::is_partial_match(child_command, command_str);
            },
            [&](auto const &child_command) {
                return ::complete_id(child_command, command_str) +
                       generate_guide_text(child_command,
                                           pop_first_token(child_command, command_str));
            },
            command_group.commands.commands);
    }
    else
    {
        return std::string{};
    }
}

template <typename Command_t>
[[nodiscard]] auto generate_guide_text(PatternPrefix<Command_t> const &pattern,
                                       std::string const &command_str) -> std::string
{
    if (is_complete_match(pattern.command, command_str))
    {
        return generate_guide_text(pattern.command,
                                   pop_first_token(pattern.command, command_str));
    }
    else if (is_partial_match(pattern.command, command_str))
    {
        return ::complete_id(pattern.command, command_str) +
               generate_guide_text(pattern.command,
                                   pop_first_token(pattern.command, command_str));
    }
    else
    {
        return std::string{};
    }
}

} // namespace

namespace xen
{

auto generate_guide_text(std::string const &partial_command) -> std::string
{
    // Don't complete top-level CommandGroup
    if (strip(partial_command).empty())
    {
        return "";
    }
    else
    {
        return ::generate_guide_text(command_tree, partial_command);
    }
}

auto complete_id(std::string const &partial_command) -> std::string
{
    // This could have a proper implementation, but this is enough for now.
    auto const potential = get_first_word(generate_guide_text(partial_command));
    if (!potential.empty() && potential.front() == '[')
    {
        return "";
    }
    else
    {
        return potential;
    }
}

} // namespace xen