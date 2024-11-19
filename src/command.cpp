#include <xen/command.hpp>

#include <sequence/pattern.hpp>

#include <xen/string_manip.hpp>

#include <string>

namespace xen
{

auto split_input(std::string input) -> SplitInput
{
    auto split_input = SplitInput{
        .pattern = {0, {1}},
        .words = {},
    };

    if (sequence::contains_valid_pattern(input))
    {
        split_input.pattern = sequence::parse_pattern(input);
        input = sequence::pop_pattern_chars(input);
    }

    split_input.words = split_quoted_string(input);

    return split_input;
}

// -------------------------------------------------------------------------------------

CommandGroup::CommandGroup(std::string_view id) : id_{id}
{
}

void CommandGroup::add(std::unique_ptr<CommandBase> cmd)
{
    commands_.push_back(std::move(cmd));
}

auto CommandGroup::id() const -> std::string_view
{
    return id_;
}

auto CommandGroup::execute(PluginState &ps, SplitInput input) const
    -> std::pair<MessageLevel, std::string>
{
    if (input.words.empty())
    {
        return {MessageLevel::Error, "No command given."};
    }

    auto const front = to_lower(input.words.front());
    for (auto const &command_ptr : commands_)
    {
        if (front == to_lower(command_ptr->id()))
        {
            input.words.erase(input.words.begin());
            return command_ptr->execute(ps, input);
        }
    }

    return {MessageLevel::Error, "Command not found: " + input.words.front()};
}

auto CommandGroup::complete_text(SplitInput input) const -> std::string
{
    if (input.words.empty())
    {
        return "[next command]";
    }

    // Search for a complete match.
    for (auto const &command_ptr : commands_)
    {
        CommandBase &cmd = *command_ptr;

        if (to_lower(cmd.id()) == to_lower(input.words.front()))
        {
            input.words.erase(input.words.begin());
            return cmd.complete_text(input);
        }
    }

    // If no complete match, search for first partial match.
    for (auto const &command_ptr : commands_)
    {
        CommandBase const &cmd = *command_ptr;

        if (to_lower(cmd.id().substr(0, input.words.front().size())) ==
            to_lower(input.words.front()))
        {
            return std::string{cmd.id().substr(input.words.front().size())};
        }
    }

    return "";
}

auto cmd_group(std::string_view id) -> std::unique_ptr<CommandGroup>
{
    return std::make_unique<CommandGroup>(id);
}

} // namespace xen