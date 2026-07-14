#include <xen/ipc_protocol.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <xen/project_validation.hpp>
#include <xen/serialize.hpp>

namespace xen::ipc
{
namespace
{

[[nodiscard]] auto composition_coordinate_from_json(nlohmann::json const &json,
                                                    char const *field)
    -> CompositionCoordinate
{
    if (!json.contains(field) ||
        (!json.at(field).is_number_integer() && !json.at(field).is_number_unsigned()))
        throw std::invalid_argument{"Composition coordinate must be an integer."};
    auto const value = json.at(field).get<std::int64_t>();
    if (value < std::numeric_limits<CompositionCoordinate>::min() ||
        value > std::numeric_limits<CompositionCoordinate>::max())
        throw std::invalid_argument{"Composition coordinate is out of range."};
    return static_cast<CompositionCoordinate>(value);
}

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

[[nodiscard]] auto optional_string_from_json(nlohmann::json const &json)
    -> std::optional<std::string>
{
    return json.is_null() ? std::nullopt
                          : std::optional<std::string>{json.get<std::string>()};
}

[[nodiscard]] auto optional_string_to_json(std::optional<std::string> const &value)
    -> nlohmann::json
{
    return value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

[[nodiscard]] auto binding_to_json(InstanceBinding const &binding,
                                   bool require_channel = true) -> nlohmann::json
{
    if (binding.session_id.empty())
    {
        throw std::invalid_argument{"Session ID must not be empty."};
    }
    if (binding.instance_id.empty())
    {
        throw std::invalid_argument{"Instance ID must not be empty."};
    }
    if (require_channel && binding.channel_id.empty())
    {
        throw std::invalid_argument{"Channel ID must not be empty."};
    }
    if (binding.session_id.size() > MAX_PERSISTED_STRING_BYTES ||
        binding.instance_id.size() > MAX_PERSISTED_STRING_BYTES ||
        binding.channel_id.size() > MAX_PERSISTED_STRING_BYTES)
    {
        throw std::invalid_argument{"Instance binding field is too long."};
    }
    return {
        {"session_id", binding.session_id},
        {"instance_id", binding.instance_id},
        {"channel_id", binding.channel_id},
    };
}

[[nodiscard]] auto binding_from_json(nlohmann::json const &json,
                                     bool require_channel = true) -> InstanceBinding
{
    auto binding = InstanceBinding{
        .session_id = json.at("session_id").get<SessionId>(),
        .instance_id = json.at("instance_id").get<InstanceId>(),
        .channel_id = json.at("channel_id").get<ChannelId>(),
    };
    (void)binding_to_json(binding, require_channel);
    return binding;
}

[[nodiscard]] auto snapshot_to_json(ProjectSnapshot const &snapshot) -> nlohmann::json
{
    return {
        {"history_entry_id", snapshot.history_entry_id.value()},
        {"project_revision", snapshot.project_revision.value()},
        {"state_revision", snapshot.state_revision.value()},
        {"preview_active", snapshot.preview_active},
        {"document",
         {{"relative_path", optional_string_to_json(snapshot.document.relative_path)},
          {"file_revision", optional_string_to_json(snapshot.document.file_revision)},
          {"saved_project_digest",
           optional_string_to_json(snapshot.document.saved_project_digest)},
          {"dirty", snapshot.document.dirty}}},
        {"recovery",
         snapshot.recovery.has_value()
             ? nlohmann::json{{"revision", snapshot.recovery->revision},
                              {"saved_at_unix_ms", snapshot.recovery->saved_at_unix_ms},
                              {"relative_path", optional_string_to_json(
                                                    snapshot.recovery->relative_path)},
                              {"project_revision",
                               snapshot.recovery->project_revision.value()}}
             : nlohmann::json(nullptr)},
        {"project", nlohmann::json::parse(serialize_project(snapshot.project))},
    };
}

[[nodiscard]] auto snapshot_from_json(nlohmann::json const &json) -> ProjectSnapshot
{
    auto const &document = json.at("document");
    auto snapshot = ProjectSnapshot{
        .project = deserialize_project(json.at("project").dump()),
        .history_entry_id =
            HistoryEntryId{json.at("history_entry_id").get<std::uint64_t>()},
        .project_revision =
            ProjectRevision{json.at("project_revision").get<std::uint64_t>()},
        .state_revision = StateRevision{json.at("state_revision").get<std::uint64_t>()},
        .preview_active = json.at("preview_active").get<bool>(),
        .document =
            ProjectDocumentState{
                .relative_path =
                    optional_string_from_json(document.at("relative_path")),
                .file_revision =
                    optional_string_from_json(document.at("file_revision")),
                .saved_project_digest =
                    optional_string_from_json(document.at("saved_project_digest")),
                .dirty = document.at("dirty").get<bool>(),
            },
    };
    if (!json.at("recovery").is_null())
    {
        auto const &recovery = json.at("recovery");
        snapshot.recovery = RecoveryMetadata{
            .revision = recovery.at("revision").get<std::string>(),
            .saved_at_unix_ms = recovery.at("saved_at_unix_ms").get<std::uint64_t>(),
            .relative_path = optional_string_from_json(recovery.at("relative_path")),
            .project_revision =
                ProjectRevision{recovery.at("project_revision").get<std::uint64_t>()},
        };
    }
    return snapshot;
}

[[nodiscard]] auto scale_to_json(Scale const &scale) -> nlohmann::json
{
    return {
        {"name", scale.name},
        {"tuning_length", scale.tuning_length},
        {"intervals", scale.intervals},
        {"mode", scale.mode},
    };
}

[[nodiscard]] auto scale_from_json(nlohmann::json const &json) -> Scale
{
    auto scale = Scale{
        .name = json.at("name").get<std::string>(),
        .tuning_length = json.at("tuning_length").get<std::size_t>(),
        .intervals = json.at("intervals").get<std::vector<std::uint8_t>>(),
        .mode = json.at("mode").get<std::uint8_t>(),
    };
    validate_scale(scale);
    return scale;
}

[[nodiscard]] auto library_scale_to_json(LibraryScale const &scale) -> nlohmann::json
{
    return {
        {"id", scale.id},
        {"definition", scale_to_json(scale.definition)},
    };
}

[[nodiscard]] auto library_scale_from_json(nlohmann::json const &json) -> LibraryScale
{
    return {
        .id = json.at("id").get<std::string>(),
        .definition = scale_from_json(json.at("definition")),
    };
}

[[nodiscard]] auto chord_to_json(Chord const &chord) -> nlohmann::json
{
    return {
        {"name", chord.name},
        {"intervals", chord.intervals},
    };
}

[[nodiscard]] auto chord_from_json(nlohmann::json const &json) -> Chord
{
    return {
        .name = json.at("name").get<std::string>(),
        .intervals = json.at("intervals").get<std::vector<int>>(),
    };
}

[[nodiscard]] auto content_library_to_json(ContentLibrary const &library)
    -> nlohmann::json
{
    auto scales = nlohmann::json::array();
    for (auto const &scale : library.scales)
    {
        scales.push_back(library_scale_to_json(scale));
    }

    auto chords = nlohmann::json::array();
    for (auto const &chord : library.chords)
    {
        chords.push_back(chord_to_json(chord));
    }

    return {
        {"scales", std::move(scales)},
        {"chords", std::move(chords)},
    };
}

[[nodiscard]] auto content_library_from_json(nlohmann::json const &json)
    -> ContentLibrary
{
    auto library = ContentLibrary{};
    for (auto const &scale : json.at("scales"))
    {
        library.scales.push_back(library_scale_from_json(scale));
    }
    for (auto const &chord : json.at("chords"))
    {
        library.chords.push_back(chord_from_json(chord));
    }
    validate(library);
    return library;
}

[[nodiscard]] auto library_to_json(LibrarySnapshot const &snapshot) -> nlohmann::json
{
    return {
        {"library", content_library_to_json(snapshot.library)},
        {"content_directory", snapshot.workspace.content_directory.string()},
        {"tuning_directory", snapshot.workspace.tuning_directory.string()},
        {"library_revision", snapshot.library_revision.value()},
    };
}

[[nodiscard]] auto library_from_json(nlohmann::json const &json) -> LibrarySnapshot
{
    return {
        .library = content_library_from_json(json.at("library")),
        .workspace =
            WorkspaceSettings{
                .content_directory = json.at("content_directory").get<std::string>(),
                .tuning_directory = json.at("tuning_directory").get<std::string>(),
            },
        .library_revision =
            LibraryRevision{json.at("library_revision").get<std::uint64_t>()},
    };
}

[[nodiscard]] auto bindings_to_json(std::vector<InstanceBinding> const &bindings)
    -> nlohmann::json
{
    auto result = nlohmann::json::array();
    for (auto const &binding : bindings)
    {
        result.push_back(binding_to_json(binding));
    }
    return result;
}

[[nodiscard]] auto bindings_from_json(nlohmann::json const &json)
    -> std::vector<InstanceBinding>
{
    auto result = std::vector<InstanceBinding>{};
    for (auto const &item : json)
    {
        result.push_back(binding_from_json(item));
    }
    return result;
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
             : nlohmann::json(nullptr)},
        {"selection", selection_to_json(context.selection)},
        {"preview_id", context.preview_id.has_value()
                           ? nlohmann::json(*context.preview_id)
                           : nlohmann::json(nullptr)},
        {"cursor",
         {{"row_coordinate", context.cursor.row_coordinate},
          {"column_coordinate", context.cursor.column_coordinate},
          {"sequence_id", context.cursor.sequence_id.has_value()
                              ? nlohmann::json(*context.cursor.sequence_id)
                              : nlohmann::json(nullptr)}}},
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
    if (!json.at("preview_id").is_null())
    {
        context.preview_id = json.at("preview_id").get<PreviewId>();
    }
    auto const &cursor = json.at("cursor");
    if (!cursor.is_object())
        throw std::invalid_argument{"Field must be an object: context.cursor."};
    context.cursor = CompositionCursor{
        .row_coordinate = composition_coordinate_from_json(cursor, "row_coordinate"),
        .column_coordinate =
            composition_coordinate_from_json(cursor, "column_coordinate"),
        .sequence_id =
            cursor.at("sequence_id").is_null()
                ? std::optional<SequenceId>{}
                : std::optional<SequenceId>{cursor.at("sequence_id").get<SequenceId>()},
    };
    return context;
}

[[nodiscard]] auto status_to_json(CommandStatus const &status) -> nlohmann::json
{
    auto level = std::string{};
    switch (status.first)
    {
    case MessageLevel::Debug:
        level = "debug";
        break;
    case MessageLevel::Info:
        level = "info";
        break;
    case MessageLevel::Warning:
        level = "warning";
        break;
    case MessageLevel::Error:
        level = "error";
        break;
    }
    return {
        {"level", std::move(level)},
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
        {"binding", binding_to_json(message.binding, false)},
        {"restore_state", nullptr},
    };
    if (message.restore_state.has_value())
    {
        validate_persisted_processor_state(*message.restore_state);
        payload["restore_state"] = {
            {"binding", binding_to_json(message.restore_state->binding)},
            {"saved_project_revision",
             message.restore_state->saved_project_revision.value()},
            {"saved_state_revision",
             message.restore_state->saved_state_revision.value()},
            {"document",
             {{"relative_path",
               optional_string_to_json(message.restore_state->document.relative_path)},
              {"file_revision",
               optional_string_to_json(message.restore_state->document.file_revision)},
              {"saved_project_digest",
               optional_string_to_json(
                   message.restore_state->document.saved_project_digest)},
              {"dirty", message.restore_state->document.dirty}}},
            {"project",
             nlohmann::json::parse(serialize_project(message.restore_state->project))},
        };
    }
    return envelope("client.hello", std::move(payload));
}

auto decode_client_hello(nlohmann::json const &message) -> ClientHello
{
    auto const &payload = require_protocol(message, "client.hello");
    auto hello =
        ClientHello{.binding = binding_from_json(payload.at("binding"), false)};
    if (!payload.at("restore_state").is_null())
    {
        auto const &restore = payload.at("restore_state");
        hello.restore_state = PersistedProcessorState{
            .binding = binding_from_json(restore.at("binding")),
            .project = deserialize_project(restore.at("project").dump()),
            .saved_project_revision =
                ProjectRevision{
                    restore.at("saved_project_revision").get<std::uint64_t>()},
            .saved_state_revision =
                StateRevision{restore.at("saved_state_revision").get<std::uint64_t>()},
            .document =
                ProjectDocumentState{
                    .relative_path = optional_string_from_json(
                        restore.at("document").at("relative_path")),
                    .file_revision = optional_string_from_json(
                        restore.at("document").at("file_revision")),
                    .saved_project_digest = optional_string_from_json(
                        restore.at("document").at("saved_project_digest")),
                    .dirty = restore.at("document").at("dirty").get<bool>(),
                },
        };
        validate_persisted_processor_state(*hello.restore_state);
    }
    return hello;
}

auto encode_coordinator_hello(CoordinatorHello const &message) -> nlohmann::json
{
    auto const persistent_snapshot =
        !message.snapshot.preview_active &&
                message.persistent_snapshot.state_revision ==
                    message.snapshot.state_revision
            ? nlohmann::json(nullptr)
            : snapshot_to_json(message.persistent_snapshot);
    return envelope("coordinator.hello",
                    {
                        {"binding", binding_to_json(message.binding)},
                        {"snapshot", snapshot_to_json(message.snapshot)},
                        {"persistent_snapshot", std::move(persistent_snapshot)},
                        {"library", library_to_json(message.library)},
                        {"instances", bindings_to_json(message.instances)},
                    });
}

auto decode_coordinator_hello(nlohmann::json const &message) -> CoordinatorHello
{
    auto const &payload = require_protocol(message, "coordinator.hello");
    auto snapshot = snapshot_from_json(payload.at("snapshot"));
    auto persistent_snapshot =
        payload.at("persistent_snapshot").is_null()
            ? snapshot
            : snapshot_from_json(payload.at("persistent_snapshot"));
    return {
        .binding = binding_from_json(payload.at("binding")),
        .snapshot = std::move(snapshot),
        .persistent_snapshot = std::move(persistent_snapshot),
        .library = library_from_json(payload.at("library")),
        .instances = bindings_from_json(payload.at("instances")),
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

auto encode_preview_begin_request(PreviewBeginRequest const &message) -> nlohmann::json
{
    return envelope(
        "preview.begin",
        {{"request_id", message.request_id},
         {"source_instance_id", message.source_instance_id},
         {"expected_project_revision", message.expected_project_revision.value()}});
}

auto decode_preview_begin_request(nlohmann::json const &message) -> PreviewBeginRequest
{
    auto const &payload = require_protocol(message, "preview.begin");
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .source_instance_id = payload.at("source_instance_id").get<InstanceId>(),
        .expected_project_revision =
            ProjectRevision{
                payload.at("expected_project_revision").get<std::uint64_t>()},
    };
}

namespace
{
auto encode_preview_end_request(char const *type, PreviewEndRequest const &message)
    -> nlohmann::json
{
    return envelope(type, {{"request_id", message.request_id},
                           {"source_instance_id", message.source_instance_id},
                           {"preview_id", message.preview_id},
                           {"expected_project_revision",
                            message.expected_project_revision.value()}});
}

auto decode_preview_end_request(char const *type, nlohmann::json const &message)
    -> PreviewEndRequest
{
    auto const &payload = require_protocol(message, type);
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .source_instance_id = payload.at("source_instance_id").get<InstanceId>(),
        .preview_id = payload.at("preview_id").get<PreviewId>(),
        .expected_project_revision =
            ProjectRevision{
                payload.at("expected_project_revision").get<std::uint64_t>()},
    };
}
} // namespace

auto encode_preview_commit_request(PreviewEndRequest const &message) -> nlohmann::json
{
    return encode_preview_end_request("preview.commit", message);
}

auto decode_preview_commit_request(nlohmann::json const &message) -> PreviewEndRequest
{
    return decode_preview_end_request("preview.commit", message);
}

auto encode_preview_cancel_request(PreviewEndRequest const &message) -> nlohmann::json
{
    return encode_preview_end_request("preview.cancel", message);
}

auto decode_preview_cancel_request(nlohmann::json const &message) -> PreviewEndRequest
{
    return decode_preview_end_request("preview.cancel", message);
}

auto encode_preview_response(PreviewResponse const &message) -> nlohmann::json
{
    return envelope("preview.result",
                    {{"request_id", message.request_id},
                     {"status", status_to_json(message.result.status)},
                     {"preview_id", message.result.preview_id.has_value()
                                        ? nlohmann::json(*message.result.preview_id)
                                        : nlohmann::json(nullptr)},
                     {"snapshot", snapshot_to_json(message.snapshot)}});
}

auto decode_preview_response(nlohmann::json const &message) -> PreviewResponse
{
    auto const &payload = require_protocol(message, "preview.result");
    auto result = PreviewControlResult{
        .status = status_from_json(payload.at("status")),
    };
    if (!payload.at("preview_id").is_null())
    {
        result.preview_id = payload.at("preview_id").get<PreviewId>();
    }
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .result = std::move(result),
        .snapshot = snapshot_from_json(payload.at("snapshot")),
    };
}

auto encode_document_request(DocumentRequest const &message) -> nlohmann::json
{
    return envelope(
        "document.execute",
        {{"request_id", message.request_id},
         {"source_instance_id", message.source_instance_id},
         {"operation", message.operation},
         {"relative_path", message.relative_path},
         {"expected_project_revision", message.expected_project_revision.value()},
         {"discard_unsaved", message.discard_unsaved},
         {"expected_file_revision",
          optional_string_to_json(message.expected_file_revision)},
         {"recovery_revision", message.recovery_revision},
         {"cursor",
          {{"row_coordinate", message.cursor.row_coordinate},
           {"column_coordinate", message.cursor.column_coordinate},
           {"sequence_id", message.cursor.sequence_id.has_value()
                               ? nlohmann::json(*message.cursor.sequence_id)
                               : nlohmann::json(nullptr)}}},
         {"selection", selection_to_json(message.selection)}});
}

auto decode_document_request(nlohmann::json const &message) -> DocumentRequest
{
    auto const &payload = require_protocol(message, "document.execute");
    auto const &cursor = payload.at("cursor");
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .source_instance_id = payload.at("source_instance_id").get<InstanceId>(),
        .operation = payload.at("operation").get<std::string>(),
        .relative_path = payload.at("relative_path").get<std::string>(),
        .expected_project_revision =
            ProjectRevision{
                payload.at("expected_project_revision").get<std::uint64_t>()},
        .discard_unsaved = payload.at("discard_unsaved").get<bool>(),
        .expected_file_revision =
            optional_string_from_json(payload.at("expected_file_revision")),
        .recovery_revision = payload.at("recovery_revision").get<std::string>(),
        .cursor =
            CompositionCursor{
                .row_coordinate =
                    composition_coordinate_from_json(cursor, "row_coordinate"),
                .column_coordinate =
                    composition_coordinate_from_json(cursor, "column_coordinate"),
                .sequence_id = cursor.at("sequence_id").is_null()
                                   ? std::optional<SequenceId>{}
                                   : std::optional<SequenceId>{cursor.at("sequence_id")
                                                                   .get<SequenceId>()},
            },
        .selection = selection_from_json(payload.at("selection")),
    };
}

