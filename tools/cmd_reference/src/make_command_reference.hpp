#include <cstddef>
#include <regex>
#include <sstream>
#include <string>
#include <tuple>

#include <xen/command.hpp>

template <typename ID_t, typename Fn, typename... Args>
[[nodiscard]] auto make_command_reference(xen::Command<ID_t, Fn, Args...> const &cmd,
                                          std::string name, std::string signature)
    -> std::string
{
    auto const cmd_signature = xen::generate_display(cmd.signature);
    name += cmd_signature.id;
    signature += cmd_signature.id;
    for (auto const &arg : cmd_signature.arguments)
    {
        signature += (" " + arg);
    }

    auto description = cmd.description;
    description = std::regex_replace(description, std::regex{"\n"}, "<br>");
    auto const result = name + " | " + signature + "` | " + description + "\n";
    return result;
}

template <typename Command_t>
[[nodiscard]] auto make_command_reference(xen::PatternPrefix<Command_t> const &,
                                          std::string name, std::string signature)
    -> std::string;

template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto make_command_reference(
    xen::CommandGroup<ID_t, ChildID_t, Commands...> const &cmd_group, std::string name,
    std::string signature) -> std::string
{
    name += std::string{cmd_group.id} + " ";
    signature += std::string{cmd_group.id} + " ";
    return std::apply(
        [&](auto const &...child_cmds) {
            return (... + make_command_reference(child_cmds, name, signature));
        },
        cmd_group.commands.commands);
}

template <typename Command_t>
[[nodiscard]] auto make_command_reference(xen::PatternPrefix<Command_t> const &pattern,
                                          std::string name, std::string signature)
    -> std::string
{
    signature += "[Pattern] ";
    return make_command_reference(pattern.command, name, signature);
}

template <typename ID_t, typename ChildID_t, typename... Commands>
[[nodiscard]] auto make_command_reference_table(
    xen::CommandGroup<ID_t, ChildID_t, Commands...> const &cmd_group) -> std::string
{
    return "name | signature | description\n"
           "---- | --------- | -----------\n" +
           make_command_reference(cmd_group, "", "`");
}

// template <typename ID_t, typename Fn, typename... Args>
// [[nodiscard]] auto make_command_reference(xen::Command<ID_t, Fn, Args...> const &cmd,
//                                           std::size_t depth, bool add_heading)
//     -> std::string
// {
//     auto const signature = xen::generate_display(cmd.signature);
//     auto result = (add_heading ? std::string(depth, '#') + " `" : "") + signature.id;
//     for (auto const &arg : signature.arguments)
//     {
//         result += (" " + arg);
//     }
//     result += "`\n\n" + cmd.description + "\n\n";
//     return result;
// }

// template <typename Command_t>
// [[nodiscard]] auto make_command_reference(xen::PatternPrefix<Command_t> const &,
//                                           std::size_t, bool) -> std::string;

// template <typename ID_t, typename ChildID_t, typename... Commands>
// [[nodiscard]] auto make_command_reference(
//     xen::CommandGroup<ID_t, ChildID_t, Commands...> const &cmd_group, std::size_t
//     depth, bool add_heading = true) -> std::string
// {
//     auto oss = std::ostringstream{};
//     if (depth != 1)
//     {
//         oss << (add_heading ? std::string(depth, '#') + " `" : "") << cmd_group.id
//             << " [" << xen::arg_info_to_string(cmd_group.commands.id_info) <<
//             "]`\n\n";
//     }
//     return oss.str() +
//            std::apply(
//                [&](auto const &...child_cmds) {
//                    return (... + make_command_reference(child_cmds, depth + 1,
//                    true));
//                },
//                cmd_group.commands.commands);
// }

// template <typename Command_t>
// [[nodiscard]] auto make_command_reference(xen::PatternPrefix<Command_t> const
// &pattern,
//                                           std::size_t depth, bool add_heading)
//     -> std::string
// {
//     return (add_heading ? std::string(depth, '#') + " `" : "") + "[Pattern] " +
//            make_command_reference(pattern.command, depth + 1, false);
// }