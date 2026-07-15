#include <xen/webview_bridge_services.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#include <sequence/tuning.hpp>

#include <xen/bridge_serialize.hpp>
#include <xen/constants.hpp>
#include <xen/document_storage.hpp>
#include <xen/modulation_json.hpp>
#include <xen/text_file.hpp>
#include <xen/user_directory.hpp>

namespace xen::bridge
{

namespace
{

auto as_juce_file(std::filesystem::path const &path) -> juce::File
{
    return juce::File{path.string()};
}

void append_bridge_error_log(juce::String const &message)
{
    juce::Logger::writeToLog(message);

    try
    {
        auto const log_file =
            xen::get_user_settings_directory().getChildFile("bridge-errors.log");
        log_file.appendText(message + "\n\n", false, false, "\n");
    }
    catch (std::exception const &)
    {
    }
}

void log_json_bridge_exception(std::string const &request_json,
                               ParsedRequest const &request,
                               nlohmann::json::exception const &error)
{
    auto request_excerpt = juce::String{request_json};
    if (request_excerpt.length() > 4096)
    {
        request_excerpt = request_excerpt.substring(0, 4096) + "...<truncated>";
    }

    auto message = juce::String{"XenSequencer bridge JSON exception: "} + error.what() +
                   "\nrequest_name: " +
                   (request.name.empty() ? juce::String{"<unparsed>"}
                                         : juce::String{request.name}) +
                   "\nrequest_id: " +
                   (request.request_id.has_value() ? juce::String{*request.request_id}
                                                   : juce::String{"<none>"}) +
                   "\nraw_request: " + request_excerpt;
    append_bridge_error_log(message);
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
        {"file_revision", file.file_revision},
        {"command",
         normalize_utf8(command_prefix + quote_command_arg(file.relative_path))},
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
    auto const extension = file.getFileExtension().toStdString();
    auto maximum_bytes = std::size_t{};
    if (extension == ".xencell")
    {
        maximum_bytes = MAX_CELL_FILE_BYTES;
    }
    else if (extension == ".xenproj")
    {
        maximum_bytes = MAX_PROJECT_FILE_BYTES;
    }
    else if (extension == ".scl")
    {
        maximum_bytes = MAX_TUNING_FILE_BYTES;
    }
    else
    {
        throw DocumentError{DocumentErrorCode::InvalidPath,
                            "Library entry has an unsupported extension."};
    }
    auto const safe_path =
        resolve_content_path(directory.getFullPathName().toStdString(),
                             relative_path.toStdString(), extension);
    auto error = std::error_code{};
    if (std::filesystem::weakly_canonical(file.getFullPathName().toStdString(),
                                          error) != safe_path ||
        error)
    {
        throw DocumentError{DocumentErrorCode::InvalidPath,
                            "Library entry escapes its configured directory."};
    }

    auto const path = file.getFullPathName().toStdString();
    auto revision = std::optional<std::string>{};
    try
    {
        revision = xen::file_revision(path, maximum_bytes);
    }
    catch (std::length_error const &)
    {
        throw DocumentError{DocumentErrorCode::FileTooLarge,
                            "Library file exceeds the permitted size."};
    }
    if (!revision.has_value())
    {
        throw std::runtime_error{"Library file disappeared while being scanned."};
    }
    return {
        .name = file.getFileName().toStdString(),
        .relative_path = relative_path.toStdString(),
        .stem = stem_path,
        .file_revision = *revision,
    };
}

auto require_boolean(nlohmann::json const &json, std::string_view field) -> bool
{
    auto const key = std::string{field};
    if (!json.contains(key) || !json.at(key).is_boolean())
    {
        throw BridgeError{"invalid_request", "Field must be boolean: " + key};
    }
    return json.at(key).get<bool>();
}

auto optional_revision(nlohmann::json const &json, std::string_view field)
    -> std::optional<std::string>
{
    auto const key = std::string{field};
    if (!json.contains(key) || json.at(key).is_null())
    {
        return std::nullopt;
    }
    if (!json.at(key).is_string() ||
        json.at(key).get_ref<std::string const &>().empty())
    {
        throw BridgeError{"invalid_request",
                          "Field must be null or a non-empty string: " + key};
    }
    return json.at(key).get<std::string>();
}

auto document_error_code(DocumentErrorCode code) -> std::string
{
    switch (code)
    {
    case DocumentErrorCode::StaleProject:
        return "stale_project";
    case DocumentErrorCode::UnsavedChanges:
        return "unsaved_changes";
    case DocumentErrorCode::InvalidPath:
        return "invalid_path";
    case DocumentErrorCode::NotFound:
        return "not_found";
    case DocumentErrorCode::FileExists:
        return "file_exists";
    case DocumentErrorCode::FileConflict:
        return "file_conflict";
    case DocumentErrorCode::ProjectPathRequired:
        return "project_path_required";
    case DocumentErrorCode::FileTooLarge:
        return "file_too_large";
    case DocumentErrorCode::InvalidDocument:
        return "invalid_document";
    case DocumentErrorCode::PreviewActive:
        return "preview_active";
    case DocumentErrorCode::RecoveryConflict:
        return "recovery_conflict";
    case DocumentErrorCode::Io:
        return "io_error";
    }
    return "io_error";
}

auto document_result_to_json(DocumentOperationResult const &result) -> nlohmann::json
{
    auto file = nlohmann::json(nullptr);
    if (result.file.has_value())
    {
        file = {
            {"name", normalize_utf8(result.file->name)},
            {"relative_path", normalize_utf8(result.file->relative_path)},
            {"stem", normalize_utf8(result.file->stem)},
            {"file_revision", result.file->file_revision},
        };
    }
    return {
        {"snapshot", make_project_snapshot(result.snapshot)},
        {"file", std::move(file)},
        {"suggested_selection", selection_to_json(result.suggested_selection)},
    };
}

} // namespace

SequencerApplicationBridgeService::SequencerApplicationBridgeService(
    SequencerSessionPort &session)
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
    return session_.command_catalog_metadata();
}