auto encode_document_response(DocumentResponse const &message) -> nlohmann::json
{
    auto file = nlohmann::json(nullptr);
    if (message.result.file.has_value())
    {
        file = {
            {"name", message.result.file->name},
            {"relative_path", message.result.file->relative_path},
            {"stem", message.result.file->stem},
            {"file_revision", message.result.file->file_revision},
        };
    }
    return envelope("document.result",
                    {{"request_id", message.request_id},
                     {"snapshot", snapshot_to_json(message.result.snapshot)},
                     {"file", std::move(file)},
                     {"suggested_selection",
                      selection_to_json(message.result.suggested_selection)}});
}

auto decode_document_response(nlohmann::json const &message) -> DocumentResponse
{
    auto const &payload = require_protocol(message, "document.result");
    auto result = DocumentOperationResult{
        .snapshot = snapshot_from_json(payload.at("snapshot")),
        .suggested_selection = selection_from_json(payload.at("suggested_selection")),
    };
    if (!payload.at("file").is_null())
    {
        auto const &file = payload.at("file");
        result.file = ContentFileInfo{
            .name = file.at("name").get<std::string>(),
            .relative_path = file.at("relative_path").get<std::string>(),
            .stem = file.at("stem").get<std::string>(),
            .file_revision = file.at("file_revision").get<std::string>(),
        };
    }
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .result = std::move(result),
    };
}

