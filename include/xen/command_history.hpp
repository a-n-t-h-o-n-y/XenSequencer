#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace xen
{

/**
 * @brief A class that stores a history of commands.
 *
 * This has the concept of a 'current' command, which has no state
 * stored here and returns std::nullopt when retrieved. As commands are added
 * the current command is one past the just added command.
 */
class CommandHistory
{
  public:
    /// Constructor that initializes an empty history.
    CommandHistory() : history_{}, current_index_{0}
    {
    }

  public:
    /**
     * @brief Adds a command to the history and erases all items from the current
     * index to the end.
     *
     * If the new command is a duplicate of the last, it is ignored.
     *
     * @param command The command to add to the history.
     * @return None.
     */
    auto add_command(std::string const &command) -> void
    {
        // First truncate history if needed
        if (current_index_ != history_.size())
        {
            history_.resize(++current_index_);
        }

        if (history_.empty() || command != history_.back())
        {
            history_.push_back(command);
            ++current_index_;
        }
    }

    /**
     * @brief Returns the previous command and sets the current command to it.
     *
     * @return The previous command string if available; std::nullopt if at the
     * 'current' position.
     */
    [[nodiscard]] auto previous() -> std::optional<std::string>
    {
        if (history_.empty())
        {
            return std::nullopt;
        }

        if (current_index_ != 0)
        {
            --current_index_;
        }

        return history_[current_index_];
    }

    /**
     * @brief Returns the next command and sets the current command to it.
     *
     * @return The next command string if available; std::nullopt if at the
     * 'current' position.
     */
    [[nodiscard]] auto next() -> std::optional<std::string>
    {
        if (current_index_ < history_.size())
        {
            ++current_index_;
        }

        if (current_index_ == history_.size())
        {
            return std::nullopt;
        }

        return history_[current_index_];
    }

    /**
     * @brief Returns the current command.
     *
     * @return The current command string if available; std::nullopt if at the
     * 'current' position.
     */
    [[nodiscard]] auto get_command() const -> std::optional<std::string>
    {
        if (current_index_ == history_.size())
        {
            return std::nullopt;
        }

        return history_[current_index_];
    }

  private:
    std::vector<std::string> history_;
    std::size_t current_index_;
};

} // namespace xen