auto SequencerApplicationBridgeService::execute_command_string(
    std::string const &command, CommandContext const &context)
    -> CommandApplicationResult
{
    return session_.execute_command_string(command, context);
}

auto SequencerApplicationBridgeService::begin_preview(ProjectRevision expected_revision)
    -> PreviewControlResult
{
    return session_.begin_preview(expected_revision);
}

auto SequencerApplicationBridgeService::begin_modulation_preview(
    ProjectRevision expected_revision, ModulationTarget target) -> PreviewControlResult
{
    return session_.begin_modulation_preview(expected_revision, std::move(target));
}

auto SequencerApplicationBridgeService::update_modulation_preview(
    ModulationPreviewUpdate const &update) -> ModulationPreviewUpdateResult
{
    return session_.update_modulation_preview(update);
}

auto SequencerApplicationBridgeService::commit_preview(
    PreviewId const &preview_id, ProjectRevision expected_revision)
    -> PreviewControlResult
{
    return session_.commit_preview(preview_id, expected_revision);
}

auto SequencerApplicationBridgeService::cancel_preview(
    PreviewId const &preview_id, ProjectRevision expected_revision)
    -> PreviewControlResult
{
    return session_.cancel_preview(preview_id, expected_revision);
}

auto SequencerApplicationBridgeService::create_project(
    ProjectRevision expected_revision, bool discard_unsaved) -> DocumentOperationResult
{
    return session_.create_project(expected_revision, discard_unsaved);
}

auto SequencerApplicationBridgeService::open_project(std::string relative_path,
                                                     ProjectRevision expected_revision,
                                                     bool discard_unsaved)
    -> DocumentOperationResult
{
    return session_.open_project(std::move(relative_path), expected_revision,
                                 discard_unsaved);
}

auto SequencerApplicationBridgeService::save_project(ProjectRevision expected_revision)
    -> DocumentOperationResult
{
    return session_.save_project(expected_revision);
}

