#include <xen/webview_bridge_protocol.hpp>

#include <charconv>
#include <cstddef>
#include <limits>
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

auto require_composition_coordinate(nlohmann::json const &json,
                                    std::string_view field_name)
    -> CompositionCoordinate
{
    auto const key = std::string{field_name};
    if (!json.contains(key) ||
        (!json.at(key).is_number_integer() && !json.at(key).is_number_unsigned()))
        throw BridgeError{"invalid_request", "Field must be an integer: " + key};

    auto const value = json.at(key).get<std::int64_t>();
    if (value < std::numeric_limits<CompositionCoordinate>::min() ||
        value > std::numeric_limits<CompositionCoordinate>::max())
        throw BridgeError{"invalid_request",
                          "Composition coordinate is out of range: " + key};
    return static_cast<CompositionCoordinate>(value);
}

auto require_resource_revision(nlohmann::json const &json, std::string_view field_name)
    -> std::uint64_t
{
    auto const key = std::string{field_name};
    if (!json.contains(key) || !json.at(key).is_string())
    {
        throw BridgeError{"invalid_request",
                          "Field must be a decimal revision string: " + key};
    }

    auto const &text = json.at(key).get_ref<std::string const &>();
    auto revision = std::uint64_t{};
    auto const [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), revision);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size())
    {
        throw BridgeError{"invalid_request",
                          "Field must be a decimal revision string: " + key};
    }
    return revision;
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
    if (json_context.contains("preview_id"))
    {
        context.preview_id = require_string(json_context, "preview_id");
        if (context.preview_id->empty())
        {
            throw BridgeError{"invalid_request",
                              "Field must not be empty: context.preview_id"};
        }
    }

    auto const &cursor = require_object(json_context, "cursor");
    context.cursor = CompositionCursor{
        .row_coordinate = require_composition_coordinate(cursor, "row_coordinate"),
        .column_coordinate =
            require_composition_coordinate(cursor, "column_coordinate"),
        .sequence_id =
            cursor.at("sequence_id").is_null()
                ? std::optional<SequenceId>{}
                : std::optional<SequenceId>{require_unsigned(cursor, "sequence_id")},
    };

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
