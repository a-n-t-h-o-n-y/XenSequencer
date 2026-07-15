#pragma once

#include <filesystem>
#include <map>
#include <optional>
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
    [[nodiscard]] auto begin_preview(PreviewBeginRequest request) -> PreviewResponse;
    [[nodiscard]] auto begin_modulation_preview(ModulationPreviewBeginRequest request)
        -> PreviewResponse;
    [[nodiscard]] auto update_modulation_preview(ModulationPreviewUpdateRequest request)
        -> ModulationPreviewUpdateResponse;
    [[nodiscard]] auto commit_preview(PreviewEndRequest request) -> PreviewResponse;
    [[nodiscard]] auto cancel_preview(PreviewEndRequest request) -> PreviewResponse;
    [[nodiscard]] auto execute_document(DocumentRequest request) -> DocumentResponse;
    [[nodiscard]] auto disconnect(InstanceId const &instance_id)
        -> std::optional<ProjectSnapshot>;
    [[nodiscard]] auto set_binding(BindingSetRequest request) -> BindingSetResponse;

    [[nodiscard]] auto snapshot() const -> ProjectSnapshot;
    [[nodiscard]] auto library_snapshot() const -> LibrarySnapshot;
    [[nodiscard]] auto instances() const -> std::vector<InstanceBinding>;
    [[nodiscard]] auto binding_for(InstanceId const &instance_id) const
        -> InstanceBinding const *;
    void perform_recovery_maintenance(std::uint64_t now_unix_ms, bool force);

  private:
    SequencerSession session_;
    std::map<InstanceId, InstanceBinding> bindings_;
    StateRevision seed_revision_{};
    bool live_edit_started_{false};
    struct PreviewOwner
    {
        PreviewId preview_id{};
        InstanceId instance_id{};
    };
    std::optional<PreviewOwner> preview_owner_{};
    int next_channel_index_{1};

    void maybe_seed_from(ClientHello const &hello);
    [[nodiscard]] auto assign_binding(InstanceBinding binding) -> InstanceBinding;
    void ensure_channel_row(ChannelId const &channel_id);
};

} // namespace xen::ipc
