#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <xen/command_catalog_types.hpp>
#include <xen/document.hpp>
#include <xen/state.hpp>

namespace xen::ipc
{

inline constexpr auto protocol = "xen.ipc.v3";

struct ClientHello
{
    InstanceBinding binding{};
    std::optional<PersistedProcessorState> restore_state{};
};

struct CoordinatorHello
{
    InstanceBinding binding{};
    ProjectSnapshot snapshot{};
    ProjectSnapshot persistent_snapshot{};
    LibrarySnapshot library{};
    std::vector<InstanceBinding> instances{};
};

struct CommandRequest
{
    std::string request_id{};
    InstanceId source_instance_id{};
    std::string command{};
    CommandContext context{};
};

struct CommandResponse
{
    std::string request_id{};
    CommandApplicationResult result{};
    ProjectSnapshot snapshot{};
};

struct PreviewBeginRequest
{
    std::string request_id{};
    InstanceId source_instance_id{};
    ProjectRevision expected_project_revision{};
};

struct PreviewEndRequest
{
    std::string request_id{};
    InstanceId source_instance_id{};
    PreviewId preview_id{};
    ProjectRevision expected_project_revision{};
};

struct PreviewResponse
{
    std::string request_id{};
    PreviewControlResult result{};
    ProjectSnapshot snapshot{};
};

struct DocumentRequest
{
    std::string request_id{};
    InstanceId source_instance_id{};
    std::string operation{};
    std::string relative_path{};
    ProjectRevision expected_project_revision{};
    bool discard_unsaved{false};
    std::optional<std::string> expected_file_revision{};
    std::string recovery_revision{};
    CompositionCursor cursor{};
    std::optional<SelectionPath> selection{};
};

struct DocumentResponse
{
    std::string request_id{};
    DocumentOperationResult result{};
};

struct ProjectChanged
{
    ProjectSnapshot snapshot{};
};

struct LibraryChanged
{
    LibrarySnapshot snapshot{};
};

struct BindingSetRequest
{
    std::string request_id{};
    InstanceId instance_id{};
    ChannelId channel_id{};
};

struct BindingSetResponse
{
    std::string request_id{};
    InstanceBinding binding{};
    ProjectSnapshot snapshot{};
};

struct InstancesChanged
{
    std::vector<InstanceBinding> instances{};
};

struct Heartbeat
{
    std::uint64_t sequence{};
};

struct ShutdownIfIdleRequest
{
    std::string request_id{};
};

struct ShutdownIfIdleResponse
{
    std::string request_id{};
    bool will_exit{};
};

struct IpcError
{
    std::string request_id{};
    std::string code{};
    std::string message{};
    std::optional<std::string> current_file_revision{};
};

[[nodiscard]] auto encode_client_hello(ClientHello const &message) -> nlohmann::json;
[[nodiscard]] auto decode_client_hello(nlohmann::json const &message) -> ClientHello;

[[nodiscard]] auto encode_coordinator_hello(CoordinatorHello const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_coordinator_hello(nlohmann::json const &message)
    -> CoordinatorHello;

[[nodiscard]] auto encode_command_request(CommandRequest const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_command_request(nlohmann::json const &message)
    -> CommandRequest;

[[nodiscard]] auto encode_command_response(CommandResponse const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_command_response(nlohmann::json const &message)
    -> CommandResponse;
[[nodiscard]] auto encode_preview_begin_request(PreviewBeginRequest const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_preview_begin_request(nlohmann::json const &message)
    -> PreviewBeginRequest;
[[nodiscard]] auto encode_preview_commit_request(PreviewEndRequest const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_preview_commit_request(nlohmann::json const &message)
    -> PreviewEndRequest;
[[nodiscard]] auto encode_preview_cancel_request(PreviewEndRequest const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_preview_cancel_request(nlohmann::json const &message)
    -> PreviewEndRequest;
[[nodiscard]] auto encode_preview_response(PreviewResponse const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_preview_response(nlohmann::json const &message)
    -> PreviewResponse;

[[nodiscard]] auto encode_document_request(DocumentRequest const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_document_request(nlohmann::json const &message)
    -> DocumentRequest;
[[nodiscard]] auto encode_document_response(DocumentResponse const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_document_response(nlohmann::json const &message)
    -> DocumentResponse;

[[nodiscard]] auto encode_project_changed(ProjectChanged const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_project_changed(nlohmann::json const &message)
    -> ProjectChanged;

[[nodiscard]] auto encode_library_changed(LibraryChanged const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_library_changed(nlohmann::json const &message)
    -> LibraryChanged;

[[nodiscard]] auto encode_binding_set_request(BindingSetRequest const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_binding_set_request(nlohmann::json const &message)
    -> BindingSetRequest;

[[nodiscard]] auto encode_binding_set_response(BindingSetResponse const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_binding_set_response(nlohmann::json const &message)
    -> BindingSetResponse;

[[nodiscard]] auto encode_instances_changed(InstancesChanged const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_instances_changed(nlohmann::json const &message)
    -> InstancesChanged;

[[nodiscard]] auto encode_heartbeat(Heartbeat const &message) -> nlohmann::json;
[[nodiscard]] auto decode_heartbeat(nlohmann::json const &message) -> Heartbeat;

[[nodiscard]] auto encode_shutdown_if_idle_request(ShutdownIfIdleRequest const &message)
    -> nlohmann::json;
[[nodiscard]] auto decode_shutdown_if_idle_request(nlohmann::json const &message)
    -> ShutdownIfIdleRequest;

[[nodiscard]] auto encode_shutdown_if_idle_response(
    ShutdownIfIdleResponse const &message) -> nlohmann::json;
[[nodiscard]] auto decode_shutdown_if_idle_response(nlohmann::json const &message)
    -> ShutdownIfIdleResponse;

[[nodiscard]] auto encode_error(IpcError const &message) -> nlohmann::json;
[[nodiscard]] auto decode_error(nlohmann::json const &message) -> IpcError;

} // namespace xen::ipc
