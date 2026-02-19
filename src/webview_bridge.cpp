#include <xen/webview_bridge.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include <sequence/tuning.hpp>

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

auto to_sorted_files(juce::Array<juce::File> const &files) -> std::vector<juce::File>
{
    auto result = std::vector<juce::File>{};
    result.reserve((std::size_t)files.size());
    for (auto const &file : files)
    {
        result.push_back(file);
    }

    std::sort(result.begin(), result.end(),
              [](juce::File const &lhs, juce::File const &rhs) {
                  return lhs.getFileName().compareNatural(rhs.getFileName()) < 0;
              });
    return result;
}

auto quote_command_arg(std::string const &value) -> std::string
{
    auto escaped = std::string{};
    escaped.reserve(value.size());
    for (auto const ch : value)
    {
        if (ch == '\\' || ch == '"')
        {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return "\"" + escaped + "\"";
}

auto normalize_utf8(std::string_view input) -> std::string
{
    auto out = std::string{};
    out.reserve(input.size());

    auto const *bytes = reinterpret_cast<unsigned char const *>(input.data());
    auto const n = input.size();
    auto i = std::size_t{0};

    auto append_replacement = [&out] { out += "\xEF\xBF\xBD"; };

    while (i < n)
    {
        auto const b0 = bytes[i];
        if (b0 <= 0x7F)
        {
            out.push_back((char)b0);
            ++i;
            continue;
        }

        auto need = std::size_t{0};
        std::uint32_t codepoint = 0;
        if ((b0 & 0xE0) == 0xC0)
        {
            need = 2;
            codepoint = b0 & 0x1F;
            if (codepoint < 0x2)
            {
                append_replacement();
                ++i;
                continue;
            }
        }
        else if ((b0 & 0xF0) == 0xE0)
        {
            need = 3;
            codepoint = b0 & 0x0F;
        }
        else if ((b0 & 0xF8) == 0xF0)
        {
            need = 4;
            codepoint = b0 & 0x07;
        }
        else
        {
            append_replacement();
            ++i;
            continue;
        }

        if (i + need > n)
        {
            append_replacement();
            break;
        }

        auto valid = true;
        for (auto j = std::size_t{1}; j < need; ++j)
        {
            auto const bx = bytes[i + j];
            if ((bx & 0xC0) != 0x80)
            {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (bx & 0x3F);
        }

        if (!valid)
        {
            append_replacement();
            ++i;
            continue;
        }

        if ((need == 3 && codepoint < 0x800) ||
            (need == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF) ||
            codepoint > 0x10FFFF)
        {
            append_replacement();
            ++i;
            continue;
        }

        out.append(input.substr(i, need));
        i += need;
    }

    return out;
}

auto make_file_entries(juce::File const &directory, std::string const &glob,
                       std::string const &command_prefix) -> nlohmann::json
{
    if (!directory.isDirectory())
    {
        throw std::runtime_error("Invalid library directory: " +
                                 directory.getFullPathName().toStdString());
    }

    auto const files = to_sorted_files(
        directory.findChildFiles(juce::File::findFiles, true, glob));

    auto out = nlohmann::json::array();
    for (auto const &file : files)
    {
        auto relative_path =
            file.getRelativePathFrom(directory).replaceCharacter('\\', '/');
        auto stem_path = relative_path.toStdString();
        if (auto const dot = stem_path.find_last_of('.');
            dot != std::string::npos)
        {
            stem_path.erase(dot);
        }

        out.push_back(nlohmann::json{
            {"name", normalize_utf8(file.getFileName().toStdString())},
            {"relative_path", normalize_utf8(relative_path.toStdString())},
            {"stem", normalize_utf8(stem_path)},
            {"path", normalize_utf8(file.getFullPathName().toStdString())},
            {"command",
             normalize_utf8(command_prefix + quote_command_arg(stem_path))},
        });
    }
    return out;
}

auto make_tuning_entries(juce::File const &directory) -> nlohmann::json
{
    if (!directory.isDirectory())
    {
        throw std::runtime_error("Invalid library directory: " +
                                 directory.getFullPathName().toStdString());
    }

    auto const files = to_sorted_files(
        directory.findChildFiles(juce::File::findFiles, true, "*.scl"));

    auto out = nlohmann::json::array();
    for (auto const &file : files)
    {
        auto relative_path =
            file.getRelativePathFrom(directory).replaceCharacter('\\', '/');
        auto stem_path = relative_path.toStdString();
        if (auto const dot = stem_path.find_last_of('.');
            dot != std::string::npos)
        {
            stem_path.erase(dot);
        }

        auto const tuning = sequence::from_scala(
            file.getFullPathName().toStdString());

        out.push_back(nlohmann::json{
            {"name", normalize_utf8(file.getFileName().toStdString())},
            {"relative_path", normalize_utf8(relative_path.toStdString())},
            {"stem", normalize_utf8(stem_path)},
            {"path", normalize_utf8(file.getFullPathName().toStdString())},
            {"command",
             normalize_utf8("load tuning " + quote_command_arg(stem_path))},
            {"description", normalize_utf8(tuning.description)},
            {"intervals", tuning.intervals},
            {"octave", tuning.octave},
            {"note_count", tuning.intervals.size()},
        });
    }
    return out;
}

auto make_library_payload(xen::XenProcessor const &processor) -> nlohmann::json
{
    auto const snapshot = processor.get_engine_snapshot();
    auto const &config = processor.plugin_state.config;
    auto const &library = processor.plugin_state.library;

    auto scales = nlohmann::json::array();
    scales.push_back(nlohmann::json{
        {"name", normalize_utf8("chromatic")},
        {"intervals", nlohmann::json::array()},
        {"command",
         normalize_utf8("set scale " + quote_command_arg("chromatic"))},
    });
    for (auto const &scale : library.scales)
    {
        scales.push_back(nlohmann::json{
            {"name", normalize_utf8(scale.name)},
            {"intervals", scale.intervals},
            {"command", normalize_utf8("set scale " + quote_command_arg(scale.name))},
        });
    }

    auto chords = nlohmann::json::array();
    for (auto const &chord : library.chords)
    {
        chords.push_back(nlohmann::json{
            {"name", normalize_utf8(chord.name)},
            {"intervals", chord.intervals},
            {"command", normalize_utf8("arp " + quote_command_arg(chord.name))},
        });
    }

    auto active_scale = nlohmann::json{};
    if (snapshot.engine.scale.has_value())
    {
        active_scale = normalize_utf8(snapshot.engine.scale->name);
    }
    else
    {
        active_scale = nullptr;
    }

    return nlohmann::json{
        {"paths",
         {
             {"library",
              normalize_utf8(
                  xen::get_user_library_directory().getFullPathName().toStdString())},
             {"sequences",
              normalize_utf8(config.current_sequence_directory.getFullPathName()
                                 .toStdString())},
             {"tunings",
              normalize_utf8(config.current_tuning_directory.getFullPathName()
                                 .toStdString())},
         }},
        {"sequence_banks",
         make_file_entries(config.current_sequence_directory, "*.xss",
                           "load sequenceBank ")},
        {"tunings", make_tuning_entries(config.current_tuning_directory)},
        {"scales", std::move(scales)},
        {"chords", std::move(chords)},
        {"commands",
         {
             {"reload_scales", "load scales"},
             {"reload_chords", "load chords"},
             {"library_directory", "libraryDirectory"},
         }},
        {"active",
         {
             {"tuning_name", normalize_utf8(snapshot.engine.tuning_name)},
             {"scale_name", std::move(active_scale)},
         }},
    };
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
            auto const keymap =
                export_merged_keymap(get_system_keys_file(), get_user_keys_file());
            payload = nlohmann::json{
                {"protocol", bridge::protocol},
                {"snapshot_schema_version", bridge::snapshot_schema_version},
                {"plugin_version", VERSION},
                {"reference",
                 bridge::make_reference_payload(catalog_docs(), keymap)},
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
        else if (request.name == "library.get")
        {
            validate_empty_object_payload(request.payload, request);
            payload = make_library_payload(processor_);
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

auto WebviewBridge::make_trigger_note_event_json(std::size_t sequence_index,
                                                 bool active) const -> std::string
{
    auto const payload = nlohmann::json{
        {"sequence_index", sequence_index},
    };
    auto const name = active ? "transport.trigger.noteOn"
                             : "transport.trigger.noteOff";
    return make_envelope("event", name, std::nullopt, payload).dump();
}

auto WebviewBridge::make_phase_sync_event_json(
    std::vector<SequencePhase> const &phases, float bpm) const -> std::string
{
    auto payload_phases = nlohmann::json::array();
    for (auto const &phase : phases)
    {
        payload_phases.push_back({
            {"sequence_index", phase.sequence_index},
            {"phase", phase.phase},
        });
    }

    auto const payload = nlohmann::json{
        {"bpm", bpm},
        {"phases", std::move(payload_phases)},
    };
    return make_envelope("event", "transport.phase.sync", std::nullopt, payload).dump();
}

} // namespace xen
