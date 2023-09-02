#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "command.hpp"
#include "message_type.hpp"
#include "signature.hpp"
#include "util.hpp"
#include "xen_timeline.hpp"

namespace xen
{

/**
 *  A command line system.
 *
 *  This class allows adding commands, matching commands based on input and
 *  executing commands.
 */
class CommandCore
{
  public:
    explicit CommandCore(XenTimeline &tl);

  public:
    /**
     *  Adds a command to the system.
     *
     *  @param cmd The command to be added.
     *  @throws std::invalid_argument if cmd is nullptr
     *  @throws std::runtime_error if the command name already exists in the system.
     */
    auto add(std::unique_ptr<CommandBase> cmd) -> void;

    /**
     *  Tries to match a partial command name to its signature string.
     *
     *  @param input The input string.
     *  @return The SignatureDisplay of the matched command or nullopt if no
     *  command matches, only returns non null if there is a single match.
     */
    [[nodiscard]] auto get_matched_signature(std::string const &input) const
        -> std::optional<SignatureDisplay>;

    /**
     *  Tries to match a partial command name to its Command object.
     *
     *  @param input The input string.
     *  @return The CommandBase Pointer of the matched command or nullptr if no
     *  command matches, only returns non null if there is a single match.
     */
    [[nodiscard]] auto get_matched_command(std::string input) const
        -> CommandBase const *;

    /**
     *  Executes a command.
     *
     *  @param input The input string.
     *  @return The result of the command execution.
     *  @throws std::runtime_error if the command does not exist.
     */
    [[nodiscard]] auto execute_command(std::string input) const
        -> std::pair<MessageType, std::string>;

  private:
    /// Map of command names to Command objects
    std::map<std::string, std::unique_ptr<CommandBase>> commands_;

    XenTimeline &timeline_;
};

} // namespace xen