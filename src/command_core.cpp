#include "command_core.hpp"

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "signature.hpp"
#include "util.hpp"
#include "xen_timeline.hpp"

namespace xen
{

CommandCore::CommandCore(XenTimeline &tl) : timeline_{tl}
{
    // TODO rewrite help command and figure out if it is worth displaying in-plugin

    // this->add(cmd("help", "Prints all commands.", [this](XenTimeline &) {
    //     auto help_message = std::string{"Available commands:\n"};
    //     //   for (auto const &[name, command] : commands_)
    //     //   {
    //     //   help_message += command->get_name() + " - " +
    //     //   }
    //     return help_message;
    // }));
}

auto CommandCore::add(std::unique_ptr<CommandBase> cmd) -> void
{
    if (!cmd)
    {
        throw std::invalid_argument("Command cannot be null.");
    }

    auto name = to_lower(cmd->get_name());

    // Check if command already exists
    if (commands_.find(name) != std::cend(commands_))
    {
        throw std::runtime_error("Command with the same name already exists.");
    }

    commands_.emplace(std::move(name), std::move(cmd));
}

auto CommandCore::match_command(std::string input) const
    -> std::optional<SignatureDisplay>
{
    input = to_lower(input);
    input = input.substr(0, input.find(' '));
    auto matches = std::vector<std::string>{};
    for (auto const &[name, command] : commands_)
    {
        if (name.rfind(input, 0) == 0)
        { // input is a prefix of name
            matches.push_back(command->get_name());
        }
    }

    if (matches.size() == 1)
    {
        return commands_.at(matches[0])->get_signature_display();
    }

    return std::nullopt;
}

auto CommandCore::execute_command(std::string const &input) const -> std::string
{
    auto iss = std::istringstream{input};
    auto command_name = std::string{};
    std::getline(iss, command_name, ' ');
    auto const command_name_lower = to_lower(command_name);

    auto it = commands_.find(command_name_lower);
    if (it == commands_.end())
    {
        throw std::runtime_error("Command '" + command_name + "' not found");
    }

    auto params = std::vector<std::string>{};
    auto param = std::string{};
    while (iss >> param)
    {
        params.push_back(param);
    }

    return it->second->execute(timeline_, params);
}

} // namespace xen