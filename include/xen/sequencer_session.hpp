#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include <xen/command.hpp>
#include <xen/command_catalog.hpp>
#include <xen/engine_state_mailbox.hpp>
#include <xen/state.hpp>
#include <xen/submission_effects.hpp>
#include <xen/workspace_settings.hpp>

namespace xen
{

class SequencerSession
{
  public:
    explicit SequencerSession(SubmissionEffects::FailurePoint effect_failure =
                                  SubmissionEffects::FailurePoint::None,
                              std::filesystem::path workspace_settings_file =
                                  WorkspaceSettingsStore::default_file());

    [[nodiscard]] auto project_snapshot() const -> ProjectSnapshot;
    [[nodiscard]] auto library_snapshot() const -> LibrarySnapshot;
    [[nodiscard]] auto command_catalog() const noexcept -> CommandCatalog const &;
    [[nodiscard]] auto command_session() const noexcept -> CommandSessionState const &;

    [[nodiscard]] auto execute_command_string(std::string const &command_string,
                                              CommandContext const &context)
        -> CommandApplicationResult;

    void replace_project_history(ProjectState state);
    void replace_library(ContentLibrary library);

    [[nodiscard]] auto audio_project_update_version() const noexcept -> std::uint64_t;
    [[nodiscard]] auto try_consume_audio_project_update() noexcept
        -> std::optional<EngineStateMailbox::ReadView>;

  private:
    PluginState state_;
    WorkspaceSettingsStore workspace_settings_store_;
    CommandCatalog command_catalog_;
    SubmissionEffects::FailurePoint effect_failure_;
    EngineStateMailbox pending_engine_state_update_;

    void publish_project_snapshot();
};

} // namespace xen
