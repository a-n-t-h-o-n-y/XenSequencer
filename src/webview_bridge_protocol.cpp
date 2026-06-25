#include <xen/webview_bridge_protocol.hpp>

#include <cstddef>
#include <utility>

#include <xen/bridge_serialize.hpp>

namespace xen::bridge
{

BridgeError::BridgeError(std::string code_in, std::string message_in,
                         std::string name_in, std::optional<std::string> request_id_in)
    : std::runtime_error(std::move(message_in)), code{std::move(code_in)},
      name{std::move(name_in)}, request_id{std::move(request_id_in)}
{
}

auto require_object(nlohmann::json const &json, std::string_view field_name)
    -> nlohmann::json const &
{
    auto const key = std::string{field_name};
    if (!json.contains(key) || !json.at(key).is_object())
    {
        throw BridgeError{"invalid_request", "Field must be an object: " + key};
    }
    return json.at(key);
}

auto require_string(nlohmann::json const &json, std::string_view field_name)
    -> std::string
{
    auto const key = std::string{field_name};
    if (!json.contains(key) || !json.at(key).is_string())
    {
        throw BridgeError{"invalid_request", "Field must be a string: " + key};
    }
    return json.at(key).get<std::string>();
}

auto require_unsigned(nlohmann::json const &json, std::string_view field_name)
    -> std::uint64_t
{
    auto const key = std::string{field_name};
    if (!json.contains(key) || !json.at(key).is_number_unsigned())
    {
        throw BridgeError{"invalid_request",
                          "Field must be an unsigned integer: " + key};
    }
    return json.at(key).get<std::uint64_t>();
}

auto parse_keymap_trigger(nlohmann::json const &json) -> KeymapTrigger
{
    if (!json.is_object())
    {
        throw BridgeError{"invalid_request", "Keymap trigger must be an object."};
    }
    auto const &modifiers = require_object(json, "modifiers");
    auto const require_boolean = [](nlohmann::json const &object,
                                    std::string_view field) {
        auto const key = std::string{field};
        if (!object.contains(key) || !object.at(key).is_boolean())
        {
            throw BridgeError{"invalid_request",
                              "Field must be a boolean: modifiers." + key};
        }
        return object.at(key).get<bool>();
    };

    auto value = KeymapTrigger{
        .key = require_string(json, "key"),
        .shift = require_boolean(modifiers, "shift"),
        .command = require_boolean(modifiers, "command"),
        .alt = require_boolean(modifiers, "alt"),
    };
    if (json.contains("when"))
    {
        value.input_mode = require_string(require_object(json, "when"), "input_mode");
    }
    validate(value);
    return value;
}

auto parse_keymap_target(nlohmann::json const &json) -> KeymapTarget
{
    if (!json.is_object())
    {
        throw BridgeError{"invalid_request", "Keymap target must be an object."};
    }
    auto const type = require_string(json, "type");
    auto value = KeymapTarget{};
    if (type == "command")
    {
        value.type = KeymapTargetType::Command;
        value.value = require_string(json, "command");
    }
    else if (type == "ui_action")
    {
        value.type = KeymapTargetType::UiAction;
        value.value = require_string(json, "action");
        value.arguments = require_object(json, "arguments");
    }
    else
    {
        throw BridgeError{"invalid_request", "Unknown keymap target type: " + type};
    }
    validate(value);
    return value;
}

auto parse_command_context(nlohmann::json const &payload) -> CommandContext
{
    auto context = CommandContext{};
    if (!payload.contains("context"))
    {
        return context;
    }

    auto const &json_context = require_object(payload, "context");
    if (json_context.contains("expected_project_revision"))
    {
        auto const &revision = json_context.at("expected_project_revision");
        if (!revision.is_number_unsigned())
        {
            throw BridgeError{
                "invalid_request",
                "Field must be an unsigned integer: context.expected_project_revision",
            };
        }
        context.expected_project_revision =
            ProjectRevision{revision.get<std::uint64_t>()};
    }

    if (json_context.contains("active_measure_target") &&
        !json_context.at("active_measure_target").is_null())
    {
        auto const &target = require_object(json_context, "active_measure_target");
        context.active_measure_target = ActiveMeasureTarget{
            .row_index =
                static_cast<std::size_t>(require_unsigned(target, "row_index")),
            .column_index =
                static_cast<std::size_t>(require_unsigned(target, "column_index")),
            .measure_id = require_unsigned(target, "measure_id"),
        };
    }

    if (json_context.contains("selection"))
    {
        auto const &selection = require_object(json_context, "selection");
        auto const &path_json = selection.at("path");
        if (!path_json.is_array())
        {
            throw BridgeError{"invalid_request",
                              "Field must be an array: context.selection.path"};
        }

        auto path = SelectionPath{};
        for (auto const &step_json : path_json)
        {
            if (!step_json.is_object())
            {
                throw BridgeError{
                    "invalid_request",
                    "Selection steps must be objects: context.selection.path",
                };
            }

            auto const kind = require_string(step_json, "kind");
            auto const &index_json = step_json.at("index");
            if (!index_json.is_number_unsigned())
            {
                throw BridgeError{
                    "invalid_request",
                    "Field must be an unsigned integer: context.selection.path[].index",
                };
            }

            auto step = SelectionStep{.index = index_json.get<std::size_t>()};
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
                throw BridgeError{
                    "invalid_request",
                    "Invalid selection step kind: " + kind,
                };
            }

            path.path.push_back(step);
        }
        context.selection = std::move(path);
    }
    return context;
}

