#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace xen
{

/**
 * A class that stores a history of commands.
 *
 * @details This has the concept of a 'current' command, which has no state stored here
 * and returns std::nullopt when retrieved. As commands are added the current command is
 * one past the just added command.
 */
class CommandHistory
{
  public:
    CommandHistory();

  public:
    /**
     * Adds a command to the history and erases all items from the current index to the
     * end.
     *
     * @details If the new command is a duplicate of the last, it is ignored.
     * @param command The command to add to the history.
     * @return None.
     */
    void add_command(std::string const &command);

    /**
     * Returns the previous command and sets the current command to it.
     *
     * @return The previous command string if available; std::nullopt if at the
     * 'current' position.
     */
    [[nodiscard]] auto previous() -> std::optional<std::string>;

    /**
     * Returns the next command and sets the current command to it.
     *
     * @return The next command string if available; std::nullopt if at the 'current'
     * position.
     */
    [[nodiscard]] auto next() -> std::optional<std::string>;

    /**
     * Returns the current command.
     *
     * @return The current command string if available; std::nullopt if at the 'current'
     * position.
     */
    [[nodiscard]] auto get_command() const -> std::optional<std::string>;

  private:
    std::vector<std::string> history_;
    std::size_t current_index_;
};

} // namespace xen