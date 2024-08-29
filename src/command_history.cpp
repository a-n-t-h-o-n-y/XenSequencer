#include <xen/command_history.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace xen
{

CommandHistory::CommandHistory() : history_{}, current_index_{0}
{
}

void CommandHistory::add_command(std::string const &command)
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

auto CommandHistory::previous() -> std::optional<std::string>
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

auto CommandHistory::next() -> std::optional<std::string>
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

auto CommandHistory::get_command() const -> std::optional<std::string>
{
    if (current_index_ == history_.size())
    {
        return std::nullopt;
    }

    return history_[current_index_];
}

} // namespace xen