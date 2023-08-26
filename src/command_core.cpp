#include "command_core.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "util.hpp"
#include "xen_timeline.hpp"

namespace xen
{

CommandCore::CommandCore(XenTimeline &t) : timeline_{t}
{
    this->add_command({"help", "help", "Prints this help message.",
                       [this](XenTimeline &, std::vector<std::string> const &) {
                           auto help_message = std::string{"Available commands:\n"};
                           for (auto const &[name, command] : commands)
                           {
                               help_message += name + " - " + command.documentation +
                                               "\n" + command.signature + "\n\n";
                           }
                           return help_message;
                       }});
}

auto CommandCore::add_command(Command const &cmd) -> void
{
    // Validate command name
    if (cmd.name.empty() ||
        !std::all_of(std::cbegin(cmd.name), std::cend(cmd.name), ::isalpha) ||
        !std::all_of(std::cbegin(cmd.name), std::cend(cmd.name), ::islower))
    {
        throw std::invalid_argument("Command name must be an alpha-only, all "
                                    "lowercase string with no spaces.");
    }

    // Validate signature
    if (cmd.signature.empty())
    {
        throw std::invalid_argument("Command signature cannot be an empty string.");
    }

    // Validate documentation
    if (cmd.documentation.empty())
    {
        throw std::invalid_argument("Command documentation cannot be an empty string.");
    }

    // Validate function
    if (!cmd.function)
    {
        throw std::invalid_argument("Command function cannot be null.");
    }

    // Check if command already exists
    if (commands.find(cmd.name) != std::cend(commands))
    {
        throw std::runtime_error("Command with the same name already exists.");
    }

    // Add command to map
    commands[cmd.name] = cmd;
}

auto CommandCore::match_command(std::string input) const -> std::optional<std::string>
{
    input = to_lower(input);
    auto matches = std::vector<std::string>{};
    for (auto const &[name, command] : commands)
    {
        if (name.rfind(input, 0) == 0)
        { // input is a prefix of name
            matches.push_back(name);
        }
    }

    if (matches.size() == 1)
    {
        return commands.at(matches[0]).signature;
    }

    return std::nullopt;
}

auto CommandCore::execute_command(std::string const &input) const -> std::string
{
    auto iss = std::istringstream{input};
    auto command_name = std::string{};
    std::getline(iss, command_name, ' ');
    auto const command_name_lower = to_lower(command_name);

    auto it = commands.find(command_name_lower);
    if (it == commands.end())
    {
        throw std::runtime_error("Command '" + command_name + "' not found");
    }

    auto params = std::vector<std::string>{};
    auto param = std::string{};
    while (std::getline(iss, param, ' '))
    {
        params.push_back(param);
    }

    return it->second.function(timeline_, params);
}

} // namespace xen