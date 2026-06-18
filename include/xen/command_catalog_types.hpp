#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <xen/message_level.hpp>
#include <xen/state.hpp>

namespace xen
{

struct CommandInvocation;

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

enum class BoundCommandControl : std::uint8_t
{
    Execute,
    ReplayPrevious,
};

struct CommandExecutionResult
{
    std::pair<MessageLevel, std::string> status{MessageLevel::Debug, ""};
    ExecutionContext context{};
    bool engine_mutated{false};
    CommitIntent commit_intent{CommitIntent::Auto};
};

using BoundCommandExecutor =
    std::function<CommandExecutionResult(PluginState &, ExecutionContext)>;

struct BoundCommand
{
    std::string canonical{};
    BoundCommandControl control{BoundCommandControl::Execute};
    BoundCommandExecutor execute{};
};

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
    std::function<BoundCommand(CommandInvocation const &, std::size_t)> bind{};
};

using BindInvocationResult = std::variant<BoundCommand, CatalogBindError>;
using BindChainResult = std::variant<std::vector<BoundCommand>, CatalogBindError>;

} // namespace xen
