#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include <xen/command.hpp>
#include <xen/command_catalog.hpp>
#include <xen/sequencer_session_port.hpp>
#include <xen/state.hpp>
#include <xen/submission_effects.hpp>
#include <xen/workspace_settings.hpp>

namespace xen
{

class SequencerSession final : public SequencerSessionPort
{
  public:
    explicit SequencerSession(SubmissionEffects::FailurePoint effect_failure =
                                  SubmissionEffects::FailurePoint::None,
                              std::filesystem::path workspace_settings_file =
                                  WorkspaceSettingsStore::default_file());

    [[nodiscard]] auto project_snapshot() const -> ProjectSnapshot override;
    [[nodiscard]] auto persistent_project_snapshot() const -> ProjectSnapshot override;
    [[nodiscard]] auto library_snapshot() const -> LibrarySnapshot override;
    [[nodiscard]] auto instance_binding() const -> InstanceBinding override;
    [[nodiscard]] auto command_catalog() const noexcept -> CommandCatalog const &;
    [[nodiscard]] auto command_catalog_metadata() const
        -> std::vector<CatalogCommandMetadata> override;
    [[nodiscard]] auto command_session() const noexcept -> CommandSessionState const &;

    [[nodiscard]] auto execute_command_string(std::string const &command_string,
                                              CommandContext const &context)
        -> CommandApplicationResult override;
    [[nodiscard]] auto begin_preview(ProjectRevision expected_revision)
        -> PreviewControlResult override;
    [[nodiscard]] auto begin_modulation_preview(ProjectRevision expected_revision,
                                                ModulationTarget target)
        -> PreviewControlResult override;
    [[nodiscard]] auto update_modulation_preview(ModulationPreviewUpdate const &update)
        -> ModulationPreviewUpdateResult override;
    [[nodiscard]] auto commit_preview(PreviewId const &preview_id,
                                      ProjectRevision expected_revision)
        -> PreviewControlResult override;
    [[nodiscard]] auto cancel_preview(PreviewId const &preview_id,
                                      ProjectRevision expected_revision)
        -> PreviewControlResult override;
    [[nodiscard]] auto create_project(ProjectRevision expected_revision,
                                      bool discard_unsaved)
        -> DocumentOperationResult override;
    [[nodiscard]] auto open_project(std::string relative_path,
                                    ProjectRevision expected_revision,
                                    bool discard_unsaved)
        -> DocumentOperationResult override;
    [[nodiscard]] auto save_project(ProjectRevision expected_revision)
        -> DocumentOperationResult override;
    [[nodiscard]] auto save_project_as(
        std::string relative_path, ProjectRevision expected_revision,
        std::optional<std::string> expected_file_revision)
        -> DocumentOperationResult override;
    [[nodiscard]] auto restore_recovery(std::string recovery_revision,
                                        ProjectRevision expected_revision,
                                        bool discard_unsaved)
        -> DocumentOperationResult override;
    [[nodiscard]] auto discard_recovery(std::string recovery_revision)
        -> DocumentOperationResult override;
    [[nodiscard]] auto import_cell(std::string relative_path,
                                   ProjectRevision expected_revision,
                                   CompositionCursor cursor)
        -> DocumentOperationResult override;
    [[nodiscard]] auto save_cell(std::string relative_path,
                                 ProjectRevision expected_revision,
                                 CompositionCursor cursor, SelectionPath selection,
                                 std::optional<std::string> expected_file_revision)
        -> DocumentOperationResult override;

    void replace_project_history(ProjectState state);
    void replace_library(ContentLibrary library);
    void replace_instance_binding(InstanceBinding binding);
    void replace_project_history_and_binding(ProjectState state,
                                             InstanceBinding binding);
    void restore_persisted_state(PersistedProcessorState state);
    void perform_recovery_maintenance(std::uint64_t now_unix_ms, bool force);
    void set_channel_id(ChannelId channel_id) override;

  private:
    PluginState state_;
    InstanceBinding instance_binding_;
    WorkspaceSettingsStore workspace_settings_store_;
    CommandCatalog command_catalog_;
    SubmissionEffects::FailurePoint effect_failure_;

    struct ActivePreview
    {
        enum class Kind : std::uint8_t
        {
            Command,
            Modulation,
        };

        PreviewId id{};
        ProjectSnapshot baseline{};
        CommandSessionState command_session{};
        Kind kind{Kind::Command};
        std::optional<ModulationTarget> modulation_target{};
        std::uint64_t accepted_update_sequence{};
    };
    std::optional<ActivePreview> active_preview_{};
    std::optional<PersistedRecoveryState> recovery_candidate_{};
    std::filesystem::path recovery_file_{};
    std::optional<std::uint64_t> recovery_due_unix_ms_{};

    void require_document_operation(
        ProjectRevision expected_revision, bool discard_unsaved,
        bool pending_recovery_requires_discard = true) const;
    void advance_state_revision();
    void refresh_document_dirty(std::string const &project_digest);
    void schedule_recovery();
    void clear_recovery();
    void configure_recovery(std::optional<StateRevision> baseline_revision);
};

} // namespace xen
