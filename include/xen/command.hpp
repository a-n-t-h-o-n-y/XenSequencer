#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <sequence/pattern.hpp>

#include <xen/message_level.hpp>
#include <xen/parse_args.hpp>
#include <xen/signature.hpp>
#include <xen/string_manip.hpp>

namespace xen
{
struct PluginState;

/**
 * A slightly more structured version of the input string.
 * @details Quoted strings are not split, they are a single 'word'.
 */
struct SplitInput
{
    sequence::Pattern pattern;
    std::vector<std::string> words;
};

/**
 * Split the input string into a Pattern and a vector of words.
 * @param input The input string to split.
 * @return SplitInput The split input.
 * @exception std::invalid_argument Thrown when the input string is not valid, such as
 * having an invalid pattern or unterminated quotes.
 */
[[nodiscard]] auto split_input(std::string input) -> SplitInput;

/**
 * Provides a textural description of a command for documentation purposes.
 */
struct Documentation
{
    SignatureDisplay signature;
    std::string description;
};

/**
 * Base class for all commands.
 */
class CommandBase
{
  public:
    CommandBase() = default;

    CommandBase(CommandBase const &) = delete;
    CommandBase(CommandBase &&) = default;

    CommandBase &operator=(CommandBase const &) = delete;
    CommandBase &operator=(CommandBase &&) = default;

    virtual ~CommandBase() = default;

  public:
    [[nodiscard]] virtual auto id() const -> std::string_view = 0;

    [[nodiscard]] virtual auto execute(PluginState &ps, SplitInput input) const
        -> std::pair<MessageLevel, std::string> = 0;

    [[nodiscard]] virtual auto complete_text(SplitInput input) const -> std::string = 0;

    [[nodiscard]] virtual auto generate_docs() -> std::vector<Documentation> = 0;
};

// -------------------------------------------------------------------------------------

/**
 * Description of a command line string command with a single ID and a specified array
 * of arguments.
 */
template <typename Signature_t, typename Fn>
class Command : public CommandBase
{
  public:
    Signature_t signature;
    Fn fn;
    std::string description;

  public:
    Command(Signature_t signature_, Fn fn_, char const *description_)
        : signature{std::move(signature_)}, fn{std::move(fn_)},
          description{description_}
    {
    }

  public:
    [[nodiscard]] auto id() const -> std::string_view override
    {
        return signature.id; // TODO probably hardcode Signature to have a string ID
    }

    [[nodiscard]] auto execute(PluginState &ps, SplitInput input) const
        -> std::pair<MessageLevel, std::string> override
    {
        // Dispatches based on Signature or PatternedSignature.
        return do_invoke(signature, fn, ps, input);
    }

    [[nodiscard]] auto complete_text(SplitInput input) const -> std::string override
    {
        // Display all ArgInfos beyond the current number of input args.
        auto const display = generate_display(signature);

        auto oss = std::ostringstream{};
        auto prefix = std::string{""};
        for (auto i = input.words.size(); i < display.arguments.size(); ++i)
        {
            oss << prefix << display.arguments[i];
            prefix = " ";
        }

        return oss.str();
    }

    [[nodiscard]] auto generate_docs() -> std::vector<Documentation> override
    {
        return {Documentation{
            .signature = generate_display(signature),
            .description = this->description,
        }};
    }

  private:
    template <typename... Args>
    [[nodiscard]] static auto do_invoke(Signature<Args...> signature, Fn fn,
                                        PluginState &ps, SplitInput input)
        -> std::pair<MessageLevel, std::string>
    {
        static_assert(std::is_invocable_r_v<std::pair<MessageLevel, std::string>, Fn,
                                            PluginState &, Args...>,
                      "Invalid function type.");

        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return fn(
                ps, get_argument_value<I>(input.words, std::get<I>(signature.args))...);
        }(std::index_sequence_for<Args...>{});
    }

    template <typename... Args>
    [[nodiscard]] static auto do_invoke(PatternedSignature<Args...> signature, Fn fn,
                                        PluginState &ps, SplitInput input)
    {
        static_assert(
            std::is_invocable_r_v<std::pair<MessageLevel, std::string>, Fn,
                                  PluginState &, sequence::Pattern const &, Args...>,
            "Invalid function type.");

        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return fn(
                ps, input.pattern,
                get_argument_value<I>(input.words, std::get<I>(signature.args))...);
        }(std::index_sequence_for<Args...>{});
    }
};

/**
 * Allocate a new Command object as a std::unique_ptr.
 */
template <typename Signature_t, typename Fn>
[[nodiscard]] auto cmd(Signature_t signature, char const *description, Fn fn)
    -> std::unique_ptr<Command<Signature_t, Fn>>
{
    return std::make_unique<Command<Signature_t, Fn>>(std::move(signature),
                                                      std::move(fn), description);
}

// -------------------------------------------------------------------------------------

class CommandGroup : public CommandBase
{
  public:
    CommandGroup(std::string_view id);

  public:
    void add(std::unique_ptr<CommandBase> cmd);

  public:
    [[nodiscard]] auto id() const -> std::string_view override;

    [[nodiscard]] auto execute(PluginState &ps, SplitInput input) const
        -> std::pair<MessageLevel, std::string> override;

    [[nodiscard]] auto complete_text(SplitInput input) const -> std::string override;

    [[nodiscard]] auto generate_docs() -> std::vector<Documentation> override;

  private:
    std::string_view id_;
    std::vector<std::unique_ptr<CommandBase>> commands_;
};

[[nodiscard]] auto cmd_group(std::string_view id) -> std::unique_ptr<CommandGroup>;

} // namespace xen
