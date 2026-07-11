#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <xen/command.hpp>
#include <xen/command_catalog_types.hpp>
#include <xen/state.hpp>
#include <xen/submission_effects.hpp>

namespace xen
{

enum class HistoryPlanKind : std::uint8_t
{
    None,
    Commit,
    Amend,
    Replace,
    NavigateUndo,
    NavigateRedo,
};

struct HistoryPlan
{
    HistoryPlanKind kind{HistoryPlanKind::None};
    std::optional<HistoryEntryId> expected_entry_id{};
};

class CommandTransaction;

struct TransformInputs
{
    ProjectState baseline{};
    SelectionPath selection{};
    std::string chord_name{};
    int inversion{-1};
};

class ProjectReadCapability
{
  public:
    [[nodiscard]] auto get() const -> ProjectState const &;

  private:
    friend class CommandTransaction;
    explicit ProjectReadCapability(CommandTransaction &transaction)
        : transaction_{transaction}
    {
    }
    CommandTransaction &transaction_;
};

class ProjectEditCapability
{
  public:
    [[nodiscard]] auto get() -> ProjectState &;

  private:
    friend class CommandTransaction;
    friend struct CommandHandlerContext;
    explicit ProjectEditCapability(CommandTransaction &transaction)
        : transaction_{transaction}
    {
    }
    CommandTransaction &transaction_;
};

class LibraryReadCapability
{
  public:
    [[nodiscard]] auto get() const -> ContentLibrary const &;

  private:
    friend class CommandTransaction;
    explicit LibraryReadCapability(CommandTransaction &transaction)
        : transaction_{transaction}
    {
    }
    CommandTransaction &transaction_;
};

class LibraryEditCapability
{
  public:
    [[nodiscard]] auto get() -> ContentLibrary &;

  private:
    friend class CommandTransaction;
    explicit LibraryEditCapability(CommandTransaction &transaction)
        : transaction_{transaction}
    {
    }
    CommandTransaction &transaction_;
};

class WorkspaceReadCapability
{
  public:
    [[nodiscard]] auto get() const -> WorkspaceSettings const &;

  private:
    friend class CommandTransaction;
    explicit WorkspaceReadCapability(CommandTransaction &transaction)
        : transaction_{transaction}
    {
    }
    CommandTransaction &transaction_;
};

class WorkspaceEditCapability
{
  public:
    [[nodiscard]] auto get() -> WorkspaceSettings &;

  private:
    friend class CommandTransaction;
    explicit WorkspaceEditCapability(CommandTransaction &transaction)
        : transaction_{transaction}
    {
    }
    CommandTransaction &transaction_;
};

class FileReadCapability
{
  public:
    [[nodiscard]] auto read_text(std::filesystem::path const &source) const
        -> std::optional<std::string>;

  private:
    friend class CommandTransaction;
    explicit FileReadCapability(CommandTransaction &transaction)
        : transaction_{transaction}
    {
    }
    CommandTransaction &transaction_;
};

class FileWriteCapability
{
  public:
    void write_text(std::filesystem::path const &destination, std::string content);

  private:
    friend class CommandTransaction;
    explicit FileWriteCapability(CommandTransaction &transaction)
        : transaction_{transaction}
    {
    }
    CommandTransaction &transaction_;
};

struct CommandHandlerContext
{
    ProjectReadCapability *project_read{};
    ProjectEditCapability *project_edit{};
    LibraryReadCapability *library_read{};
    LibraryEditCapability *library_edit{};
    WorkspaceReadCapability *workspace_read{};
    WorkspaceEditCapability *workspace_edit{};
    FileReadCapability *file_read{};
    FileWriteCapability *file_write{};
    CommandExecutionContext &execution;

