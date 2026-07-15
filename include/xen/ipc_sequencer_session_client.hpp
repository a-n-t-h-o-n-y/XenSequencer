#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <juce_events/juce_events.h>
#include <nlohmann/json.hpp>

#include <xen/command_catalog.hpp>
#include <xen/coordinator_launcher.hpp>
#include <xen/ipc_protocol.hpp>
#include <xen/midi_compilation_service.hpp>
#include <xen/processor_session_port.hpp>

namespace xen::ipc
{

class IpcSequencerSessionClient final : public ProcessorSessionPort,
                                        private juce::InterprocessConnection
{
  public:
    explicit IpcSequencerSessionClient(
        InstanceBinding binding,
        std::optional<PersistedProcessorState> restore_state = std::nullopt,
        std::filesystem::path helper_path = {});
    ~IpcSequencerSessionClient() override;

    [[nodiscard]] auto project_snapshot() const -> ProjectSnapshot override;
    [[nodiscard]] auto persistent_project_snapshot() const -> ProjectSnapshot override;
    [[nodiscard]] auto library_snapshot() const -> LibrarySnapshot override;
    [[nodiscard]] auto instance_binding() const -> InstanceBinding override;
    [[nodiscard]] auto command_catalog_metadata() const
        -> std::vector<CatalogCommandMetadata> override;

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
    void set_channel_id(ChannelId channel_id) override;

    [[nodiscard]] auto compiled_midi_generation() const noexcept
        -> std::uint64_t override;
    [[nodiscard]] auto try_consume_compiled_midi() noexcept
        -> std::optional<CompiledMidiMailbox::ReadView> override;
    [[nodiscard]] auto midi_compilation_status() const
        -> MidiCompilationStatus override;

    [[nodiscard]] auto online() const noexcept -> bool;
    [[nodiscard]] auto last_error() const -> std::string;

  private:
    std::mutex request_mutex_;
    mutable std::mutex mutex_;
    std::condition_variable response_ready_;
    InstanceBinding binding_;
    ProjectSnapshot project_snapshot_{};
    ProjectSnapshot persistent_project_snapshot_{};
    LibrarySnapshot library_snapshot_{};
    CommandCatalog command_catalog_;
    MidiCompilationService midi_compilation_;
    std::optional<StateRevision> last_midi_compilation_state_revision_;
    ChannelId last_midi_compilation_channel_id_{};
    bool online_{false};
    std::string last_error_{};
    std::uint64_t next_request_id_{1};
    std::optional<CommandResponse> pending_command_response_;
    std::optional<PreviewResponse> pending_preview_response_;
    std::optional<ModulationPreviewUpdateResponse>
        pending_modulation_preview_update_response_;
    std::optional<DocumentResponse> pending_document_response_;
    std::optional<BindingSetResponse> pending_binding_response_;
    std::optional<ShutdownIfIdleResponse> pending_shutdown_response_;
    std::optional<IpcError> pending_error_;
    std::unique_ptr<juce::ChildProcess> helper_process_;

    void connect_or_throw(std::optional<PersistedProcessorState> restore_state,
                          std::filesystem::path helper_path);
    [[nodiscard]] auto next_request_id() -> std::string;
    [[nodiscard]] auto request_helper_shutdown_if_idle() -> bool;
    void submit_midi_compilation();
    void ingest_project_snapshot(ProjectSnapshot snapshot);
    void send_json(nlohmann::json const &message);
    [[nodiscard]] auto finish_preview_request(nlohmann::json request,
                                              std::string const &request_id)
        -> PreviewControlResult;
    [[nodiscard]] auto finish_document_request(DocumentRequest request)
        -> DocumentOperationResult;

    void connectionMade() override;
    void connectionLost() override;
    void messageReceived(juce::MemoryBlock const &message) override;
};

} // namespace xen::ipc
