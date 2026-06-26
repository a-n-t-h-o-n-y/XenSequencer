#include <xen/coordinator_server.hpp>

#include <algorithm>
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

[[nodiscard]] auto json_from_memory_block(juce::MemoryBlock const &message)
    -> nlohmann::json
{
    return nlohmann::json::parse(
        std::string{static_cast<char const *>(message.getData()), message.getSize()});
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
                auto const hello =
                    server_.coordinator().connect(decode_client_hello(json));
                (void)sendMessage(
                    memory_block_from_json(encode_coordinator_hello(hello)));
                server_.broadcast(encode_instances_changed(
                    {.instances = server_.coordinator().instances()}));
                server_.broadcast(encode_project_changed(
                    {.snapshot = server_.coordinator().snapshot()}));
                return;
            }
            if (type == "command.execute")
            {
                auto const response =
                    server_.coordinator().execute(decode_command_request(json));
                (void)sendMessage(
                    memory_block_from_json(encode_command_response(response)));
                server_.broadcast(
                    encode_project_changed({.snapshot = response.snapshot}));
                return;
            }
            if (type == "instance.binding.set")
            {
                auto const response =
                    server_.coordinator().set_binding(decode_binding_set_request(json));
                (void)sendMessage(
                    memory_block_from_json(encode_binding_set_response(response)));
                server_.broadcast(encode_instances_changed(
                    {.instances = server_.coordinator().instances()}));
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
        catch (std::exception const &e)
        {
            append_coordinator_error_log(
                juce::String{"XenSequencer coordinator IPC exception: "} + e.what() +
                "\nraw_message: " +
                juce::String::fromUTF8(static_cast<char const *>(message.getData()),
                                       static_cast<int>(message.getSize())));
            (void)sendMessage(memory_block_from_json(
                encode_error({.code = "ipc-error", .message = e.what()})));
        }
    }

    void send_json(nlohmann::json const &message)
    {
        (void)sendMessage(memory_block_from_json(message));
    }

  private:
    CoordinatorServer &server_;
};

CoordinatorServer::CoordinatorServer(SessionId session_id,
                                     std::filesystem::path registry_file)
    : session_id_{std::move(session_id)}, registry_{std::move(registry_file)}
{
}

CoordinatorServer::~CoordinatorServer()
{
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
    auto const lock = std::scoped_lock{connections_mutex_};
    auto const connected = std::ranges::count_if(
        connections_, [](auto const &item) { return item->isConnected(); });
    client_count_.store(static_cast<int>(connected));
    last_disconnect_ms_.store(juce::Time::currentTimeMillis());
    (void)connection;
}

auto CoordinatorServer::coordinator() noexcept -> SessionCoordinator &
{
    return coordinator_;
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
