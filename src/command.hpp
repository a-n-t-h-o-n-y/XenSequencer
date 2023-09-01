#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "parse_args.hpp"
#include "signature.hpp"
#include "xen_timeline.hpp"

namespace xen
{
template <typename... Args>
using Action = std::function<std::string(XenTimeline &, Args...)>;

/**
 * @brief A command that can be executed with a set of string arguments.
 */
class CommandBase
{
  public:
    [[nodiscard]] virtual auto get_name() const -> std::string = 0;
    [[nodiscard]] virtual auto get_description() const -> std::string = 0;
    [[nodiscard]] virtual auto get_signature_display() const -> SignatureDisplay = 0;
    [[nodiscard]] virtual auto get_default_arg_strings() const
        -> std::vector<std::string> = 0;

  public:
    [[nodiscard]] virtual auto execute(XenTimeline &tl,
                                       std::vector<std::string> const &args)
        -> std::string = 0;

  public:
    virtual ~CommandBase() = default;
};

/**
 * @brief A command that can be executed with a set of typed arguments.
 *
 * Create Command objects to build up a CommandCore.
 *
 * @tparam Args The types of the arguments.
 */
template <typename... Args>
class Command : public CommandBase
{
  public:
    /**
     * @brief Construct a new Command object.
     *
     * @param name The name of the command. Must be a non-empty alpha-only string with
     * no spaces.
     * @param description The text description of the command.
     * @param arg_infos The argument infos.
     * @param action The action function.
     */
    Command(std::string name, std::string description, ArgInfos<Args...> arg_infos,
            Action<Args...> action)
        : name_{std::move(name)}, description_{std::move(description)},
          arg_infos_{std::move(arg_infos)}, action_{std::move(action)}
    {
        // Validate Command Name
        if (name_.empty() ||
            !std::all_of(std::cbegin(name_), std::cend(name_), ::isalpha))
        {
            throw std::invalid_argument(
                "Command name must be a non-empty alpha-only string with no spaces.");
        }

        // Validate Action
        if (!action_)
        {
            throw std::invalid_argument("Command action cannot be null.");
        }

        // Validate Argument Names
        this->validate_arg_names(arg_infos_);
    }

  public:
    [[nodiscard]] auto execute(XenTimeline &tl, std::vector<std::string> const &args)
        -> std::string override
    {
        if (args.size() > sizeof...(Args))
        {
            return "Invalid number of arguments";
        }
        return this->invoke_action(tl, args, std::index_sequence_for<Args...>());
    }

  public:
    [[nodiscard]] auto get_name() const -> std::string override
    {
        return name_;
    }

    [[nodiscard]] auto get_description() const -> std::string override
    {
        return description_;
    }

    [[nodiscard]] auto get_signature_display() const -> SignatureDisplay override
    {
        return generate_signature(name_, arg_infos_);
    }

    [[nodiscard]] auto get_default_arg_strings() const
        -> std::vector<std::string> override
    {
        return collect_default_args(arg_infos_);
    }

  private:
    template <std::size_t... I>
    [[nodiscard]] auto invoke_action(XenTimeline &tl,
                                     std::vector<std::string> const &args,
                                     std::index_sequence<I...>) -> std::string
    {
        return action_(tl, this->get_argument_value<I, Args>(args)...);
    }

    template <std::size_t I, typename T>
    [[nodiscard]] auto get_argument_value(std::vector<std::string> const &args) const
        -> T
    {
        auto const &arg_info = std::get<I>(arg_infos_);
        if (I < args.size())
        {
            return parse<T>(args[I]);
        }
        else if (arg_info.default_value.has_value())
        {
            return arg_info.default_value.value();
        }
        else
        {
            throw std::invalid_argument("Missing argument and no default value");
        }
    }

    /**
     * @brief Validate the argument names.
     *
     * Each argument name must be a non-empty alpha-only string with no spaces.
     *
     * @param args The argument infos.
     * @throws std::invalid_argument if an argument name contains invalid characters.
     */
    auto validate_arg_names(ArgInfos<Args...> const &args) -> void
    {
        std::apply(
            [](auto &&...arg_infos) {
                auto validator = [](auto const &arg) {
                    if (arg.name.empty())
                    {
                        throw std::invalid_argument("Argument name cannot be empty.");
                    }
                    else if (!std::all_of(
                                 std::cbegin(arg.name), std::cend(arg.name),
                                 [](char ch) { return std::isalpha(ch) || ch == '_'; }))
                    {
                        throw std::invalid_argument(
                            "Invalid character in argument name: " + arg.name);
                    }
                };
                (validator(arg_infos), ...);
            },
            args);
    }

  private:
    std::string name_;
    std::string description_;
    ArgInfos<Args...> arg_infos_;
    Action<Args...> action_;
};

/**
 * @brief Create a unique ptr Command object.
 *
 * @tparam ActionFn The type of the action function.
 * @tparam Args The types of the arguments.
 * @param name The name of the command.
 * @param description The description of the command.
 * @param action The action function.
 * @param arg_infos The argument infos.
 * @return std::unique_ptr<CommandBase>
 */
template <typename ActionFn, typename... Args>
auto cmd(std::string name, std::string description, ActionFn action,
         ArgInfo<Args> &&...arg_infos) -> std::unique_ptr<CommandBase>
{
    static_assert(std::is_invocable_v<ActionFn, XenTimeline &, Args...>,
                  "ActionFn signature doesn't match (XenTimeline&, Args...)");

    auto tuple_args = ArgInfos<Args...>{std::forward<ArgInfo<Args>>(arg_infos)...};
    return std::make_unique<Command<std::decay_t<Args>...>>(
        std::move(name), std::move(description), std::move(tuple_args),
        std::move(action));
}

} // namespace xen