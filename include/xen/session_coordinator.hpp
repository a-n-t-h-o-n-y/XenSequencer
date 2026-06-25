#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

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
    [[nodiscard]] auto set_binding(BindingSetRequest request) -> BindingSetResponse;

    [[nodiscard]] auto snapshot() const -> ProjectSnapshot;
    [[nodiscard]] auto library_snapshot() const -> LibrarySnapshot;
    [[nodiscard]] auto instances() const -> std::vector<InstanceBinding>;
    [[nodiscard]] auto binding_for(InstanceId const &instance_id) const
        -> InstanceBinding const *;
    [[nodiscard]] auto live_edit_started() const noexcept -> bool;

  private:
    SequencerSession session_;
    std::map<InstanceId, InstanceBinding> bindings_;
    ProjectRevision seed_revision_{};
    bool live_edit_started_{false};
    int next_output_index_{1};

    void maybe_seed_from(ClientHello const &hello);
    [[nodiscard]] auto assign_binding(InstanceBinding binding) -> InstanceBinding;
    void ensure_output_row(OutputId const &output_id);
};

} // namespace xen::ipc
