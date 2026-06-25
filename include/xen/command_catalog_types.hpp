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

class CommandTransaction;

struct CommandContext
{
    std::optional<SelectionPath> selection{};
    std::optional<ProjectRevision> expected_project_revision{};
    std::optional<ActiveMeasureTarget> active_measure_target{};
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
    CommandTransaction &, CommandExecutionContext &)>;

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

struct CatalogArgumentConstraint
{
    std::string kind{};
    std::optional<double> minimum{};
    std::optional<double> maximum{};
    std::vector<std::string> values{};
};

struct CatalogArgumentMetadata
{
    std::string kind{};
    std::string display_name{};
    bool required{true};
    std::optional<std::string> default_value{};
    std::vector<CatalogArgumentConstraint> constraints{};
};

struct CatalogCommandMetadata
{
    std::vector<std::string> path{};
    std::vector<std::string> keywords{};
    bool accepts_pattern_prefix{false};
    TargetRequirement target{TargetRequirement::None};
    std::vector<CatalogArgumentMetadata> arguments{};
    std::string description{};
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