auto encode_project_changed(ProjectChanged const &message) -> nlohmann::json
{
    return envelope("project.changed",
                    {{"snapshot", snapshot_to_json(message.snapshot)}});
}

auto decode_project_changed(nlohmann::json const &message) -> ProjectChanged
{
    auto const &payload = require_protocol(message, "project.changed");
    return {.snapshot = snapshot_from_json(payload.at("snapshot"))};
}

auto encode_library_changed(LibraryChanged const &message) -> nlohmann::json
{
    return envelope("library.changed",
                    {{"snapshot", library_to_json(message.snapshot)}});
}

auto decode_library_changed(nlohmann::json const &message) -> LibraryChanged
{
    auto const &payload = require_protocol(message, "library.changed");
    return {.snapshot = library_from_json(payload.at("snapshot"))};
}

auto encode_binding_set_request(BindingSetRequest const &message) -> nlohmann::json
{
    if (message.request_id.empty())
    {
        throw std::invalid_argument{"Binding request ID must not be empty."};
    }
    if (message.instance_id.empty())
    {
        throw std::invalid_argument{"Binding instance ID must not be empty."};
    }
    if (message.channel_id.empty())
    {
        throw std::invalid_argument{"Binding channel ID must not be empty."};
    }
    return envelope("instance.binding.set", {
                                                {"request_id", message.request_id},
                                                {"instance_id", message.instance_id},
                                                {"channel_id", message.channel_id},
                                            });
}

