#include <xen/webview_bridge_services.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <utility>

#include <sequence/tuning.hpp>

#include <xen/bridge_serialize.hpp>
#include <xen/constants.hpp>
#include <xen/user_directory.hpp>

namespace xen::bridge
{

namespace
{

auto as_juce_file(std::filesystem::path const &path) -> juce::File
{
    return juce::File{path.string()};
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

auto file_entry_to_json(LibraryFileEntry const &file, std::string const &command_prefix)
    -> nlohmann::json
{
    return {
        {"name", normalize_utf8(file.name)},
        {"relative_path", normalize_utf8(file.relative_path)},
        {"stem", normalize_utf8(file.stem)},
        {"path", normalize_utf8(file.path)},
        {"command", normalize_utf8(command_prefix + quote_command_arg(file.stem))},
    };
}

auto file_entries_to_json(std::vector<LibraryFileEntry> const &files,
                          std::string const &command_prefix) -> nlohmann::json
{
    auto out = nlohmann::json::array();
    for (auto const &file : files)
    {
        out.push_back(file_entry_to_json(file, command_prefix));
    }
    return out;
}

auto tuning_entries_to_json(std::vector<LibraryTuningEntry> const &tunings)
    -> nlohmann::json
{
    auto out = nlohmann::json::array();
    for (auto const &tuning : tunings)
    {
        auto json = file_entry_to_json(tuning.file, "load tuning ");
        json["description"] = normalize_utf8(tuning.description);
        json["intervals"] = tuning.intervals;
        json["octave"] = tuning.octave;
        json["note_count"] = tuning.intervals.size();
        out.push_back(std::move(json));
    }
    return out;
}

auto make_library_file_entry(juce::File const &directory, juce::File const &file)
    -> LibraryFileEntry
{
    auto relative_path =
        file.getRelativePathFrom(directory).replaceCharacter('\\', '/');
    auto stem_path = relative_path.toStdString();
    if (auto const dot = stem_path.find_last_of('.'); dot != std::string::npos)
    {
        stem_path.erase(dot);
    }

    return {
        .name = file.getFileName().toStdString(),
        .relative_path = relative_path.toStdString(),
        .stem = stem_path,
        .path = file.getFullPathName().toStdString(),
    };
}

} // namespace

SequencerApplicationBridgeService::SequencerApplicationBridgeService(
    SequencerSession &session)
    : session_{session}
{
}

auto SequencerApplicationBridgeService::project_snapshot() const -> ProjectSnapshot
{
    return session_.project_snapshot();
}

auto SequencerApplicationBridgeService::instance_binding() const -> InstanceBinding
{
    return session_.instance_binding();
}

auto SequencerApplicationBridgeService::library_snapshot() const -> LibrarySnapshot
{
    return session_.library_snapshot();
}

auto SequencerApplicationBridgeService::command_catalog_metadata() const
    -> std::vector<CatalogCommandMetadata>
{
    return session_.command_catalog().metadata();
}

auto SequencerApplicationBridgeService::execute_command_string(
    std::string const &command, CommandContext const &context)
    -> CommandApplicationResult
{
    return session_.execute_command_string(command, context);
}

void SequencerApplicationBridgeService::set_output_id(OutputId output_id)
{
    session_.set_output_id(std::move(output_id));
}

StoreKeymapBridgeService::StoreKeymapBridgeService(std::filesystem::path keymap_file)
    : store_{std::move(keymap_file)}
{
}

auto StoreKeymapBridgeService::snapshot() const -> KeymapSnapshot
{
    return store_.snapshot();
}

auto StoreKeymapBridgeService::revision() const noexcept -> std::uint64_t
{
    return store_.revision();
}

auto StoreKeymapBridgeService::set_override(std::uint64_t expected_revision,
                                            std::string context, KeymapTrigger trigger,
                                            std::optional<KeymapTarget> target)
    -> KeymapSnapshot
{
    return store_.set_override(expected_revision, std::move(context),
                               std::move(trigger), std::move(target));
}

auto StoreKeymapBridgeService::remove_override(std::uint64_t expected_revision,
                                               std::string const &context,
                                               KeymapTrigger const &trigger)
    -> KeymapSnapshot
{
    return store_.remove_override(expected_revision, context, trigger);
}

auto StoreKeymapBridgeService::reset(std::uint64_t expected_revision) -> KeymapSnapshot
{
    return store_.reset(expected_revision);
}

JuceLibraryBridgeService::JuceLibraryBridgeService(LibraryFilePort &files)
    : files_{files}
{
}

auto JuceLibraryBridgeService::make_payload(LibrarySnapshot const &snapshot) const
    -> nlohmann::json
{
    auto const &workspace = snapshot.workspace;
    auto const &library = snapshot.library;

    auto scales = nlohmann::json::array();
    scales.push_back(nlohmann::json{
        {"id", "chromatic"},
        {"name", normalize_utf8("chromatic")},
        {"definition", nullptr},
        {"intervals", nlohmann::json::array()},
        {"command", normalize_utf8("set scale " + quote_command_arg("chromatic"))},
    });
    for (auto const &scale : library.scales)
    {
        scales.push_back(nlohmann::json{
            {"id", normalize_utf8(scale.id)},
            {"definition",
             {
                 {"name", normalize_utf8(scale.definition.name)},
                 {"tuning_length", scale.definition.tuning_length},
                 {"intervals", scale.definition.intervals},
                 {"mode", scale.definition.mode},
             }},
            {"command", normalize_utf8("set scale " + quote_command_arg(scale.id))},
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

    return nlohmann::json{
        {"schema_version", library_schema_version},
        {"library_revision", snapshot.library_revision.value()},
        {"paths",
         {
             {"library", normalize_utf8(files_.library_root().string())},
             {"sequences", normalize_utf8(workspace.sequence_directory.string())},
             {"tunings", normalize_utf8(workspace.tuning_directory.string())},
         }},
        {"measures",
         file_entries_to_json(files_.measure_files(workspace.sequence_directory),
                              "load measure ")},
        {"tunings",
         tuning_entries_to_json(files_.tuning_files(workspace.tuning_directory))},
        {"scales", std::move(scales)},
        {"chords", std::move(chords)},
        {"commands",
         {
             {"reload_scales", "load scales"},
             {"reload_chords", "load chords"},
             {"library_directory", "libraryDirectory"},
         }},
    };
}

auto JuceLibraryFilePort::library_root() const -> std::filesystem::path
{
    return get_user_library_directory().getFullPathName().toStdString();
}

auto JuceLibraryFilePort::measure_files(
    std::filesystem::path const &directory_path) const -> std::vector<LibraryFileEntry>
{
    auto const directory = as_juce_file(directory_path);
    if (!directory.isDirectory())
    {
        throw std::runtime_error("Invalid library directory: " +
                                 directory.getFullPathName().toStdString());
    }

    auto const files =
        to_sorted_files(directory.findChildFiles(juce::File::findFiles, true, "*.xss"));
    auto out = std::vector<LibraryFileEntry>{};
    out.reserve(files.size());
    for (auto const &file : files)
    {
        out.push_back(make_library_file_entry(directory, file));
    }
    return out;
}

auto JuceLibraryFilePort::tuning_files(std::filesystem::path const &directory_path)
    const -> std::vector<LibraryTuningEntry>
{
    auto const directory = as_juce_file(directory_path);
    if (!directory.isDirectory())
    {
        throw std::runtime_error("Invalid library directory: " +
                                 directory.getFullPathName().toStdString());
    }

    auto const files =
        to_sorted_files(directory.findChildFiles(juce::File::findFiles, true, "*.scl"));
    auto out = std::vector<LibraryTuningEntry>{};
    out.reserve(files.size());
    for (auto const &file : files)
    {
        auto const tuning = sequence::from_scala(file.getFullPathName().toStdString());
        out.push_back({
            .file = make_library_file_entry(directory, file),
            .description = tuning.description,
            .intervals = tuning.intervals,
            .octave = tuning.octave,
        });
    }
    return out;
}

BridgeRequestDispatcher::BridgeRequestDispatcher(ApplicationBridgeService &application,
                                                 LibraryBridgeService &library,
                                                 KeymapBridgeService &keymap)
    : application_{application}, library_{library}, keymap_{keymap}
{
    handlers_ = {
        {"session.hello",
         [this](ParsedRequest const &request) {
             return handle_session_hello(request);
         }},
        {"state.get",
         [this](ParsedRequest const &request) { return handle_state_get(request); }},
        {"session.binding.get",
         [this](ParsedRequest const &request) {
             return handle_session_binding_get(request);
         }},
        {"session.binding.set",
         [this](ParsedRequest const &request) {
             return handle_session_binding_set(request);
         }},
        {"command.execute",
         [this](ParsedRequest const &request) {
             return handle_command_execute(request);
         }},
        {"library.get",
         [this](ParsedRequest const &request) { return handle_library_get(request); }},
        {"keymap.get",
         [this](ParsedRequest const &request) { return handle_keymap_get(request); }},
        {"keymap.override.set",
         [this](ParsedRequest const &request) {
             return handle_keymap_override_set(request);
         }},
        {"keymap.override.remove",
         [this](ParsedRequest const &request) {
             return handle_keymap_override_remove(request);
         }},
        {"keymap.reset",
         [this](ParsedRequest const &request) { return handle_keymap_reset(request); }},
    };
}

auto BridgeRequestDispatcher::handle_request_json(std::string const &request_json)
    -> std::string
{
    auto request = ParsedRequest{};

    try
    {
        request = parse_request(request_json);
        auto const handler = handlers_.find(request.name);
        if (handler == handlers_.end())
        {
            throw BridgeError{
                "invalid_request",
                "Unknown request name: " + request.name,
                request.name,
                request.request_id,
            };
        }

        auto payload = handler->second(request);
        return make_envelope("response", request.name, request.request_id,
                             std::move(payload))
            .dump();
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
    catch (std::invalid_argument const &error)
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

auto BridgeRequestDispatcher::handle_session_hello(ParsedRequest const &request)
    -> nlohmann::json
{
    validate_session_hello_payload(request.payload, request);
    return {
        {"protocol", protocol},
        {"plugin_version", VERSION},
        {"project_schema_version", project_schema_version},
        {"library_schema_version", library_schema_version},
        {"catalog", make_catalog_payload(application_.command_catalog_metadata())},
        {"binding", make_instance_binding(application_.instance_binding())},
        {"keymap", make_keymap_payload(keymap_.snapshot())},
    };
}

auto BridgeRequestDispatcher::handle_state_get(ParsedRequest const &request)
    -> nlohmann::json
{
    validate_empty_object_payload(request.payload, request);
    return make_project_snapshot(application_.project_snapshot());
}

auto BridgeRequestDispatcher::handle_session_binding_get(ParsedRequest const &request)
    -> nlohmann::json
{
    validate_empty_object_payload(request.payload, request);
    return make_instance_binding(application_.instance_binding());
}

auto BridgeRequestDispatcher::handle_session_binding_set(ParsedRequest const &request)
    -> nlohmann::json
{
    auto output_id = require_string(request.payload, "output_id");
    if (output_id.empty())
    {
        throw BridgeError{
            "invalid_request",
            "Field must not be empty: output_id",
            request.name,
            request.request_id,
        };
    }

    application_.set_output_id(std::move(output_id));
    return make_instance_binding(application_.instance_binding());
}

auto BridgeRequestDispatcher::handle_command_execute(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const command = require_string(request.payload, "command");
    auto const context = parse_command_context(request.payload);
    auto const result = application_.execute_command_string(command, context);
    return {
        {"status",
         {
             {"level", to_string(result.status.first)},
             {"message", result.status.second},
         }},
        {"suggested_selection", selection_to_json(result.suggested_selection)},
        {"snapshot", make_project_snapshot(application_.project_snapshot())},
    };
}

auto BridgeRequestDispatcher::handle_library_get(ParsedRequest const &request)
    -> nlohmann::json
{
    validate_empty_object_payload(request.payload, request);
    return library_.make_payload(application_.library_snapshot());
}

auto BridgeRequestDispatcher::handle_keymap_get(ParsedRequest const &request)
    -> nlohmann::json
{
    validate_empty_object_payload(request.payload, request);
    return make_keymap_payload(keymap_.snapshot());
}

auto BridgeRequestDispatcher::handle_keymap_override_set(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const expected_revision =
        require_unsigned(request.payload, "expected_revision");
    auto const context = require_string(request.payload, "context");
    auto const trigger =
        parse_keymap_trigger(require_object(request.payload, "trigger"));
    auto target = std::optional<KeymapTarget>{};
    if (!request.payload.contains("target"))
    {
        throw BridgeError{"invalid_request", "Missing field: target"};
    }
    if (!request.payload.at("target").is_null())
    {
        target = parse_keymap_target(request.payload.at("target"));
    }
    return make_keymap_payload(
        keymap_.set_override(expected_revision, context, trigger, std::move(target)));
}

auto BridgeRequestDispatcher::handle_keymap_override_remove(
    ParsedRequest const &request) -> nlohmann::json
{
    auto const expected_revision =
        require_unsigned(request.payload, "expected_revision");
    auto const context = require_string(request.payload, "context");
    auto const trigger =
        parse_keymap_trigger(require_object(request.payload, "trigger"));
    return make_keymap_payload(
        keymap_.remove_override(expected_revision, context, trigger));
}

auto BridgeRequestDispatcher::handle_keymap_reset(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const expected_revision =
        require_unsigned(request.payload, "expected_revision");
    return make_keymap_payload(keymap_.reset(expected_revision));
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
        auto codepoint = std::uint32_t{0};
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

        if ((need == 3 && codepoint < 0x800) || (need == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF) || codepoint > 0x10FFFF)
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

} // namespace xen::bridge
