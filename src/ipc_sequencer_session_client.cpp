#include <xen/ipc_sequencer_session_client.hpp>

#include <chrono>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <xen/message_level.hpp>

namespace xen::ipc
{
namespace
{

constexpr auto MAX_IPC_MESSAGE_BYTES = std::size_t{160 * 1'024 * 1'024};
constexpr auto MAX_IPC_MESSAGE_DEPTH = std::size_t{384};
constexpr auto MAX_IPC_MESSAGE_EVENTS = std::size_t{10'000'000};

[[nodiscard]] auto memory_block_from_json(nlohmann::json const &message)
    -> juce::MemoryBlock
{
    auto const text = message.dump();
    return juce::MemoryBlock{text.data(), text.size()};
}

[[nodiscard]] auto json_from_memory_block(juce::MemoryBlock const &message)
    -> nlohmann::json
{
    if (message.getSize() > MAX_IPC_MESSAGE_BYTES)
    {
        throw std::length_error{"Coordinator IPC message exceeds 160 MiB."};
    }
    auto events = std::size_t{};
    return nlohmann::json::parse(
        std::string{static_cast<char const *>(message.getData()), message.getSize()},
        [&events](int depth, nlohmann::json::parse_event_t, nlohmann::json &) {
            ++events;
            if (depth < 0 || static_cast<std::size_t>(depth) > MAX_IPC_MESSAGE_DEPTH ||
                events > MAX_IPC_MESSAGE_EVENTS)
            {
                throw std::invalid_argument{"Coordinator IPC message is too complex."};
            }
            return true;
        });
}

[[nodiscard]] auto error_result(std::string message) -> CommandApplicationResult
{
    return {
        .status = {MessageLevel::Error, std::move(message)},
        .suggested_selection = std::nullopt,
    };
}

[[nodiscard]] auto preview_error(std::string message) -> PreviewControlResult
{
    return {.status = {MessageLevel::Error, std::move(message)}};
}

[[nodiscard]] auto modulation_preview_error(std::string message,
                                            ModulationPreviewUpdate const &update)
    -> ModulationPreviewUpdateResult
{
    return {
        .status = {MessageLevel::Error, std::move(message)},
        .preview_id = update.preview_id,
        .accepted_update_sequence = 0,
        .accepted = false,
    };
}

[[nodiscard]] auto error_matches(std::optional<IpcError> const &error,
                                 std::string_view request_id) -> bool
{
    return error.has_value() &&
           (error->request_id.empty() || error->request_id == request_id);
}

} // namespace

IpcSequencerSessionClient::IpcSequencerSessionClient(
    InstanceBinding binding, std::optional<PersistedProcessorState> restore_state,
    std::filesystem::path helper_path)
    : juce::InterprocessConnection{false}, binding_{std::move(binding)},
      command_catalog_{create_command_catalog()}
{
    connect_or_throw(std::move(restore_state), std::move(helper_path));
}

IpcSequencerSessionClient::~IpcSequencerSessionClient()
{
    auto const should_wait_for_helper = request_helper_shutdown_if_idle();
    disconnect(500, Notify::no);
    if (should_wait_for_helper && helper_process_ != nullptr &&
        helper_process_->isRunning())
    {
        (void)helper_process_->waitForProcessToFinish(1000);
    }
}

auto IpcSequencerSessionClient::project_snapshot() const -> ProjectSnapshot
{
    auto const lock = std::scoped_lock{mutex_};
    return project_snapshot_;
}

auto IpcSequencerSessionClient::persistent_project_snapshot() const -> ProjectSnapshot
{
    auto const lock = std::scoped_lock{mutex_};
    return persistent_project_snapshot_;
}

auto IpcSequencerSessionClient::library_snapshot() const -> LibrarySnapshot
{
    auto const lock = std::scoped_lock{mutex_};
    return library_snapshot_;
}

auto IpcSequencerSessionClient::instance_binding() const -> InstanceBinding
{
    auto const lock = std::scoped_lock{mutex_};
    return binding_;
}

auto IpcSequencerSessionClient::command_catalog_metadata() const
    -> std::vector<CatalogCommandMetadata>
{
    return command_catalog_.metadata();
}

auto IpcSequencerSessionClient::execute_command_string(
    std::string const &command_string, CommandContext const &context)
    -> CommandApplicationResult
{
    auto const request_lock = std::scoped_lock{request_mutex_};
    auto request = CommandRequest{
        .request_id = next_request_id(),
        .source_instance_id = instance_binding().instance_id,
        .command = command_string,
        .context = context,
    };

    {
        auto const lock = std::scoped_lock{mutex_};
        if (!online_)
        {
            return error_result("XenSequencerCoordinator is offline.");
        }
        pending_command_response_.reset();
        pending_error_.reset();
    }

    send_json(encode_command_request(request));
    auto lock = std::unique_lock{mutex_};
    auto const received = response_ready_.wait_for(lock, std::chrono::seconds{5}, [&] {
        return (pending_command_response_.has_value() &&
                pending_command_response_->request_id == request.request_id) ||
               error_matches(pending_error_, request.request_id) || !online_;
    });
    if (!received)
    {
        return error_result("Timed out waiting for XenSequencerCoordinator.");
    }
    if (error_matches(pending_error_, request.request_id))
    {
        return error_result(pending_error_->message);
    }
    if (!pending_command_response_.has_value())
    {
        return error_result("XenSequencerCoordinator disconnected.");
    }
    auto response = std::move(*pending_command_response_);
    pending_command_response_.reset();
    ingest_project_snapshot(std::move(response.snapshot));
    submit_midi_compilation();
    return std::move(response.result);
}

auto IpcSequencerSessionClient::begin_preview(ProjectRevision expected_revision)
    -> PreviewControlResult
{
    auto const request_id = next_request_id();
    return finish_preview_request(
        encode_preview_begin_request({
            .request_id = request_id,
            .source_instance_id = instance_binding().instance_id,
            .expected_project_revision = expected_revision,
        }),
        request_id);
}

auto IpcSequencerSessionClient::begin_modulation_preview(
    ProjectRevision expected_revision, ModulationTarget target) -> PreviewControlResult
{
    auto const request_id = next_request_id();
    return finish_preview_request(
        encode_modulation_preview_begin_request({
            .request_id = request_id,
            .source_instance_id = instance_binding().instance_id,
            .expected_project_revision = expected_revision,
            .target = std::move(target),
        }),
        request_id);
}

auto IpcSequencerSessionClient::update_modulation_preview(
    ModulationPreviewUpdate const &update) -> ModulationPreviewUpdateResult
{
    auto const request_lock = std::scoped_lock{request_mutex_};
    auto const request_id = next_request_id();
    {
        auto const lock = std::scoped_lock{mutex_};
        if (!online_)
        {
            return modulation_preview_error("XenSequencerCoordinator is offline.",
                                            update);
        }
        pending_modulation_preview_update_response_.reset();
        pending_error_.reset();
    }

    send_json(encode_modulation_preview_update_request({
        .request_id = request_id,
        .source_instance_id = instance_binding().instance_id,
        .update = update,
    }));
    auto lock = std::unique_lock{mutex_};
    auto const received = response_ready_.wait_for(lock, std::chrono::seconds{5}, [&] {
        return (pending_modulation_preview_update_response_.has_value() &&
                pending_modulation_preview_update_response_->request_id ==
                    request_id) ||
               error_matches(pending_error_, request_id) || !online_;
    });
    if (!received)
    {
        return modulation_preview_error(
            "Timed out waiting for XenSequencerCoordinator.", update);
    }
    if (error_matches(pending_error_, request_id))
    {
        return modulation_preview_error(pending_error_->message, update);
    }
    if (!pending_modulation_preview_update_response_.has_value())
    {
        return modulation_preview_error("XenSequencerCoordinator disconnected.",
                                        update);
    }
    auto response = std::move(*pending_modulation_preview_update_response_);
    pending_modulation_preview_update_response_.reset();
    return std::move(response.result);
}

auto IpcSequencerSessionClient::commit_preview(PreviewId const &preview_id,
                                               ProjectRevision expected_revision)
    -> PreviewControlResult
{
    auto const request_id = next_request_id();
    return finish_preview_request(
        encode_preview_commit_request({
            .request_id = request_id,
            .source_instance_id = instance_binding().instance_id,
            .preview_id = preview_id,
            .expected_project_revision = expected_revision,
        }),
        request_id);
}

auto IpcSequencerSessionClient::cancel_preview(PreviewId const &preview_id,
                                               ProjectRevision expected_revision)
    -> PreviewControlResult
{
    auto const request_id = next_request_id();
    return finish_preview_request(
        encode_preview_cancel_request({
            .request_id = request_id,
            .source_instance_id = instance_binding().instance_id,
            .preview_id = preview_id,
            .expected_project_revision = expected_revision,
        }),
        request_id);
}

auto IpcSequencerSessionClient::create_project(ProjectRevision expected_revision,
                                               bool discard_unsaved)
    -> DocumentOperationResult
{
    return finish_document_request({
        .operation = "project.new",
        .expected_project_revision = expected_revision,
        .discard_unsaved = discard_unsaved,
    });
}

auto IpcSequencerSessionClient::open_project(std::string relative_path,
                                             ProjectRevision expected_revision,
                                             bool discard_unsaved)
    -> DocumentOperationResult
{
    return finish_document_request({
        .operation = "project.open",
        .relative_path = std::move(relative_path),
        .expected_project_revision = expected_revision,
        .discard_unsaved = discard_unsaved,
    });
}

auto IpcSequencerSessionClient::save_project(ProjectRevision expected_revision)
    -> DocumentOperationResult
{
    return finish_document_request({
        .operation = "project.save",
        .expected_project_revision = expected_revision,
    });
}

auto IpcSequencerSessionClient::save_project_as(
    std::string relative_path, ProjectRevision expected_revision,
    std::optional<std::string> expected_file_revision) -> DocumentOperationResult
{
    return finish_document_request({
        .operation = "project.save_as",
        .relative_path = std::move(relative_path),
        .expected_project_revision = expected_revision,
        .expected_file_revision = std::move(expected_file_revision),
    });
}

auto IpcSequencerSessionClient::restore_recovery(std::string recovery_revision,
                                                 ProjectRevision expected_revision,
                                                 bool discard_unsaved)
    -> DocumentOperationResult
{
    return finish_document_request({
        .operation = "project.recovery.restore",
        .expected_project_revision = expected_revision,
        .discard_unsaved = discard_unsaved,
        .recovery_revision = std::move(recovery_revision),
    });
}

auto IpcSequencerSessionClient::discard_recovery(std::string recovery_revision)
    -> DocumentOperationResult
{
    return finish_document_request({
        .operation = "project.recovery.discard",
        .recovery_revision = std::move(recovery_revision),
    });
}

auto IpcSequencerSessionClient::import_cell(std::string relative_path,
                                            ProjectRevision expected_revision,
                                            CompositionCursor cursor)
    -> DocumentOperationResult
{
    return finish_document_request({
        .operation = "cell.import",
        .relative_path = std::move(relative_path),
        .expected_project_revision = expected_revision,
        .cursor = cursor,
    });
}

auto IpcSequencerSessionClient::save_cell(
    std::string relative_path, ProjectRevision expected_revision,
    CompositionCursor cursor, SelectionPath selection,
    std::optional<std::string> expected_file_revision) -> DocumentOperationResult
{
    return finish_document_request({
        .operation = "cell.save",
        .relative_path = std::move(relative_path),
        .expected_project_revision = expected_revision,
        .expected_file_revision = std::move(expected_file_revision),
        .cursor = cursor,
        .selection = std::move(selection),
    });
}

auto IpcSequencerSessionClient::finish_document_request(DocumentRequest request)
    -> DocumentOperationResult
{
    auto const request_lock = std::scoped_lock{request_mutex_};
    request.request_id = next_request_id();
    {
        auto const lock = std::scoped_lock{mutex_};
        if (!online_)
        {
            throw DocumentError{DocumentErrorCode::Io,
                                "XenSequencerCoordinator is offline."};
        }
        pending_document_response_.reset();
        pending_error_.reset();
        request.source_instance_id = binding_.instance_id;
    }
    send_json(encode_document_request(request));
    auto lock = std::unique_lock{mutex_};
    auto const received = response_ready_.wait_for(lock, std::chrono::seconds{5}, [&] {
        return (pending_document_response_.has_value() &&
                pending_document_response_->request_id == request.request_id) ||
               error_matches(pending_error_, request.request_id) || !online_;
    });
    if (!received || !online_)
    {
        throw DocumentError{DocumentErrorCode::Io,
                            "XenSequencerCoordinator did not respond."};
    }
    if (error_matches(pending_error_, request.request_id))
    {
        auto const &error = *pending_error_;
        auto code = DocumentErrorCode::Io;
        if (error.code == "stale_project")
            code = DocumentErrorCode::StaleProject;
        else if (error.code == "unsaved_changes")
            code = DocumentErrorCode::UnsavedChanges;
        else if (error.code == "invalid_path")
            code = DocumentErrorCode::InvalidPath;
        else if (error.code == "not_found")
            code = DocumentErrorCode::NotFound;
        else if (error.code == "file_exists")
            code = DocumentErrorCode::FileExists;
        else if (error.code == "file_conflict")
            code = DocumentErrorCode::FileConflict;
        else if (error.code == "project_path_required")
            code = DocumentErrorCode::ProjectPathRequired;
        else if (error.code == "file_too_large")
            code = DocumentErrorCode::FileTooLarge;
        else if (error.code == "invalid_document")
            code = DocumentErrorCode::InvalidDocument;
        else if (error.code == "preview_active")
            code = DocumentErrorCode::PreviewActive;
        else if (error.code == "recovery_conflict")
            code = DocumentErrorCode::RecoveryConflict;
        throw DocumentError{code, error.message, error.current_file_revision};
    }
    if (!pending_document_response_.has_value())
    {
        throw DocumentError{DocumentErrorCode::Io,
                            "XenSequencerCoordinator disconnected."};
    }
    auto response = std::move(*pending_document_response_);
    pending_document_response_.reset();
    ingest_project_snapshot(std::move(response.result.snapshot));
    response.result.snapshot = project_snapshot_;
    submit_midi_compilation();
    return std::move(response.result);
}

auto IpcSequencerSessionClient::finish_preview_request(nlohmann::json request,
                                                       std::string const &request_id)
    -> PreviewControlResult
{
    auto const request_lock = std::scoped_lock{request_mutex_};
    {
        auto const lock = std::scoped_lock{mutex_};
        if (!online_)
        {
            return preview_error("XenSequencerCoordinator is offline.");
        }
        pending_preview_response_.reset();
        pending_error_.reset();
    }

    send_json(request);
    auto lock = std::unique_lock{mutex_};
    auto const received = response_ready_.wait_for(lock, std::chrono::seconds{5}, [&] {
        return (pending_preview_response_.has_value() &&
                pending_preview_response_->request_id == request_id) ||
               error_matches(pending_error_, request_id) || !online_;
    });
    if (!received)
    {
        return preview_error("Timed out waiting for XenSequencerCoordinator.");
    }
    if (error_matches(pending_error_, request_id))
    {
        return preview_error(pending_error_->message);
    }
    if (!pending_preview_response_.has_value())
    {
        return preview_error("XenSequencerCoordinator disconnected.");
    }
    auto response = std::move(*pending_preview_response_);
    pending_preview_response_.reset();
    ingest_project_snapshot(std::move(response.snapshot));
    submit_midi_compilation();
    return std::move(response.result);
}

void IpcSequencerSessionClient::set_channel_id(ChannelId channel_id)
{
    auto const request_lock = std::scoped_lock{request_mutex_};
    auto request = BindingSetRequest{
        .request_id = next_request_id(),
        .instance_id = instance_binding().instance_id,
        .channel_id = std::move(channel_id),
    };

    {
        auto const lock = std::scoped_lock{mutex_};
        if (!online_)
        {
            throw std::runtime_error{"XenSequencerCoordinator is offline."};
        }
        pending_binding_response_.reset();
        pending_error_.reset();
    }

    send_json(encode_binding_set_request(request));
    auto lock = std::unique_lock{mutex_};
    auto const received = response_ready_.wait_for(lock, std::chrono::seconds{5}, [&] {
        return (pending_binding_response_.has_value() &&
                pending_binding_response_->request_id == request.request_id) ||
               error_matches(pending_error_, request.request_id) || !online_;
    });
    if (!received)
    {
        throw std::runtime_error{"Timed out waiting for XenSequencerCoordinator."};
    }
    if (error_matches(pending_error_, request.request_id))
    {
        throw std::runtime_error{pending_error_->message};
    }
    if (!pending_binding_response_.has_value())
    {
        throw std::runtime_error{"XenSequencerCoordinator disconnected."};
    }

    binding_ = pending_binding_response_->binding;
    ingest_project_snapshot(std::move(pending_binding_response_->snapshot));
    pending_binding_response_.reset();
    submit_midi_compilation();
}

auto IpcSequencerSessionClient::compiled_midi_generation() const noexcept
    -> std::uint64_t
{
    return midi_compilation_.published_generation();
}

auto IpcSequencerSessionClient::try_consume_compiled_midi() noexcept
    -> std::optional<CompiledMidiMailbox::ReadView>
{
    return midi_compilation_.try_consume_latest();
}

auto IpcSequencerSessionClient::midi_compilation_status() const -> MidiCompilationStatus
{
    return midi_compilation_.status();
}

auto IpcSequencerSessionClient::online() const noexcept -> bool
{
    auto const lock = std::scoped_lock{mutex_};
    return online_;
}

auto IpcSequencerSessionClient::last_error() const -> std::string
{
    auto const lock = std::scoped_lock{mutex_};
    return last_error_;
}

void IpcSequencerSessionClient::connect_or_throw(
    std::optional<PersistedProcessorState> restore_state,
    std::filesystem::path helper_path)
{
    auto const registry =
        CoordinatorRegistry{CoordinatorRegistry::default_file(binding_.session_id)};
    helper_process_ = std::make_unique<juce::ChildProcess>();
    auto const entry = CoordinatorLauncher{std::move(helper_path)}.ensure_running(
        binding_.session_id, registry, helper_process_.get());
    if (!helper_process_->isRunning())
    {
        helper_process_.reset();
    }
    if (!connectToSocket("127.0.0.1", entry.port, 3000))
    {
        throw std::runtime_error{"Could not connect to XenSequencerCoordinator."};
    }

    send_json(encode_client_hello(
        {.binding = binding_, .restore_state = std::move(restore_state)}));
    auto lock = std::unique_lock{mutex_};
    auto const received = response_ready_.wait_for(lock, std::chrono::seconds{5}, [&] {
        return project_snapshot_.project_revision.value() != 0 ||
               pending_error_.has_value();
    });
    if (!received)
    {
        throw std::runtime_error{"Timed out waiting for coordinator hello."};
    }
    if (pending_error_.has_value())
    {
        throw std::runtime_error{pending_error_->message};
    }
}

auto IpcSequencerSessionClient::next_request_id() -> std::string
{
    auto const lock = std::scoped_lock{mutex_};
    return binding_.instance_id + "-" + std::to_string(next_request_id_++);
}

auto IpcSequencerSessionClient::request_helper_shutdown_if_idle() -> bool
{
    auto const request_lock = std::scoped_lock{request_mutex_};
    if (!isConnected())
    {
        return false;
    }

    auto request = ShutdownIfIdleRequest{.request_id = next_request_id()};

    {
        auto const lock = std::scoped_lock{mutex_};
        if (!online_)
        {
            return false;
        }
        pending_shutdown_response_.reset();
        pending_error_.reset();
    }

    send_json(encode_shutdown_if_idle_request(request));

    auto lock = std::unique_lock{mutex_};
    auto const received =
        response_ready_.wait_for(lock, std::chrono::milliseconds{250}, [&] {
            return (pending_shutdown_response_.has_value() &&
                    pending_shutdown_response_->request_id == request.request_id) ||
                   error_matches(pending_error_, request.request_id) || !online_;
        });
    if (!received || error_matches(pending_error_, request.request_id) ||
        !pending_shutdown_response_.has_value())
    {
        return false;
    }

    auto const will_exit = pending_shutdown_response_->will_exit;
    pending_shutdown_response_.reset();
    return will_exit;
}

void IpcSequencerSessionClient::submit_midi_compilation()
{
    if (last_midi_compilation_state_revision_ == project_snapshot_.state_revision &&
        last_midi_compilation_channel_id_ == binding_.channel_id)
    {
        return;
    }
    (void)midi_compilation_.submit(project_snapshot_.project, binding_.channel_id);
    last_midi_compilation_state_revision_ = project_snapshot_.state_revision;
    last_midi_compilation_channel_id_ = binding_.channel_id;
}

void IpcSequencerSessionClient::ingest_project_snapshot(ProjectSnapshot snapshot)
{
    if (snapshot.state_revision < project_snapshot_.state_revision)
    {
        return;
    }
    project_snapshot_ = std::move(snapshot);
    if (!project_snapshot_.preview_active)
    {
        persistent_project_snapshot_ = project_snapshot_;
        return;
    }
    if (project_snapshot_.state_revision >= persistent_project_snapshot_.state_revision)
    {
        persistent_project_snapshot_.state_revision = project_snapshot_.state_revision;
        persistent_project_snapshot_.document = project_snapshot_.document;
        persistent_project_snapshot_.recovery = project_snapshot_.recovery;
    }
}

void IpcSequencerSessionClient::send_json(nlohmann::json const &message)
{
    if (!sendMessage(memory_block_from_json(message)))
    {
        auto const lock = std::scoped_lock{mutex_};
        online_ = false;
        last_error_ = "Could not send message to XenSequencerCoordinator.";
        response_ready_.notify_all();
    }
}

void IpcSequencerSessionClient::connectionMade()
{
    auto const lock = std::scoped_lock{mutex_};
    online_ = true;
    last_error_.clear();
}

void IpcSequencerSessionClient::connectionLost()
{
    auto const lock = std::scoped_lock{mutex_};
    online_ = false;
    last_error_ = "XenSequencerCoordinator disconnected.";
    response_ready_.notify_all();
}

void IpcSequencerSessionClient::messageReceived(juce::MemoryBlock const &message)
{
    try
    {
        auto const json = json_from_memory_block(message);
        auto const type = json.at("type").get<std::string>();

        auto lock = std::unique_lock{mutex_};
        if (type == "coordinator.hello")
        {
            auto hello = decode_coordinator_hello(json);
            binding_ = std::move(hello.binding);
            project_snapshot_ = std::move(hello.snapshot);
            persistent_project_snapshot_ = std::move(hello.persistent_snapshot);
            library_snapshot_ = std::move(hello.library);
            submit_midi_compilation();
            response_ready_.notify_all();
            return;
        }
        if (type == "command.result")
        {
            pending_command_response_ = decode_command_response(json);
            response_ready_.notify_all();
            return;
        }
        if (type == "preview.result")
        {
            pending_preview_response_ = decode_preview_response(json);
            response_ready_.notify_all();
            return;
        }
        if (type == "modulation.preview.update.result")
        {
            pending_modulation_preview_update_response_ =
                decode_modulation_preview_update_response(json);
            response_ready_.notify_all();
            return;
        }
        if (type == "document.result")
        {
            pending_document_response_ = decode_document_response(json);
            response_ready_.notify_all();
            return;
        }
        if (type == "instance.binding.result")
        {
            pending_binding_response_ = decode_binding_set_response(json);
            response_ready_.notify_all();
            return;
        }
        if (type == "coordinator.shutdown_if_idle.result")
        {
            pending_shutdown_response_ = decode_shutdown_if_idle_response(json);
            response_ready_.notify_all();
            return;
        }
        if (type == "project.changed")
        {
            auto changed = decode_project_changed(json).snapshot;
            auto const accepted =
                changed.state_revision >= project_snapshot_.state_revision;
            ingest_project_snapshot(std::move(changed));
            if (accepted)
            {
                submit_midi_compilation();
            }
            response_ready_.notify_all();
            return;
        }
        if (type == "library.changed")
        {
            auto changed = decode_library_changed(json).snapshot;
            if (changed.library_revision >= library_snapshot_.library_revision)
            {
                library_snapshot_ = std::move(changed);
            }
            response_ready_.notify_all();
            return;
        }
        if (type == "instances.changed" || type == "heartbeat")
        {
            return;
        }
        if (type == "error")
        {
            pending_error_ = decode_error(json);
            last_error_ = pending_error_->message;
            response_ready_.notify_all();
            return;
        }
    }
    catch (std::exception const &e)
    {
        auto const lock = std::scoped_lock{mutex_};
        pending_error_ = IpcError{.code = "decode-error", .message = e.what()};
        last_error_ = e.what();
        response_ready_.notify_all();
    }
}

} // namespace xen::ipc
