#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include <xen/command_catalog_types.hpp>
#include <xen/state.hpp>

namespace xen::ipc
{

inline constexpr auto protocol = "xen.ipc.v1";

struct ClientHello
{
    InstanceBinding binding{};
    std::optional<PersistedProcessorState> restore_state{};
};

struct CoordinatorHello
{
    InstanceBinding binding{};
    ProjectSnapshot snapshot{};
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

} // namespace xen::ipc
