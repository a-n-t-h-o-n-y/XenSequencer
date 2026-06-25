#pragma once

#include <filesystem>
#include <map>
#include <string>

#include <xen/ipc_protocol.hpp>
#include <xen/sequencer_session.hpp>

namespace xen::ipc
{

class SessionCoordinator
{
  public:
    explicit SessionCoordinator(SubmissionEffects::FailurePoint effect_failure =
                                    SubmissionEffects::FailurePoint::None,
                                std::filesystem::path workspace_settings_file =
                                    WorkspaceSettingsStore::default_file());

    [[nodiscard]] auto connect(ClientHello hello) -> CoordinatorHello;
    [[nodiscard]] auto execute(CommandRequest request) -> CommandResponse;

    [[nodiscard]] auto snapshot() const -> ProjectSnapshot;
    [[nodiscard]] auto binding_for(InstanceId const &instance_id) const
        -> InstanceBinding const *;
    [[nodiscard]] auto live_edit_started() const noexcept -> bool;

  private:
    SequencerSession session_;
    std::map<InstanceId, InstanceBinding> bindings_;
    ProjectRevision seed_revision_{};
    bool live_edit_started_{false};

    void maybe_seed_from(ClientHello const &hello);
};

} // namespace xen::ipc
