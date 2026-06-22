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

struct CommandContext
{
    std::optional<SelectionPath> selection{};
    std::optional<ProjectRevision> expected_project_revision{};
};

struct CommandExecutionContext
{
    std::optional<SelectionPath> selection{};
};

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

enum class ProjectOperation : std::uint8_t
{
    None,
    Read,
    Edit,
    ReplaceHistory,
    NavigateHistory,
};

enum class LibraryAccess : std::uint8_t
{
    None,
    Read,
    Mutate,
};

enum class WorkspaceAccess : std::uint8_t
{
    None,
    Read,
    Mutate,
};

enum class FileAccess : std::uint8_t
{
    None,
    Read,
    Write,
};

enum class TargetRequirement : std::uint8_t
{
    None,
    Cell,
    Element,
    CellOrElement,
};

enum class RepeatPolicy : std::uint8_t
{
    Never,
    OnSuccessfulProjectChange,
};

enum class HistoryPolicy : std::uint8_t
{
    None,
    Commit,
    AmendCompatibleTransform,
};

struct CommandPolicy
{
    CommandPolicy() = delete;
    constexpr CommandPolicy(ProjectOperation project_in, LibraryAccess library_in,
                            WorkspaceAccess workspace_in, FileAccess files_in,
                            TargetRequirement target_in, RepeatPolicy repeat_in,
                            HistoryPolicy history_in) noexcept
        : project{project_in}, library{library_in}, workspace{workspace_in},
          files{files_in}, target{target_in}, repeat{repeat_in}, history{history_in}
    {
    }

    ProjectOperation project;
    LibraryAccess library;
    WorkspaceAccess workspace;
    FileAccess files;
    TargetRequirement target;
    RepeatPolicy repeat;
    HistoryPolicy history;

    auto operator==(CommandPolicy const &) const -> bool = default;
};

using CommandStatus = std::pair<MessageLevel, std::string>;

struct CommandApplicationResult
{
    CommandStatus status{MessageLevel::Debug, ""};
    std::optional<SelectionPath> suggested_selection{};
};

using CommandExecutor = std::function<CommandApplicationResult(
    PluginState &, SubmissionEffects &, CommandExecutionContext &)>;

struct ExecutableCommand
{
    std::string canonical{};
    CommandInvocation invocation{};
    CommandPolicy const policy;
    CommandExecutor execute;
};

enum class HistoryNavigationDirection : std::uint8_t
{
    Undo,
    Redo,
};

struct ExecutableHistoryNavigation
{
    std::string canonical{};
    CommandPolicy const policy;
    HistoryNavigationDirection direction{HistoryNavigationDirection::Undo};
};

struct RepeatPrevious
{
};

using BoundStep =
    std::variant<ExecutableCommand, ExecutableHistoryNavigation, RepeatPrevious>;

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
    CommandPolicy const policy;
    bool const uses_submission_effects;
    std::function<BoundStep(CommandInvocation const &, std::size_t)> bind{};
};

using BindInvocationResult = std::variant<BoundStep, CatalogBindError>;
using BindChainResult = std::variant<std::vector<BoundStep>, CatalogBindError>;

} // namespace xen
