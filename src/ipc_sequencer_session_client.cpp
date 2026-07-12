#include <xen/ipc_sequencer_session_client.hpp>

#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>

#include <xen/message_level.hpp>

namespace xen::ipc
{
namespace
{

[[nodiscard]] auto memory_block_from_json(nlohmann::json const &message)
    -> juce::MemoryBlock
{
    auto const text = message.dump();
    return juce::MemoryBlock{text.data(), text.size()};
}

[[nodiscard]] auto json_from_memory_block(juce::MemoryBlock const &message)
    -> nlohmann::json
{
    return nlohmann::json::parse(
        std::string{static_cast<char const *>(message.getData()), message.getSize()});
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

auto IpcSequencerSessionClient::instance_binding() const -> InstanceBinding const &
{
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
    auto request = CommandRequest{
        .request_id = next_request_id(),
        .source_instance_id = binding_.instance_id,
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
               pending_error_.has_value() || !online_;
    });
    if (!received)
    {
        return error_result("Timed out waiting for XenSequencerCoordinator.");
    }
    if (pending_error_.has_value())
    {
        return error_result(pending_error_->message);
    }
    if (!pending_command_response_.has_value())
    {
        return error_result("XenSequencerCoordinator disconnected.");
    }
    auto response = std::move(*pending_command_response_);
    pending_command_response_.reset();
    project_snapshot_ = response.snapshot;
    if (!response.snapshot.preview_active)
    {
        persistent_project_snapshot_ = response.snapshot;
    }
    publish_audio_snapshot();
    return std::move(response.result);
}

auto IpcSequencerSessionClient::begin_preview(ProjectRevision expected_revision)
    -> PreviewControlResult
{
    auto const request_id = next_request_id();
    return finish_preview_request(encode_preview_begin_request({
                                      .request_id = request_id,
                                      .source_instance_id = binding_.instance_id,
                                      .expected_project_revision = expected_revision,
                                  }),
                                  request_id);
}

auto IpcSequencerSessionClient::commit_preview(PreviewId const &preview_id,
                                               ProjectRevision expected_revision)
    -> PreviewControlResult
{
    auto const request_id = next_request_id();
    return finish_preview_request(encode_preview_commit_request({
                                      .request_id = request_id,
                                      .source_instance_id = binding_.instance_id,
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
    return finish_preview_request(encode_preview_cancel_request({
                                      .request_id = request_id,
                                      .source_instance_id = binding_.instance_id,
                                      .preview_id = preview_id,
                                      .expected_project_revision = expected_revision,
                                  }),
                                  request_id);
}

auto IpcSequencerSessionClient::finish_preview_request(nlohmann::json request,
                                                       std::string const &request_id)
    -> PreviewControlResult
{
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
               pending_error_.has_value() || !online_;
    });
    if (!received)
    {
        return preview_error("Timed out waiting for XenSequencerCoordinator.");
    }
    if (pending_error_.has_value())
    {
        return preview_error(pending_error_->message);
    }
    if (!pending_preview_response_.has_value())
    {
        return preview_error("XenSequencerCoordinator disconnected.");
    }
    auto response = std::move(*pending_preview_response_);
    pending_preview_response_.reset();
    project_snapshot_ = response.snapshot;
    if (!response.snapshot.preview_active)
    {
        persistent_project_snapshot_ = response.snapshot;
    }
    publish_audio_snapshot();
    return std::move(response.result);
}

void IpcSequencerSessionClient::set_channel_id(ChannelId channel_id)
{
    auto request = BindingSetRequest{
        .request_id = next_request_id(),
        .instance_id = binding_.instance_id,
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
               pending_error_.has_value() || !online_;
    });
    if (!received)
    {
        throw std::runtime_error{"Timed out waiting for XenSequencerCoordinator."};
    }
    if (pending_error_.has_value())
    {
        throw std::runtime_error{pending_error_->message};
    }
    if (!pending_binding_response_.has_value())
    {
        throw std::runtime_error{"XenSequencerCoordinator disconnected."};
    }

    binding_ = pending_binding_response_->binding;
    project_snapshot_ = pending_binding_response_->snapshot;
    pending_binding_response_.reset();
    publish_audio_snapshot();
}

auto IpcSequencerSessionClient::audio_project_update_version() const noexcept
    -> std::uint64_t
{
    return pending_engine_state_update_.version();
}

auto IpcSequencerSessionClient::try_consume_audio_project_update() noexcept
    -> std::optional<EngineStateMailbox::ReadView>
{
    return pending_engine_state_update_.try_consume_latest();
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
                   pending_error_.has_value() || !online_;
        });
    if (!received || pending_error_.has_value() ||
        !pending_shutdown_response_.has_value())
    {
        return false;
    }

    auto const will_exit = pending_shutdown_response_->will_exit;
    pending_shutdown_response_.reset();
    return will_exit;
}

void IpcSequencerSessionClient::publish_audio_snapshot()
{
    pending_engine_state_update_.publish(AudioProjectSnapshot{
        .project = project_snapshot_.project,
        .channel_id = binding_.channel_id,
    });
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
            publish_audio_snapshot();
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
            project_snapshot_ = decode_project_changed(json).snapshot;
            if (!project_snapshot_.preview_active)
            {
                persistent_project_snapshot_ = project_snapshot_;
            }
            publish_audio_snapshot();
            response_ready_.notify_all();
            return;
        }
        if (type == "library.changed")
        {
            library_snapshot_ = decode_library_changed(json).snapshot;
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
