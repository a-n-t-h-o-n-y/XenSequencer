#include <xen/webview_bridge.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include <xen/bridge_serialize.hpp>
#include <xen/command_catalog.hpp>
#include <xen/constants.hpp>
#include <xen/key_core.hpp>
#include <xen/user_directory.hpp>
#include <xen/xen_processor.hpp>

namespace
{

struct ParsedRequest
{
    std::string name{};
    std::optional<std::string> request_id{};
    nlohmann::json payload = nlohmann::json::object();
};

class BridgeError : public std::runtime_error
{
  public:
    BridgeError(std::string code_in, std::string message_in,
                std::string name_in = "bridge.error",
                std::optional<std::string> request_id_in = std::nullopt)
        : std::runtime_error(std::move(message_in)),
          code{std::move(code_in)}, name{std::move(name_in)},
          request_id{std::move(request_id_in)}
    {
    }

    std::string code{};
    std::string name{};
    std::optional<std::string> request_id{};
};

auto require_object(nlohmann::json const &json, std::string_view field_name)
    -> nlohmann::json const &
{
    auto const key = std::string{field_name};
    if (!json.contains(key) || !json.at(key).is_object())
    {
        throw BridgeError{"invalid_request",
                          "Field must be an object: " + key};
    }
    return json.at(key);
}

auto require_string(nlohmann::json const &json, std::string_view field_name)
    -> std::string
{
    auto const key = std::string{field_name};
    if (!json.contains(key) || !json.at(key).is_string())
    {
        throw BridgeError{"invalid_request",
                          "Field must be a string: " + key};
    }
    return json.at(key).get<std::string>();
}

void require_integer_equals(nlohmann::json const &json, std::string_view field_name,
                            int expected)
{
    auto const key = std::string{field_name};
    if (!json.contains(key) || !json.at(key).is_number_integer())
    {
        throw BridgeError{"invalid_request",
                          "Field must be an integer: " + key};
    }
    if (json.at(key).get<int>() != expected)
    {
        throw BridgeError{"unsupported_protocol",
                          "Unsupported " + key};
    }
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

    auto const protocol = require_string(parsed, "protocol");
    if (protocol != xen::bridge::protocol)
    {
        throw BridgeError{
            "unsupported_protocol",
            "Unsupported protocol: " + protocol,
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
                   std::optional<std::string> const &request_id,
                   nlohmann::json payload) -> nlohmann::json
{
    auto out = nlohmann::json{
        {"protocol", xen::bridge::protocol},
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
    auto const protocol = require_string(payload, "protocol");
    if (protocol != xen::bridge::protocol)
    {
        throw BridgeError{
            "unsupported_protocol",
            "Unsupported protocol: " + protocol,
            request.name,
            request.request_id,
        };
    }

    require_integer_equals(payload, "snapshot_schema_version",
                           xen::bridge::snapshot_schema_version);
    (void)require_string(payload, "frontend_app");
    (void)require_string(payload, "frontend_version");
}

} // namespace

namespace xen
{

WebviewBridge::WebviewBridge(XenProcessor &processor) : processor_{processor}
{
}

auto WebviewBridge::handle_request_json(std::string const &request_json) -> std::string
{
    auto request = ParsedRequest{};

    try
    {
        request = parse_request(request_json);

        auto payload = nlohmann::json::object();

        if (request.name == "session.hello")
        {
            validate_session_hello_payload(request.payload, request);
            payload = nlohmann::json{
                {"protocol", bridge::protocol},
                {"snapshot_schema_version", bridge::snapshot_schema_version},
                {"plugin_version", VERSION},
            };
        }
        else if (request.name == "state.get")
        {
            validate_empty_object_payload(request.payload, request);
            payload = bridge::make_ui_state_snapshot(
                processor_.get_engine_snapshot(), processor_.plugin_state.library);
        }
        else if (request.name == "command.execute")
        {
            auto const command = require_string(request.payload, "command");
            auto const [level, message] = processor_.execute_command_string(command);
            payload = nlohmann::json{
                {"status",
                 {
                     {"level", bridge::to_string(level)},
                     {"message", message},
                 }},
                {"snapshot",
                 bridge::make_ui_state_snapshot(processor_.get_engine_snapshot(),
                                                processor_.plugin_state.library)},
            };
        }
        else if (request.name == "command.completeText")
        {
            auto const partial = require_string(request.payload, "partial");
            payload = nlohmann::json{
                {"suffix", catalog_complete_text(partial)},
            };
        }
        else if (request.name == "command.completeId")
        {
            auto const partial = require_string(request.payload, "partial");
            payload = nlohmann::json{
                {"id_suffix", catalog_complete_id(partial)},
            };
        }
        else if (request.name == "catalog.get")
        {
            validate_empty_object_payload(request.payload, request);
            payload = bridge::make_catalog_payload(command_metadata());
        }
        else if (request.name == "keymap.get")
        {
            validate_empty_object_payload(request.payload, request);
            payload = bridge::make_keymap_payload(
                export_merged_keymap(get_system_keys_file(), get_user_keys_file()));
        }
        else
        {
            throw BridgeError{
                "invalid_request",
                "Unknown request name: " + request.name,
                request.name,
                request.request_id,
            };
        }

        return make_envelope("response", request.name, request.request_id, payload).dump();
    }
    catch (BridgeError const &error)
    {
        return make_envelope("response", error.name, error.request_id,
                             make_error_payload(error.code, error.what()))
            .dump();
    }
    catch (nlohmann::json::exception const &error)
    {
        return make_envelope("response",
                             request.name.empty() ? "bridge.error" : request.name,
                             request.request_id,
                             make_error_payload("invalid_request", error.what()))
            .dump();
    }
    catch (std::exception const &error)
    {
        return make_envelope("response",
                             request.name.empty() ? "bridge.error" : request.name,
                             request.request_id,
                             make_error_payload("internal_error", error.what()))
            .dump();
    }
    catch (...)
    {
        return make_envelope("response",
                             request.name.empty() ? "bridge.error" : request.name,
                             request.request_id,
                             make_error_payload("internal_error", "Unknown error."))
            .dump();
    }
}

auto WebviewBridge::make_state_changed_event_json() const -> std::string
{
    auto const payload = bridge::make_ui_state_snapshot(processor_.get_engine_snapshot(),
                                                        processor_.plugin_state.library);
    return make_envelope("event", "state.changed", std::nullopt, payload).dump();
}

} // namespace xen
