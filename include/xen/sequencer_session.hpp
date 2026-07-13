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
    [[nodiscard]] auto instance_binding() const -> InstanceBinding const & override;
    [[nodiscard]] auto command_catalog() const noexcept -> CommandCatalog const &;
    [[nodiscard]] auto command_catalog_metadata() const
        -> std::vector<CatalogCommandMetadata> override;
    [[nodiscard]] auto command_session() const noexcept -> CommandSessionState const &;

    [[nodiscard]] auto execute_command_string(std::string const &command_string,
                                              CommandContext const &context)
        -> CommandApplicationResult override;
    [[nodiscard]] auto begin_preview(ProjectRevision expected_revision)
        -> PreviewControlResult override;
    [[nodiscard]] auto commit_preview(PreviewId const &preview_id,
                                      ProjectRevision expected_revision)
        -> PreviewControlResult override;
    [[nodiscard]] auto cancel_preview(PreviewId const &preview_id,
                                      ProjectRevision expected_revision)
        -> PreviewControlResult override;

    void replace_project_history(ProjectState state);
    void replace_library(ContentLibrary library);
    void replace_instance_binding(InstanceBinding binding);
    void replace_project_history_and_binding(ProjectState state,
                                             InstanceBinding binding);
    void set_channel_id(ChannelId channel_id) override;

  private:
    PluginState state_;
    InstanceBinding instance_binding_;
    WorkspaceSettingsStore workspace_settings_store_;
    CommandCatalog command_catalog_;
    SubmissionEffects::FailurePoint effect_failure_;

    struct ActivePreview
    {
        PreviewId id{};
        ProjectSnapshot baseline{};
        CommandSessionState command_session{};
    };
    std::optional<ActivePreview> active_preview_{};
};

} // namespace xen
