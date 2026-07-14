#include <xen/coordinator_server.hpp>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <random>
#include <string>
#include <utility>

#include <juce_core/juce_core.h>

#include <xen/user_directory.hpp>

namespace xen::ipc
{
namespace
{

constexpr auto MAX_IPC_MESSAGE_BYTES = std::size_t{160 * 1'024 * 1'024};
constexpr auto MAX_IPC_MESSAGE_DEPTH = std::size_t{384};
constexpr auto MAX_IPC_MESSAGE_EVENTS = std::size_t{10'000'000};

void append_coordinator_error_log(juce::String const &message)
{
    juce::Logger::writeToLog(message);

    try
    {
        auto const log_file =
            xen::get_user_settings_directory().getChildFile("coordinator-errors.log");
        log_file.appendText(message + "\n\n", false, false, "\n");
    }
    catch (std::exception const &)
    {
    }
}

[[nodiscard]] auto memory_block_from_json(nlohmann::json const &message)
    -> juce::MemoryBlock
{
    auto const text = message.dump();
    return juce::MemoryBlock{text.data(), text.size()};
}

[[nodiscard]] auto memory_block_excerpt(juce::MemoryBlock const &message)
    -> juce::String
{
    auto constexpr maximum = std::size_t{4096};
    auto const size = std::min(message.getSize(), maximum);
    auto excerpt = juce::String::fromUTF8(static_cast<char const *>(message.getData()),
                                          static_cast<int>(size));
    if (message.getSize() > maximum)
    {
        excerpt += "...<truncated>";
    }
    return excerpt;
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

[[nodiscard]] auto request_id_from_message(juce::MemoryBlock const &message) noexcept
    -> std::string
{
    try
    {
        return json_from_memory_block(message)
            .value("payload", nlohmann::json::object())
            .value("request_id", std::string{});
    }
    catch (...)
    {
        return {};
    }
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

} // namespace

class CoordinatorConnection final : public juce::InterprocessConnection
{
  public:
    explicit CoordinatorConnection(CoordinatorServer &server)
        : juce::InterprocessConnection{false}, server_{server}
    {
    }

    ~CoordinatorConnection() override
    {
        disconnect(200, Notify::no);
    }

    void connectionMade() override
    {
    }

    [[nodiscard]] auto instance_id() const -> std::optional<InstanceId> const &
    {
        return instance_id_;
    }

    void connectionLost() override
    {
        server_.connection_closed(*this);
    }

    void messageReceived(juce::MemoryBlock const &message) override
    {
        try
        {
            auto const json = json_from_memory_block(message);
            auto const type = json.at("type").get<std::string>();
            if (type == "client.hello")
            {
                auto request = decode_client_hello(json);
                if (request.binding.session_id != server_.session_id_)
                {
                    throw std::invalid_argument{
                        "Client session ID does not match this coordinator."};
                }
                auto const hello = [&] {
                    auto const lock = std::scoped_lock{server_.coordinator_mutex_};
                    return server_.coordinator().connect(std::move(request));
                }();
                instance_id_ = hello.binding.instance_id;
                (void)sendMessage(
                    memory_block_from_json(encode_coordinator_hello(hello)));
                server_.broadcast(
                    encode_instances_changed({.instances = hello.instances}));
                server_.broadcast(encode_project_changed({.snapshot = hello.snapshot}));
                return;
            }
            if (type == "command.execute")
            {
                auto const response = [&] {
                    auto const lock = std::scoped_lock{server_.coordinator_mutex_};
                    return server_.coordinator().execute(decode_command_request(json));
                }();
                (void)sendMessage(
                    memory_block_from_json(encode_command_response(response)));
                server_.broadcast(
                    encode_project_changed({.snapshot = response.snapshot}));
                auto library = [&] {
                    auto const lock = std::scoped_lock{server_.coordinator_mutex_};
                    return server_.coordinator().library_snapshot();
                }();
                server_.broadcast(
                    encode_library_changed({.snapshot = std::move(library)}));
                return;
            }
            if (type == "document.execute")
            {
                auto const response = [&] {
                    auto const lock = std::scoped_lock{server_.coordinator_mutex_};
                    return server_.coordinator().execute_document(
                        decode_document_request(json));
                }();
                (void)sendMessage(
                    memory_block_from_json(encode_document_response(response)));
                server_.broadcast(
                    encode_project_changed({.snapshot = response.result.snapshot}));
                auto library = [&] {
                    auto const lock = std::scoped_lock{server_.coordinator_mutex_};
                    return server_.coordinator().library_snapshot();
                }();
                server_.broadcast(
                    encode_library_changed({.snapshot = std::move(library)}));
                return;
            }
            if (type == "preview.begin")
            {
                auto const response = [&] {
                    auto const lock = std::scoped_lock{server_.coordinator_mutex_};
                    return server_.coordinator().begin_preview(
                        decode_preview_begin_request(json));
                }();
                (void)sendMessage(
                    memory_block_from_json(encode_preview_response(response)));
                server_.broadcast(
                    encode_project_changed({.snapshot = response.snapshot}));
                return;
            }
            if (type == "preview.commit")
            {
                auto const response = [&] {
                    auto const lock = std::scoped_lock{server_.coordinator_mutex_};
                    return server_.coordinator().commit_preview(
                        decode_preview_commit_request(json));
                }();
                (void)sendMessage(
                    memory_block_from_json(encode_preview_response(response)));
                server_.broadcast(
                    encode_project_changed({.snapshot = response.snapshot}));
                return;
            }
            if (type == "preview.cancel")
            {
                auto const response = [&] {
                    auto const lock = std::scoped_lock{server_.coordinator_mutex_};
                    return server_.coordinator().cancel_preview(
                        decode_preview_cancel_request(json));
                }();
                (void)sendMessage(
                    memory_block_from_json(encode_preview_response(response)));
                server_.broadcast(
                    encode_project_changed({.snapshot = response.snapshot}));
                return;
            }
            if (type == "instance.binding.set")
            {
                auto const [response, instances] = [&] {
                    auto const lock = std::scoped_lock{server_.coordinator_mutex_};
                    auto response = server_.coordinator().set_binding(
                        decode_binding_set_request(json));
                    return std::pair{std::move(response),
                                     server_.coordinator().instances()};
                }();
                (void)sendMessage(
                    memory_block_from_json(encode_binding_set_response(response)));
                server_.broadcast(encode_instances_changed({.instances = instances}));
                server_.broadcast(
                    encode_project_changed({.snapshot = response.snapshot}));
                return;
            }
            if (type == "heartbeat")
            {
                (void)sendMessage(message);
                return;
            }
            if (type == "coordinator.shutdown_if_idle")
            {
                auto const request = decode_shutdown_if_idle_request(json);
                auto const will_exit = server_.request_shutdown_if_idle();
                (void)sendMessage(
                    memory_block_from_json(encode_shutdown_if_idle_response(
                        {.request_id = request.request_id, .will_exit = will_exit})));
                return;
            }

            (void)sendMessage(memory_block_from_json(
                encode_error({.code = "unknown-message",
                              .message = "Unknown coordinator IPC message type."})));
        }
        catch (DocumentError const &error)
        {
            (void)sendMessage(memory_block_from_json(encode_error({
                .request_id = request_id_from_message(message),
                .code = document_error_code(error.code),
                .message = error.what(),
                .current_file_revision = error.current_file_revision,
            })));
        }
        catch (std::exception const &e)
        {
            append_coordinator_error_log(
                juce::String{"XenSequencer coordinator IPC exception: "} + e.what() +
                "\nraw_message: " + memory_block_excerpt(message));
            (void)sendMessage(memory_block_from_json(
                encode_error({.request_id = request_id_from_message(message),
                              .code = "ipc-error",
                              .message = e.what()})));
        }
    }

    void send_json(nlohmann::json const &message)
    {
        (void)sendMessage(memory_block_from_json(message));
    }

  private:
    CoordinatorServer &server_;
    std::optional<InstanceId> instance_id_{};
};

CoordinatorServer::CoordinatorServer(SessionId session_id,
                                     std::filesystem::path registry_file)
    : session_id_{std::move(session_id)}, registry_{std::move(registry_file)}
{
}

CoordinatorServer::~CoordinatorServer()
{
    try
    {
        perform_maintenance(static_cast<std::uint64_t>(juce::Time::currentTimeMillis()),
                            true);
    }
    catch (std::exception const &error)
    {
        append_coordinator_error_log(
            juce::String{"XenSequencer shutdown recovery error: "} + error.what());
    }
    stop();
    registry_.clear();
}

auto CoordinatorServer::start() -> CoordinatorRegistryEntry
{
    auto bound_port = -1;
    auto random = std::mt19937{std::random_device{}()};
    auto first_port =
        49152 + static_cast<int>(random() % static_cast<std::uint32_t>(512));
    for (auto offset = 0; offset < 512; ++offset)
    {
        auto const port = 49152 + ((first_port - 49152 + offset) % 512);
        if (beginWaitingForSocket(port))
        {
            bound_port = port;
            break;
        }
    }
    if (bound_port <= 0)
    {
        throw std::runtime_error{"Could not start coordinator IPC server."};
    }

    auto entry = CoordinatorRegistryEntry{
        .session_id = session_id_,
        .port = bound_port,
        .pid = CoordinatorRegistry::current_process_id(),
        .nonce = juce::Uuid{}.toString().toStdString(),
    };
    registry_.write(entry);
    last_disconnect_ms_.store(juce::Time::currentTimeMillis());
    return entry;
}

auto CoordinatorServer::client_count() const noexcept -> int
{
    return client_count_.load();
}

auto CoordinatorServer::idle_for_ms() const noexcept -> int64_t
{
    if (client_count() != 0)
    {
        return 0;
    }
    return juce::Time::currentTimeMillis() - last_disconnect_ms_.load();
}

auto CoordinatorServer::shutdown_requested() const noexcept -> bool
{
    return shutdown_requested_.load();
}

void CoordinatorServer::broadcast(nlohmann::json const &message)
{
    auto const lock = std::scoped_lock{connections_mutex_};
    std::erase_if(connections_,
                  [](auto const &connection) { return !connection->isConnected(); });
    for (auto const &connection : connections_)
    {
        connection->send_json(message);
    }
    client_count_.store(static_cast<int>(connections_.size()));
}

void CoordinatorServer::connection_closed(CoordinatorConnection &connection)
{
    auto restored = std::optional<ProjectSnapshot>{};
    auto instances = std::optional<std::vector<InstanceBinding>>{};
    if (connection.instance_id().has_value())
    {
        auto const lock = std::scoped_lock{coordinator_mutex_};
        restored = coordinator_.disconnect(*connection.instance_id());
        instances = coordinator_.instances();
    }
    {
        auto const lock = std::scoped_lock{connections_mutex_};
        auto const connected = std::ranges::count_if(
            connections_, [](auto const &item) { return item->isConnected(); });
        client_count_.store(static_cast<int>(connected));
        last_disconnect_ms_.store(juce::Time::currentTimeMillis());
    }
    if (restored.has_value())
    {
        broadcast(encode_project_changed({.snapshot = std::move(*restored)}));
    }
    if (instances.has_value())
    {
        broadcast(encode_instances_changed({.instances = std::move(*instances)}));
    }
}

auto CoordinatorServer::coordinator() noexcept -> SessionCoordinator &
{
    return coordinator_;
}

void CoordinatorServer::perform_maintenance(std::uint64_t now_unix_ms, bool force)
{
    auto changed = false;
    auto snapshot = ProjectSnapshot{};
    {
        auto const lock = std::scoped_lock{coordinator_mutex_};
        auto const before = coordinator_.snapshot().state_revision;
        try
        {
            coordinator_.perform_recovery_maintenance(now_unix_ms, force);
        }
        catch (std::exception const &error)
        {
            append_coordinator_error_log(
                juce::String{"XenSequencer recovery write error: "} + error.what());
        }
        snapshot = coordinator_.snapshot();
        changed = snapshot.state_revision != before;
    }
    if (changed)
    {
        broadcast(encode_project_changed({.snapshot = std::move(snapshot)}));
    }
}

auto CoordinatorServer::request_shutdown_if_idle() noexcept -> bool
{
    if (client_count() <= 1)
    {
        shutdown_requested_.store(true);
        return true;
    }
    return false;
}

auto CoordinatorServer::createConnectionObject() -> juce::InterprocessConnection *
{
    auto connection = std::make_unique<CoordinatorConnection>(*this);
    auto *raw = connection.get();
    auto const lock = std::scoped_lock{connections_mutex_};
    std::erase_if(connections_, [](auto const &item) { return !item->isConnected(); });
    connections_.push_back(std::move(connection));
    client_count_.store(static_cast<int>(connections_.size()));
    return raw;
}

} // namespace xen::ipc
