#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace xen
{

/**
 *  Represents a command for the command line system.
 *
 *  Each Command has a unique name, a documentation string and a function that is
 *  called when the command is executed.
 *
 *  Constraints for a valid Command object:
 *  - Name must be an alpha-only string with no spaces and all lowercase.
 *  - Documentation must not be an empty string.
 *  - Function must be a valid callable object.
 */
struct Command
{
    /// The unique name of the command.
    std::string name;

    /// The documentation string of the command.
    std::string documentation;

    /// The function that is called when the command is executed.
    std::function<std::string(std::vector<std::string> const &)> function;
};

/**
 *  A command line system.
 *
 *  This class allows adding commands, matching commands based on input and
 *  executing commands.
 */
class CommandCore
{
  public:
    /**
     *  Adds a command to the system.
     *
     *  @param cmd The command to be added.
     *  @throws std::invalid_argument if the command name is not an alpha-only
     *          string, if the command documentation is an empty string or if the
     *          command function is not a valid callable object.
     *  @throws std::runtime_error if the command name already exists in the system.
     */
    auto add_command(Command const &cmd) -> void
    {
        // Validate command name
        if (cmd.name.empty() ||
            !std::all_of(std::cbegin(cmd.name), std::cend(cmd.name), ::isalpha) ||
            !std::all_of(std::cbegin(cmd.name), std::cend(cmd.name), ::islower))
        {
            throw std::invalid_argument("Command name must be an alpha-only, all "
                                        "lowercase string with no spaces.");
        }

        // Validate documentation
        if (cmd.documentation.empty())
        {
            throw std::invalid_argument(
                "Command documentation cannot be an empty string.");
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

    /**
     *  Tries to match a command based on the provided input.
     *
     *  @param input The input string.
     *  @return The documentation string of the matched command or nullopt if no
     *          command matches, only returns non null if there is a single match.
     */
    [[nodiscard]] auto match_command(std::string const &input) const
        -> std::optional<std::string>
    {
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
            return commands.at(matches[0]).documentation;
        }

        return std::nullopt;
    }

    /**
     *  Executes a command.
     *
     *  @param input The input string.
     *  @return The result of the command execution.
     *  @throws std::runtime_error if the command does not exist.
     */
    [[nodiscard]] auto execute_command(std::string const &input) const -> std::string
    {
        std::istringstream iss(input);
        std::string command_name;
        std::getline(iss, command_name, ' ');

        auto it = commands.find(command_name);
        if (it == commands.end())
        {
            throw std::runtime_error("Command not found");
        }

        std::vector<std::string> params;
        std::string param;
        while (std::getline(iss, param, ' '))
        {
            params.push_back(param);
        }

        return it->second.function(params);
    }

  private:
    /// Map of command names to Command objects
    std::map<std::string, Command> commands;
};

} // namespace xen