auto selection_to_json(std::optional<SelectionPath> const &selection) -> nlohmann::json
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

auto parse_request(std::string const &request_json) -> ParsedRequest
{
    auto const parsed = nlohmann::json::parse(request_json);
    if (!parsed.is_object())
    {
        throw BridgeError{"invalid_request", "Request must be a JSON object."};
    }

    auto request = ParsedRequest{};
    request.name = require_string(parsed, "name");

    if (parsed.contains("request_id"))
    {
        if (!parsed.at("request_id").is_string())
        {
            throw BridgeError{
                "invalid_request",
                "Field must be a string: request_id",
                request.name,
            };
        }
        request.request_id = parsed.at("request_id").get<std::string>();
    }

    auto const requested_protocol = require_string(parsed, "protocol");
    if (requested_protocol != protocol)
    {
        throw BridgeError{
            "unsupported_protocol",
            "Unsupported protocol: " + requested_protocol,
            request.name,
            request.request_id,
        };
    }

    auto const type = require_string(parsed, "type");
    if (type != "request")
    {
        throw BridgeError{
            "invalid_request",
            "Field 'type' must be 'request'.",
            request.name,
            request.request_id,
        };
    }

    request.payload = require_object(parsed, "payload");
    return request;
}

auto make_envelope(std::string_view type, std::string const &name,
                   std::optional<std::string> const &request_id, nlohmann::json payload)
    -> nlohmann::json
{
    auto out = nlohmann::json{
        {"protocol", protocol},
        {"type", std::string{type}},
        {"name", name},
        {"payload", std::move(payload)},
    };

    if (request_id.has_value())
    {
        out["request_id"] = *request_id;
    }
    return out;
}

auto make_error_payload(std::string const &code, std::string const &message)
    -> nlohmann::json
{
    return nlohmann::json{
        {"error",
         {
             {"code", code},
             {"message", message},
         }},
    };
}

void validate_empty_object_payload(nlohmann::json const &payload,
                                   ParsedRequest const &request)
{
    if (!payload.empty())
    {
        throw BridgeError{
            "invalid_request",
            "Payload must be an empty object.",
            request.name,
            request.request_id,
        };
    }
}

void validate_session_hello_payload(nlohmann::json const &payload,
                                    ParsedRequest const &request)
{
    auto const requested_protocol = require_string(payload, "protocol");
    if (requested_protocol != protocol)
    {
        throw BridgeError{
            "unsupported_protocol",
            "Unsupported protocol: " + requested_protocol,
            request.name,
            request.request_id,
        };
    }

    (void)require_string(payload, "frontend_app");
    (void)require_string(payload, "frontend_version");
}

} // namespace xen::bridge
