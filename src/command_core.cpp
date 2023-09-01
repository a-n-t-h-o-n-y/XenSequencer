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

namespace
{

/**
 * @brief Splits a string into parameters, considering quotes.
 *
 * @param iss Input stringstream containing the string to split.
 * @return std::vector<std::string> List of parameters.
 */
[[nodiscard]] auto split_parameters(std::istringstream &iss) -> std::vector<std::string>
{
    auto params = std::vector<std::string>{};
    auto param = std::string{};
    auto inside_quotes = false;

    while (iss.peek() != std::istringstream::traits_type::eof())
    {
        auto ch = static_cast<char>(iss.get());

        if (ch == ' ' && !inside_quotes)
        {
            if (!param.empty())
            {
                params.push_back(param);
                param.clear();
            }
        }
        else if (ch == '"')
        {
            inside_quotes = !inside_quotes;
        }
        else
        {
            param += ch;
        }
    }

    if (!param.empty())
    {
        params.push_back(param);
    }

    return params;
}

} // namespace

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

auto CommandCore::get_matched_signature(std::string const &input) const
    -> std::optional<SignatureDisplay>
{
    CommandBase const *command = get_matched_command(input);
    if (command)
    {
        return command->get_signature_display();
    }
    return std::nullopt;
}

auto CommandCore::get_matched_command(std::string input) const -> CommandBase const *
{
    if (input.empty())
    {
        return nullptr;
    }
    input = to_lower(input);
    auto const input_name = input.substr(0, input.find(' '));
    auto matches = std::vector<std::string>{};
    for (auto const &[name, command] : commands_)
    {
        if (input_name == name)
        {
            matches.push_back(name);
        }
        // input is a prefix of name
        else if (input.find(' ') == std::string::npos && name.rfind(input_name, 0) == 0)
        {
            matches.push_back(name);
        }
    }

    if (matches.size() == 1)
    {
        return commands_.at(matches.front()).get();
    }
    return nullptr;
}

// TODO have returned message be labeled with enum for type. (info, warning, error,
// none)
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

    auto const params = split_parameters(iss);

    return it->second->execute(timeline_, params);
}

} // namespace xen