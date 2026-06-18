#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <xen/command.hpp>
#include <xen/message_level.hpp>
#include <xen/state.hpp>

namespace xen
{

class SubmissionEffects;

enum class CatalogBindErrorKind : std::uint8_t
{
    UnknownCommand,
    MissingArgument,
    InvalidArgument,
    UnexpectedArgument,
    PatternPrefixNotAllowed,
};

struct CatalogBindError
{
    CatalogBindErrorKind kind{CatalogBindErrorKind::UnknownCommand};
    std::string message{};
    std::string token{};
};

class CatalogBindException final : public std::exception
{
  public:
    CatalogBindException(CatalogBindErrorKind kind, std::string argument,
                         std::string detail);

    [[nodiscard]] auto kind() const noexcept -> CatalogBindErrorKind;
    [[nodiscard]] auto argument() const -> std::string const &;
    [[nodiscard]] auto detail() const -> std::string const &;
    [[nodiscard]] auto what() const noexcept -> char const * override;

  private:
    CatalogBindErrorKind kind_;
    std::string argument_;
    std::string detail_;
    std::string message_;
};

enum class ExecutionRole : std::uint8_t
{
    Normal,
    Undo,
    Redo,
};

enum class RepeatPolicy : std::uint8_t
{
    Never,
    EngineEdit,
};

using CommandExecutor = std::function<std::pair<MessageLevel, std::string>(
    PluginState &, SubmissionEffects &)>;

struct ExecutableCommand
{
    std::string canonical{};
    CommandInvocation invocation{};
    ExecutionRole execution_role{ExecutionRole::Normal};
    RepeatPolicy repeat_policy{RepeatPolicy::EngineEdit};
    CommandExecutor execute;
};

struct RepeatPrevious
{
};

using BoundStep = std::variant<ExecutableCommand, RepeatPrevious>;

struct CatalogArgumentMetadata
{
    std::string type{};
    std::string name{};
    std::optional<std::string> default_value{};
};

struct CatalogCommandMetadata
{
    std::vector<std::string> path{};
    bool accepts_pattern_prefix{false};
    std::vector<CatalogArgumentMetadata> arguments{};
    std::string description{};
};

enum class CompletionCandidateKind : std::uint8_t
{
    CommandToken,
    Argument,
};

struct CompletionCandidate
{
    std::string insertion{};
    std::string display{};
    std::string description{};
    CompletionCandidateKind kind{CompletionCandidateKind::CommandToken};
};

struct CompletionResult
{
    std::vector<CompletionCandidate> candidates{};
    std::optional<CatalogArgumentMetadata> active_argument{};
};

struct CommandDefinition
{
    CatalogCommandMetadata metadata{};
    std::function<BoundStep(CommandInvocation const &, std::size_t)> bind{};
};

using BindInvocationResult = std::variant<BoundStep, CatalogBindError>;
using BindChainResult = std::variant<std::vector<BoundStep>, CatalogBindError>;

} // namespace xen