auto decode_binding_set_request(nlohmann::json const &message) -> BindingSetRequest
{
    auto const &payload = require_protocol(message, "instance.binding.set");
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .instance_id = payload.at("instance_id").get<InstanceId>(),
        .channel_id = payload.at("channel_id").get<ChannelId>(),
    };
}

auto encode_binding_set_response(BindingSetResponse const &message) -> nlohmann::json
{
    return envelope("instance.binding.result",
                    {
                        {"request_id", message.request_id},
                        {"binding", binding_to_json(message.binding)},
                        {"snapshot", snapshot_to_json(message.snapshot)},
                    });
}

auto decode_binding_set_response(nlohmann::json const &message) -> BindingSetResponse
{
    auto const &payload = require_protocol(message, "instance.binding.result");
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .binding = binding_from_json(payload.at("binding")),
        .snapshot = snapshot_from_json(payload.at("snapshot")),
    };
}

auto encode_instances_changed(InstancesChanged const &message) -> nlohmann::json
{
    return envelope("instances.changed",
                    {{"instances", bindings_to_json(message.instances)}});
}

auto decode_instances_changed(nlohmann::json const &message) -> InstancesChanged
{
    auto const &payload = require_protocol(message, "instances.changed");
    return {.instances = bindings_from_json(payload.at("instances"))};
}