auto SequencerApplicationBridgeService::save_project_as(
    std::string relative_path, ProjectRevision expected_revision,
    std::optional<std::string> expected_file_revision) -> DocumentOperationResult
{
    return session_.save_project_as(std::move(relative_path), expected_revision,
                                    std::move(expected_file_revision));
}

auto SequencerApplicationBridgeService::restore_recovery(
    std::string recovery_revision, ProjectRevision expected_revision,
    bool discard_unsaved) -> DocumentOperationResult
{
    return session_.restore_recovery(std::move(recovery_revision), expected_revision,
                                     discard_unsaved);
}

auto SequencerApplicationBridgeService::discard_recovery(std::string recovery_revision)
    -> DocumentOperationResult
{
    return session_.discard_recovery(std::move(recovery_revision));
}

auto SequencerApplicationBridgeService::import_cell(std::string relative_path,
                                                    ProjectRevision expected_revision,
                                                    CompositionCursor cursor)
    -> DocumentOperationResult
{
    return session_.import_cell(std::move(relative_path), expected_revision, cursor);
}

auto SequencerApplicationBridgeService::save_cell(
    std::string relative_path, ProjectRevision expected_revision,
    CompositionCursor cursor, SelectionPath selection,
    std::optional<std::string> expected_file_revision) -> DocumentOperationResult
{
    return session_.save_cell(std::move(relative_path), expected_revision, cursor,
                              std::move(selection), std::move(expected_file_revision));
}

void SequencerApplicationBridgeService::set_channel_id(ChannelId channel_id)
{
    session_.set_channel_id(std::move(channel_id));
}

StoreKeymapBridgeService::StoreKeymapBridgeService(std::filesystem::path keymap_file)
    : store_{std::move(keymap_file)}
{
}

auto StoreKeymapBridgeService::read() -> KeymapResource
{
    return store_.read();
}

auto StoreKeymapBridgeService::revision() const noexcept -> std::uint64_t
{
    return store_.revision();
}

auto StoreKeymapBridgeService::write(std::uint64_t expected_revision,
                                     nlohmann::json document) -> KeymapResource
{
    return store_.write(expected_revision, std::move(document));
}

auto StoreKeymapBridgeService::erase(std::uint64_t expected_revision) -> KeymapResource
{
    return store_.erase(expected_revision);
}

auto StoreKeymapBridgeService::refresh() -> bool
{
    return store_.refresh();
}

StorePreferencesBridgeService::StorePreferencesBridgeService(
    std::filesystem::path preferences_file)
    : store_{std::move(preferences_file)}
{
}

auto StorePreferencesBridgeService::read() -> PreferencesResource
{
    return store_.read();
}

auto StorePreferencesBridgeService::revision() const noexcept -> std::uint64_t
{
    return store_.revision();
}

auto StorePreferencesBridgeService::write(std::uint64_t expected_revision,
                                          nlohmann::json document)
    -> PreferencesResource
{
    return store_.write(expected_revision, std::move(document));
}

auto StorePreferencesBridgeService::erase(std::uint64_t expected_revision)
    -> PreferencesResource
{
    return store_.erase(expected_revision);
}

