#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "xen_timeline.hpp"

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
 *  - Signature must not be an empty string.
 *  - Documentation must not be an empty string.
 *  - Function must be a valid callable object.
 */
struct Command
{
    /// The unique name of the command.
    std::string name;

    /// The function signature, show all command name and all parameters.
    std::string signature;

    /// The documentation string of the command.
    std::string documentation;

    /// The function that is called when the command is executed.
    std::function<std::string(XenTimeline &, std::vector<std::string> const &)>
        function;
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
    explicit CommandCore(XenTimeline &t);

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
    auto add_command(Command const &cmd) -> void;

    /**
     *  Tries to match a command to its signature based on the provided substring input.
     *
     *  @param input The input string.
     *  @return The signature string of the matched command or nullopt if no
     *          command matches, only returns non null if there is a single match.
     */
    [[nodiscard]] auto match_command(std::string input) const
        -> std::optional<std::string>;

    /**
     *  Executes a command.
     *
     *  @param input The input string.
     *  @return The result of the command execution.
     *  @throws std::runtime_error if the command does not exist.
     */
    [[nodiscard]] auto execute_command(std::string const &input) const -> std::string;

  private:
    /// Map of command names to Command objects
    std::map<std::string, Command> commands;

    XenTimeline &timeline_;
};

} // namespace xen