auto encode_heartbeat(Heartbeat const &message) -> nlohmann::json
{
    return envelope("heartbeat", {{"sequence", message.sequence}});
}

auto decode_heartbeat(nlohmann::json const &message) -> Heartbeat
{
    auto const &payload = require_protocol(message, "heartbeat");
    return {.sequence = payload.at("sequence").get<std::uint64_t>()};
}

auto encode_shutdown_if_idle_request(ShutdownIfIdleRequest const &message)
    -> nlohmann::json
{
    if (message.request_id.empty())
    {
        throw std::invalid_argument{"Shutdown request ID must not be empty."};
    }
    return envelope("coordinator.shutdown_if_idle",
                    {{"request_id", message.request_id}});
}

auto decode_shutdown_if_idle_request(nlohmann::json const &message)
    -> ShutdownIfIdleRequest
{
    auto const &payload = require_protocol(message, "coordinator.shutdown_if_idle");
    return {.request_id = payload.at("request_id").get<std::string>()};
}

auto encode_shutdown_if_idle_response(ShutdownIfIdleResponse const &message)
    -> nlohmann::json
{
    return envelope("coordinator.shutdown_if_idle.result",
                    {
                        {"request_id", message.request_id},
                        {"will_exit", message.will_exit},
                    });
}

auto decode_shutdown_if_idle_response(nlohmann::json const &message)
    -> ShutdownIfIdleResponse
{
    auto const &payload =
        require_protocol(message, "coordinator.shutdown_if_idle.result");
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .will_exit = payload.at("will_exit").get<bool>(),
    };
}

auto encode_error(IpcError const &message) -> nlohmann::json
{
    return envelope("error",
                    {
                        {"request_id", message.request_id},
                        {"code", message.code},
                        {"message", message.message},
                        {"current_file_revision",
                         optional_string_to_json(message.current_file_revision)},
                    });
}

auto decode_error(nlohmann::json const &message) -> IpcError
{
    auto const &payload = require_protocol(message, "error");
    return {
        .request_id = payload.at("request_id").get<std::string>(),
        .code = payload.at("code").get<std::string>(),
        .message = payload.at("message").get<std::string>(),
        .current_file_revision =
            optional_string_from_json(payload.at("current_file_revision")),
    };
}

} // namespace xen::ipc