auto StorePreferencesBridgeService::refresh() -> bool
{
    return store_.refresh();
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
        {"library_revision", std::to_string(snapshot.library_revision.value())},
        {"paths",
         {
             {"library", normalize_utf8(files_.library_root().string())},
             {"content", normalize_utf8(workspace.content_directory.string())},
             {"tunings", normalize_utf8(workspace.tuning_directory.string())},
         }},
        {"cells", file_entries_to_json(files_.cell_files(workspace.content_directory),
                                       "load cell ")},
        {"projects",
         file_entries_to_json(files_.project_files(workspace.content_directory),
                              "project open ")},
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

auto JuceLibraryFilePort::cell_files(std::filesystem::path const &directory_path) const
    -> std::vector<LibraryFileEntry>
{
    auto const directory = as_juce_file(directory_path);
    if (!directory.isDirectory())
    {
        throw std::runtime_error("Invalid library directory: " +
                                 directory.getFullPathName().toStdString());
    }

    auto const files = to_sorted_files(
        directory.findChildFiles(juce::File::findFiles, true, "*.xencell"));
    auto out = std::vector<LibraryFileEntry>{};
    out.reserve(files.size());
    for (auto const &file : files)
    {
        out.push_back(make_library_file_entry(directory, file));
    }
    return out;
}

auto JuceLibraryFilePort::project_files(
    std::filesystem::path const &directory_path) const -> std::vector<LibraryFileEntry>
{
    auto const directory = as_juce_file(directory_path);
    if (!directory.isDirectory())
        throw std::runtime_error("Invalid library directory: " +
                                 directory.getFullPathName().toStdString());
    auto const files = to_sorted_files(
        directory.findChildFiles(juce::File::findFiles, true, "*.xenproj"));
    auto out = std::vector<LibraryFileEntry>{};
    out.reserve(files.size());
    for (auto const &file : files)
        out.push_back(make_library_file_entry(directory, file));
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
        auto entry = make_library_file_entry(directory, file);
        auto const tuning = sequence::from_scala(file.getFullPathName().toStdString());
        auto const refreshed_entry = make_library_file_entry(directory, file);
        if (refreshed_entry.file_revision != entry.file_revision)
        {
            throw std::runtime_error{
                "Tuning file changed while the library was being scanned."};
        }
        out.push_back({
            .file = std::move(entry),
            .description = tuning.description,
            .intervals = tuning.intervals,
            .octave = tuning.octave,
        });
    }
    return out;
}