    [[nodiscard]] auto project() const -> ProjectState const &;
    [[nodiscard]] auto edit_project() -> ProjectState &;
    [[nodiscard]] auto library() const -> ContentLibrary const &;
    [[nodiscard]] auto edit_library() -> ContentLibrary &;
    [[nodiscard]] auto workspace() const -> WorkspaceSettings const &;
    [[nodiscard]] auto edit_workspace() -> WorkspaceSettings &;
    [[nodiscard]] auto read_text(std::filesystem::path const &source) const
        -> std::optional<std::string>;
    void write_text(std::filesystem::path const &destination, std::string content);
    [[nodiscard]] auto prepare_transform(TransformKind kind, std::string chord_name,
                                         int inversion) -> TransformInputs;
};

class CommandTransaction
{
  public:
    CommandTransaction(PluginState &state,
                       SubmissionEffects::FailurePoint effect_failure);

    [[nodiscard]] auto make_handler_context(CommandPolicy const &policy,
                                            CommandExecutionContext &execution)
        -> CommandHandlerContext;
    [[nodiscard]] auto project() const -> ProjectState const &;
    [[nodiscard]] auto edit_project() -> ProjectState &;
    [[nodiscard]] auto library() const -> ContentLibrary const &;
    [[nodiscard]] auto edit_library() -> ContentLibrary &;
    [[nodiscard]] auto workspace() const -> WorkspaceSettings const &;
    [[nodiscard]] auto edit_workspace() -> WorkspaceSettings &;
    [[nodiscard]] auto effects() noexcept -> SubmissionEffects &;
    [[nodiscard]] auto prepare_transform(TransformKind kind,
                                         CompositionCursor const &cursor,
                                         SelectionPath const &selection,
                                         std::string chord_name, int inversion)
        -> TransformInputs;

    void plan_history(HistoryPlan plan) noexcept;
    void record_repeat(CommandInvocation invocation);
    void clear_repeat() noexcept;
    void invalidate_transform_sessions();
    [[nodiscard]] auto repeat_candidate() const
        -> std::optional<std::vector<CommandInvocation>> const &;
    [[nodiscard]] auto project_changed() const -> bool;
    [[nodiscard]] auto library_changed() const -> bool;
    [[nodiscard]] auto workspace_changed() const -> bool;
    [[nodiscard]] auto has_domain_candidates() const noexcept -> bool;
    [[nodiscard]] auto has_project_candidate() const noexcept -> bool;
    [[nodiscard]] auto has_library_candidate() const noexcept -> bool;
    [[nodiscard]] auto has_workspace_candidate() const noexcept -> bool;
    [[nodiscard]] auto has_history_plan() const noexcept -> bool;
    [[nodiscard]] auto history_plan_is_amend() const noexcept -> bool;

    void prepare();
    void apply_effects();
    [[nodiscard]] auto rollback_effects() noexcept -> std::string;
    void install() noexcept;
    void finalize_effects() noexcept;

  private:
    friend class ProjectReadCapability;
    friend class ProjectEditCapability;
    friend class LibraryReadCapability;
    friend class LibraryEditCapability;
    friend class WorkspaceReadCapability;
    friend class WorkspaceEditCapability;
    friend class FileReadCapability;
    friend class FileWriteCapability;

    PluginState &state_;
    SubmissionEffects effects_;
    HistoryPlan history_{};
    std::optional<ProjectState> project_{};
    std::optional<ContentLibrary> library_{};
    std::optional<WorkspaceSettings> workspace_{};
    std::optional<CommandSessionState> sessions_{};
    std::optional<XenTimeline> prepared_timeline_{};
    std::optional<std::vector<CommandInvocation>> repeat_candidate_{};

    ProjectReadCapability project_read_{*this};
    ProjectEditCapability project_edit_{*this};
    LibraryReadCapability library_read_{*this};
    LibraryEditCapability library_edit_{*this};
    WorkspaceReadCapability workspace_read_{*this};
    WorkspaceEditCapability workspace_edit_{*this};
    FileReadCapability file_read_{*this};
    FileWriteCapability file_write_{*this};
};

} // namespace xen
