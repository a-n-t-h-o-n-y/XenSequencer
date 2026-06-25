#include <xen/ipc_protocol.hpp>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <xen/bridge_serialize.hpp>
#include <xen/serialize.hpp>

namespace xen::ipc
{
namespace
{

[[nodiscard]] auto require_protocol(nlohmann::json const &message, char const *type)
    -> nlohmann::json const &
{
    if (!message.is_object())
    {
        throw std::invalid_argument{"IPC message must be an object."};
    }
    if (message.at("protocol").get<std::string>() != protocol)
    {
        throw std::invalid_argument{"Unsupported IPC protocol."};
    }
    if (message.at("type").get<std::string>() != type)
    {
        throw std::invalid_argument{"Unexpected IPC message type."};
    }
    return message.at("payload");
}

[[nodiscard]] auto binding_to_json(InstanceBinding const &binding) -> nlohmann::json
{
    if (binding.session_id.empty())
    {
        throw std::invalid_argument{"Session ID must not be empty."};
    }
    if (binding.instance_id.empty())
    {
        throw std::invalid_argument{"Instance ID must not be empty."};
    }
    if (binding.output_id.empty())
    {
        throw std::invalid_argument{"Output ID must not be empty."};
    }
    return {
        {"session_id", binding.session_id},
        {"instance_id", binding.instance_id},
        {"output_id", binding.output_id},
    };
}

[[nodiscard]] auto binding_from_json(nlohmann::json const &json) -> InstanceBinding
{
    auto binding = InstanceBinding{
        .session_id = json.at("session_id").get<SessionId>(),
        .instance_id = json.at("instance_id").get<InstanceId>(),
        .output_id = json.at("output_id").get<OutputId>(),
    };
    (void)binding_to_json(binding);
    return binding;
}

[[nodiscard]] auto snapshot_to_json(ProjectSnapshot const &snapshot) -> nlohmann::json
{
    return {
        {"history_entry_id", snapshot.history_entry_id.value()},
        {"project_revision", snapshot.project_revision.value()},
        {"project", nlohmann::json::parse(serialize_project(snapshot.project))},
    };
}

[[nodiscard]] auto snapshot_from_json(nlohmann::json const &json) -> ProjectSnapshot
{
    return {
        .project = deserialize_project(json.at("project").dump()),
        .history_entry_id =
            HistoryEntryId{json.at("history_entry_id").get<std::uint64_t>()},
        .project_revision =
            ProjectRevision{json.at("project_revision").get<std::uint64_t>()},
    };
}

[[nodiscard]] auto selection_to_json(std::optional<SelectionPath> const &selection)
    -> nlohmann::json
{
    if (!selection.has_value())
    {
        return nullptr;
    }

    auto path = nlohmann::json::array();
    for (auto const &step : selection->path)
    {
        path.push_back({
            {"kind", step.kind == SelectionStepKind::Element ? "element" : "cell"},
            {"index", step.index},
        });
    }
    return nlohmann::json{{"path", std::move(path)}};
}

[[nodiscard]] auto selection_from_json(nlohmann::json const &json)
    -> std::optional<SelectionPath>
{
    if (json.is_null())
    {
        return std::nullopt;
    }

    auto selection = SelectionPath{};
    for (auto const &step_json : json.at("path"))
    {
        auto const kind = step_json.at("kind").get<std::string>();
        auto step = SelectionStep{.index = step_json.at("index").get<std::size_t>()};
        if (kind == "element")
        {
            step.kind = SelectionStepKind::Element;
        }
        else if (kind == "cell")
        {
            step.kind = SelectionStepKind::SequenceCell;
        }
        else
        {
            throw std::invalid_argument{"Invalid selection step kind."};
        }
        selection.path.push_back(step);
    }
    return selection;
}

[[nodiscard]] auto command_context_to_json(CommandContext const &context)
    -> nlohmann::json
{
    return {
        {"expected_project_revision",
         context.expected_project_revision.has_value()
             ? nlohmann::json(context.expected_project_revision->value())
             : nlohmann::json{nullptr}},
        {"selection", selection_to_json(context.selection)},
    };
}

[[nodiscard]] auto command_context_from_json(nlohmann::json const &json)
    -> CommandContext
{
    auto context = CommandContext{};
    if (!json.at("expected_project_revision").is_null())
    {
        context.expected_project_revision =
            ProjectRevision{json.at("expected_project_revision").get<std::uint64_t>()};
    }
    context.selection = selection_from_json(json.at("selection"));
    return context;
}

[[nodiscard]] auto status_to_json(CommandStatus const &status) -> nlohmann::json
{
    return {
        {"level", bridge::to_string(status.first)},
        {"message", status.second},
    };
}

[[nodiscard]] auto status_from_json(nlohmann::json const &json) -> CommandStatus
{
    auto const level = json.at("level").get<std::string>();
    auto message = json.at("message").get<std::string>();
    if (level == "debug")
    {
        return {MessageLevel::Debug, std::move(message)};
    }
    if (level == "info")
    {
        return {MessageLevel::Info, std::move(message)};
    }
    if (level == "warning")
    {
        return {MessageLevel::Warning, std::move(message)};
    }
    if (level == "error")
    {
        return {MessageLevel::Error, std::move(message)};
    }
    throw std::invalid_argument{"Invalid command status level."};
}

[[nodiscard]] auto envelope(std::string type, nlohmann::json payload) -> nlohmann::json
{
    return {
        {"protocol", protocol},
        {"type", std::move(type)},
        {"payload", std::move(payload)},
    };
}

} // namespace

auto encode_client_hello(ClientHello const &message) -> nlohmann::json
{
    auto payload = nlohmann::json{
        {"binding", binding_to_json(message.binding)},
        {"restore_state", nullptr},
    };
    if (message.restore_state.has_value())
    {
        payload["restore_state"] = {
            {"binding", binding_to_json(message.restore_state->binding)},
            {"saved_history_entry_id",
             message.restore_state->saved_history_entry_id.value()},
            {"saved_project_revision",
             message.restore_state->saved_project_revision.value()},
            {"project",
             nlohmann::json::parse(serialize_project(message.restore_state->project))},
        };
    }
    return envelope("client.hello", std::move(payload));
}

auto decode_client_hello(nlohmann::json const &message) -> ClientHello
{
    auto const &payload = require_protocol(message, "client.hello");
    auto hello = ClientHello{.binding = binding_from_json(payload.at("binding"))};
    if (!payload.at("restore_state").is_null())
    {
        auto const &restore = payload.at("restore_state");
        hello.restore_state = PersistedProcessorState{
            .binding = binding_from_json(restore.at("binding")),
            .project = deserialize_project(restore.at("project").dump()),
            .saved_history_entry_id =
                HistoryEntryId{
                    restore.at("saved_history_entry_id").get<std::uint64_t>()},
            .saved_project_revision =
                ProjectRevision{
                    restore.at("saved_project_revision").get<std::uint64_t>()},
        };
    }
    return hello;
}

auto encode_coordinator_hello(CoordinatorHello const &message) -> nlohmann::json
{
    return envelope("coordinator.hello",
                    {
                        {"binding", binding_to_json(message.binding)},
                        {"snapshot", snapshot_to_json(message.snapshot)},
                    });
}

auto decode_coordinator_hello(nlohmann::json const &message) -> CoordinatorHello
{
    auto const &payload = require_protocol(message, "coordinator.hello");
    return {
        .binding = binding_from_json(payload.at("binding")),
        .snapshot = snapshot_from_json(payload.at("snapshot")),
    };
}

auto encode_command_request(CommandRequest const &message) -> nlohmann::json
{
    if (message.request_id.empty())
    {
        throw std::invalid_argument{"Command request ID must not be empty."};
    }
    if (message.source_instance_id.empty())
    {
        throw std::invalid_argument{"Source instance ID must not be empty."};
    }
    return envelope("command.execute",
                    {
                        {"request_id", message.request_id},
                        {"source_instance_id", message.source_instance_id},
                        {"command", message.command},
                        {"context", command_context_to_json(message.context)},
                    });
}

auto decode_command_request(nlohmann::json const &message) -> CommandRequest
{
    auto const &payload = require_protocol(message, "command.execute");
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .source_instance_id = payload.at("source_instance_id").get<InstanceId>(),
        .command = payload.at("command").get<std::string>(),
        .context = command_context_from_json(payload.at("context")),
    };
}

auto encode_command_response(CommandResponse const &message) -> nlohmann::json
{
    return envelope("command.result",
                    {
                        {"request_id", message.request_id},
                        {"status", status_to_json(message.result.status)},
                        {"suggested_selection",
                         selection_to_json(message.result.suggested_selection)},
                        {"snapshot", snapshot_to_json(message.snapshot)},
                    });
}

auto decode_command_response(nlohmann::json const &message) -> CommandResponse
{
    auto const &payload = require_protocol(message, "command.result");
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .result =
            CommandApplicationResult{
                .status = status_from_json(payload.at("status")),
                .suggested_selection =
                    selection_from_json(payload.at("suggested_selection")),
            },
        .snapshot = snapshot_from_json(payload.at("snapshot")),
    };
}

} // namespace xen::ipc