BridgeRequestDispatcher::BridgeRequestDispatcher(ApplicationBridgeService &application,
                                                 LibraryBridgeService &library,
                                                 KeymapBridgeService &keymap,
                                                 PreferencesBridgeService &preferences)
    : application_{application}, library_{library}, keymap_{keymap},
      preferences_{preferences}
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
        {"preview.begin",
         [this](ParsedRequest const &request) {
             return handle_preview_begin(request);
         }},
        {"preview.commit",
         [this](ParsedRequest const &request) {
             return handle_preview_commit(request);
         }},
        {"preview.cancel",
         [this](ParsedRequest const &request) {
             return handle_preview_cancel(request);
         }},
        {"modulation.preview.begin",
         [this](ParsedRequest const &request) {
             return handle_modulation_preview_begin(request);
         }},
        {"modulation.preview.update",
         [this](ParsedRequest const &request) {
             return handle_modulation_preview_update(request);
         }},
        {"modulation.preview.commit",
         [this](ParsedRequest const &request) {
             return handle_modulation_preview_commit(request);
         }},
        {"modulation.preview.cancel",
         [this](ParsedRequest const &request) {
             return handle_modulation_preview_cancel(request);
         }},
        {"library.get",
         [this](ParsedRequest const &request) { return handle_library_get(request); }},
        {"project.new",
         [this](ParsedRequest const &request) {
             return handle_document_operation(request);
         }},
        {"project.open",
         [this](ParsedRequest const &request) {
             return handle_document_operation(request);
         }},
        {"project.save",
         [this](ParsedRequest const &request) {
             return handle_document_operation(request);
         }},
        {"project.save_as",
         [this](ParsedRequest const &request) {
             return handle_document_operation(request);
         }},
        {"project.recovery.restore",
         [this](ParsedRequest const &request) {
             return handle_document_operation(request);
         }},
        {"project.recovery.discard",
         [this](ParsedRequest const &request) {
             return handle_document_operation(request);
         }},
        {"cell.import",
         [this](ParsedRequest const &request) {
             return handle_document_operation(request);
         }},
        {"cell.save",
         [this](ParsedRequest const &request) {
             return handle_document_operation(request);
         }},
        {"keymap.read",
         [this](ParsedRequest const &request) { return handle_keymap_read(request); }},
        {"keymap.write",
         [this](ParsedRequest const &request) { return handle_keymap_write(request); }},
        {"keymap.delete",
         [this](ParsedRequest const &request) {
             return handle_keymap_delete(request);
         }},
        {"preferences.read",
         [this](ParsedRequest const &request) {
             return handle_preferences_read(request);
         }},
        {"preferences.write",
         [this](ParsedRequest const &request) {
             return handle_preferences_write(request);
         }},
        {"preferences.delete",
         [this](ParsedRequest const &request) {
             return handle_preferences_delete(request);
         }},
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
    catch (DocumentError const &error)
    {
        auto payload =
            make_error_payload(document_error_code(error.code), error.what());
        if (error.code == DocumentErrorCode::FileConflict ||
            error.code == DocumentErrorCode::FileExists)
        {
            payload["error"]["details"] = {
                {"current_file_revision",
                 error.current_file_revision.has_value()
                     ? nlohmann::json(*error.current_file_revision)
                     : nlohmann::json(nullptr)},
            };
        }
        return make_envelope("response",
                             request.name.empty() ? "bridge.error" : request.name,
                             request.request_id, std::move(payload))
            .dump();
    }
    catch (KeymapStorageError const &error)
    {
        auto code = std::string{};
        switch (error.code)
        {
        case KeymapStorageErrorCode::Conflict:
            code = "conflict";
            break;
        case KeymapStorageErrorCode::MalformedDocument:
            code = "malformed_document";
            break;
        case KeymapStorageErrorCode::Read:
            code = "keymap_read_error";
            break;
        case KeymapStorageErrorCode::Write:
            code = "keymap_write_error";
            break;
        case KeymapStorageErrorCode::Delete:
            code = "keymap_delete_error";
            break;
        }
        return make_envelope("response",
                             request.name.empty() ? "bridge.error" : request.name,
                             request.request_id, make_error_payload(code, error.what()))
            .dump();
    }
    catch (PreferencesStorageError const &error)
    {
        auto code = std::string{};
        switch (error.code)
        {
        case PreferencesStorageErrorCode::Conflict:
            code = "conflict";
            break;
        case PreferencesStorageErrorCode::MalformedDocument:
            code = "malformed_document";
            break;
        case PreferencesStorageErrorCode::Read:
            code = "preferences_read_error";
            break;
        case PreferencesStorageErrorCode::Write:
            code = "preferences_write_error";
            break;
        case PreferencesStorageErrorCode::Delete:
            code = "preferences_delete_error";
            break;
        }
        return make_envelope("response",
                             request.name.empty() ? "bridge.error" : request.name,
                             request.request_id, make_error_payload(code, error.what()))
            .dump();
    }
    catch (nlohmann::json::exception const &error)
    {
        log_json_bridge_exception(request_json, request, error);
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
        {"modulation", make_modulation_catalog_payload()},
        {"binding", make_instance_binding(application_.instance_binding())},
        {"keymap", make_keymap_payload(keymap_.read())},
        {"preferences", make_preferences_payload(preferences_.read())},
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
    auto channel_id = require_string(request.payload, "channel_id");
    if (channel_id.empty() || channel_id.size() > MAX_PERSISTED_STRING_BYTES)
    {
        throw BridgeError{
            "invalid_request",
            "Field must contain between 1 and 4096 bytes: channel_id",
            request.name,
            request.request_id,
        };
    }

    application_.set_channel_id(std::move(channel_id));
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

auto BridgeRequestDispatcher::handle_preview_begin(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const revision = ProjectRevision{
        require_resource_revision(request.payload, "expected_project_revision")};
    auto const result = application_.begin_preview(revision);
    return {
        {"status",
         {{"level", to_string(result.status.first)},
          {"message", result.status.second}}},
        {"preview_id", result.preview_id.has_value()
                           ? nlohmann::json(*result.preview_id)
                           : nlohmann::json(nullptr)},
        {"snapshot", make_project_snapshot(application_.project_snapshot())},
    };
}

auto BridgeRequestDispatcher::handle_preview_commit(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const preview_id = require_string(request.payload, "preview_id");
    if (preview_id.empty())
    {
        throw BridgeError{"invalid_request", "Field must not be empty: preview_id"};
    }
    auto const revision = ProjectRevision{
        require_resource_revision(request.payload, "expected_project_revision")};
    auto const result = application_.commit_preview(preview_id, revision);
    return {
        {"status",
         {{"level", to_string(result.status.first)},
          {"message", result.status.second}}},
        {"snapshot", make_project_snapshot(application_.project_snapshot())},
    };
}

auto BridgeRequestDispatcher::handle_preview_cancel(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const preview_id = require_string(request.payload, "preview_id");
    if (preview_id.empty())
    {
        throw BridgeError{"invalid_request", "Field must not be empty: preview_id"};
    }
    auto const revision = ProjectRevision{
        require_resource_revision(request.payload, "expected_project_revision")};
    auto const result = application_.cancel_preview(preview_id, revision);
    return {
        {"status",
         {{"level", to_string(result.status.first)},
          {"message", result.status.second}}},
        {"snapshot", make_project_snapshot(application_.project_snapshot())},
    };
}

auto BridgeRequestDispatcher::handle_modulation_preview_begin(
    ParsedRequest const &request) -> nlohmann::json
{
    auto const revision = ProjectRevision{
        require_resource_revision(request.payload, "expected_project_revision")};
    auto const result = application_.begin_modulation_preview(
        revision,
        modulation_target_from_json(require_object(request.payload, "target")));
    return {
        {"status",
         {{"level", to_string(result.status.first)},
          {"message", result.status.second}}},
        {"preview_id", result.preview_id.has_value()
                           ? nlohmann::json(*result.preview_id)
                           : nlohmann::json(nullptr)},
        {"snapshot", make_project_snapshot(application_.project_snapshot())},
    };
}

auto BridgeRequestDispatcher::handle_modulation_preview_update(
    ParsedRequest const &request) -> nlohmann::json
{
    auto preview_id = require_string(request.payload, "preview_id");
    if (preview_id.empty())
    {
        throw BridgeError{"invalid_request", "Field must not be empty: preview_id"};
    }
    auto update = ModulationPreviewUpdate{
        .preview_id = std::move(preview_id),
        .update_sequence =
            require_resource_revision(request.payload, "update_sequence"),
        .expected_project_revision = ProjectRevision{require_resource_revision(
            request.payload, "expected_project_revision")},
        .destination =
            modulation_destination_from_json(request.payload.at("destination")),
        .output_range = modulation_output_range_from_json(
            require_object(request.payload, "output_range")),
        .modulation = modulation_definition_from_json(
            require_object(request.payload, "modulation")),
    };
    validate(update.destination, update.output_range);
    auto const result = application_.update_modulation_preview(update);
    return {
        {"status",
         {{"level", to_string(result.status.first)},
          {"message", result.status.second}}},
        {"preview_id", result.preview_id},
        {"accepted_update_sequence", std::to_string(result.accepted_update_sequence)},
        {"accepted", result.accepted},
        {"project_changed", result.project_changed},
        {"project_revision", std::to_string(result.project_revision.value())},
        {"state_revision", std::to_string(result.state_revision.value())},
    };
}

auto BridgeRequestDispatcher::handle_modulation_preview_commit(
    ParsedRequest const &request) -> nlohmann::json
{
    return handle_preview_commit(request);
}

auto BridgeRequestDispatcher::handle_modulation_preview_cancel(
    ParsedRequest const &request) -> nlohmann::json
{
    return handle_preview_cancel(request);
}

auto BridgeRequestDispatcher::handle_library_get(ParsedRequest const &request)
    -> nlohmann::json
{
    validate_empty_object_payload(request.payload, request);
    return library_.make_payload(application_.library_snapshot());
}

auto BridgeRequestDispatcher::handle_document_operation(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const project_revision = [&] {
        return ProjectRevision{
            require_resource_revision(request.payload, "expected_project_revision")};
    };
    if (request.name == "project.new")
    {
        return document_result_to_json(application_.create_project(
            project_revision(), require_boolean(request.payload, "discard_unsaved")));
    }
    if (request.name == "project.open")
    {
        return document_result_to_json(application_.open_project(
            require_string(request.payload, "relative_path"), project_revision(),
            require_boolean(request.payload, "discard_unsaved")));
    }
    if (request.name == "project.save")
    {
        return document_result_to_json(application_.save_project(project_revision()));
    }
    if (request.name == "project.save_as")
    {
        if (!request.payload.contains("expected_file_revision"))
        {
            throw BridgeError{"invalid_request",
                              "Missing field: expected_file_revision"};
        }
        return document_result_to_json(application_.save_project_as(
            require_string(request.payload, "relative_path"), project_revision(),
            optional_revision(request.payload, "expected_file_revision")));
    }
    if (request.name == "project.recovery.restore")
    {
        return document_result_to_json(application_.restore_recovery(
            require_string(request.payload, "recovery_revision"), project_revision(),
            require_boolean(request.payload, "discard_unsaved")));
    }
    if (request.name == "project.recovery.discard")
    {
        return document_result_to_json(application_.discard_recovery(
            require_string(request.payload, "recovery_revision")));
    }

    auto context_payload = nlohmann::json{
        {"context",
         {{"expected_project_revision",
           request.payload.at("expected_project_revision")},
          {"cursor", request.payload.at("cursor")}}},
    };
    if (request.payload.contains("selection"))
    {
        context_payload["context"]["selection"] = request.payload.at("selection");
    }
    auto const context = parse_command_context(context_payload);
    if (request.name == "cell.import")
    {
        return document_result_to_json(
            application_.import_cell(require_string(request.payload, "relative_path"),
                                     project_revision(), context.cursor));
    }
    if (request.name == "cell.save")
    {
        if (!context.selection.has_value())
        {
            throw BridgeError{"invalid_request", "Field is required: selection"};
        }
        if (!request.payload.contains("expected_file_revision"))
        {
            throw BridgeError{"invalid_request",
                              "Missing field: expected_file_revision"};
        }
        return document_result_to_json(application_.save_cell(
            require_string(request.payload, "relative_path"), project_revision(),
            context.cursor, *context.selection,
            optional_revision(request.payload, "expected_file_revision")));
    }
    throw BridgeError{"invalid_request", "Unknown document operation: " + request.name};
}

auto BridgeRequestDispatcher::handle_keymap_read(ParsedRequest const &request)
    -> nlohmann::json
{
    validate_empty_object_payload(request.payload, request);
    return make_keymap_payload(keymap_.read());
}

auto BridgeRequestDispatcher::handle_keymap_write(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const expected_revision =
        require_resource_revision(request.payload, "expected_revision");
    if (!request.payload.contains("document"))
    {
        throw BridgeError{"invalid_request", "Missing field: document"};
    }
    return make_keymap_payload(
        keymap_.write(expected_revision, request.payload.at("document")));
}

auto BridgeRequestDispatcher::handle_keymap_delete(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const expected_revision =
        require_resource_revision(request.payload, "expected_revision");
    return make_keymap_payload(keymap_.erase(expected_revision));
}

auto BridgeRequestDispatcher::handle_preferences_read(ParsedRequest const &request)
    -> nlohmann::json
{
    validate_empty_object_payload(request.payload, request);
    return make_preferences_payload(preferences_.read());
}

auto BridgeRequestDispatcher::handle_preferences_write(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const expected_revision =
        require_resource_revision(request.payload, "expected_revision");
    auto const &document = require_object(request.payload, "document");
    return make_preferences_payload(preferences_.write(expected_revision, document));
}

auto BridgeRequestDispatcher::handle_preferences_delete(ParsedRequest const &request)
    -> nlohmann::json
{
    auto const expected_revision =
        require_resource_revision(request.payload, "expected_revision");
    return make_preferences_payload(preferences_.erase(expected_revision));
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
