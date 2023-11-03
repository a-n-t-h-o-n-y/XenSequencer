#include <xen/active_sessions.hpp>

#include <stdexcept>
#include <string>
#include <variant>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <nlohmann/json.hpp>
#include <nng/nng.h>
#include <nng/protocol/bus0/bus.h>
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>

#include <sequence/utility.hpp>

#include <xen/serialize.hpp>

namespace
{

/**
 * Build IPC endpoint based on a string identifier.
 *
 * @param identifier The string identifier for the endpoint.
 * @return The IPC endpoint as a string.
 */
[[nodiscard]] auto build_ipc_endpoint(std::string const &identifier) -> std::string
{
#if JUCE_WINDOWS
    // Windows IPC endpoint format
    return "ipc://\\\\.\\pipe\\" + identifier;
#elif JUCE_LINUX || JUCE_MAC
    // POSIX (Linux and macOS) IPC endpoint format
    return "ipc:///tmp/" + identifier;
#else
    throw std::runtime_error{"Unsupported platform"};
#endif
}

} // namespace

namespace xen
{

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

auto pull_socket(std::string const &address, int timeout) -> nng_socket
{
    auto socket = nng_socket{};

    if (auto const result = nng_pull0_open(&socket); result != 0)
    {
        throw std::runtime_error{"Failed to open pull socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (auto const result = nng_listen(socket, address.c_str(), nullptr, 0);
        result != 0)
    {
        throw std::runtime_error{"Failed to listen on pull socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (timeout > 0)
    {
        auto const result = nng_socket_set_ms(socket, NNG_OPT_RECVTIMEO, timeout);
        if (result != 0)
        {
            throw std::runtime_error{"Failed to set pull socket timeout: " +
                                     std::string{nng_strerror(result)}};
        }
    }

    return socket;
}

auto push_socket(std::string const &address, int timeout) -> nng_socket
{
    auto socket = nng_socket{};

    if (auto const result = nng_push0_open(&socket); result != 0)
    {
        throw std::runtime_error{"Failed to open push socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (auto const result = nng_dial(socket, address.c_str(), nullptr, 0); result != 0)
    {
        throw std::runtime_error{"Failed to dial on push socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (timeout > 0)
    {
        auto const result = nng_socket_set_ms(socket, NNG_OPT_SENDTIMEO, timeout);
        if (result != 0)
        {
            throw std::runtime_error{"Failed to set push socket timeout: " +
                                     std::string{nng_strerror(result)}};
        }
    }

    return socket;
}

auto bus_receive_socket(std::string const &address, int timeout) -> nng_socket
{
    auto socket = nng_socket{};

    if (auto const result = nng_bus0_open(&socket); result != 0)
    {
        throw std::runtime_error{"Failed to open bus receive socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (auto const result = nng_listen(socket, address.c_str(), nullptr, 0);
        result != 0)
    {
        throw std::runtime_error{"Failed to listen on bus receive socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (timeout > 0)
    {
        auto const result = nng_socket_set_ms(socket, NNG_OPT_RECVTIMEO, timeout);
        if (result != 0)
        {
            throw std::runtime_error{"Failed to set bus receive socket timeout: " +
                                     std::string{nng_strerror(result)}};
        }
    }

    return socket;
}

[[nodiscard]] auto bus_send_socket(std::string const &address, int timeout)
    -> nng_socket
{
    auto socket = nng_socket{};

    if (auto const result = nng_bus0_open(&socket); result != 0)
    {
        throw std::runtime_error{"Failed to open bus send socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (auto const result = nng_dial(socket, address.c_str(), nullptr, 0); result != 0)
    {
        throw std::runtime_error{"Failed to dial on bus send socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (timeout > 0)
    {
        auto const result = nng_socket_set_ms(socket, NNG_OPT_SENDTIMEO, timeout);
        if (result != 0)
        {
            throw std::runtime_error{"Failed to set push socket timeout: " +
                                     std::string{nng_strerror(result)}};
        }
    }

    return socket;
}

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

ListenSocket::ListenSocket(nng_socket const &socket) : socket_{socket}
{
}

ListenSocket::~ListenSocket()
{
    nng_close(socket_);
}

auto ListenSocket::listen() const -> std::optional<std::string>
{
    nng_msg *msg;

    if (auto const result = nng_recvmsg(socket_, &msg, 0); result == NNG_ETIMEDOUT)
    {
        return std::nullopt;
    }
    else if (result != 0)
    {
        throw std::runtime_error{"Failed to receive message"};
    }

    auto const data = nng_msg_body(msg);
    auto const len = nng_msg_len(msg);
    auto const received_message = std::string(static_cast<char *>(data), len);
    nng_msg_free(msg);

    return received_message;
}

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

SendSocket::SendSocket(nng_socket socket) : socket_{socket}
{
}

SendSocket::~SendSocket()
{
    nng_close(socket_);
}

auto SendSocket::send(std::string const &message) const -> bool
{
    auto const result =
        nng_send(socket_, const_cast<char *>(message.data()), message.size(), 0);

    if (result == NNG_ETIMEDOUT)
    {
        return false;
    }
    else if (result != 0)
    {
        throw std::runtime_error{"Failed to receive message: " +
                                 std::string{nng_strerror(result)}};
    }
    else
    {
        return true;
    }
}

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

Mailbox::Mailbox(juce::Uuid const &uuid)
    : juce::Thread{"MailboxListener"},
      pull_socket_{
          pull_socket(build_ipc_endpoint(uuid.toString().toStdString()), timeout_)}
{
    this->startThread();
}

Mailbox::~Mailbox()
{
    this->signalThreadShouldExit();
    this->stopThread(timeout_);
}

auto Mailbox::run() -> void
{
    while (!this->threadShouldExit())
    {
        if (auto data = pull_socket_.listen(); data.has_value())
        {
            juce::MessageManager::callAsync(
                [this, d = std::move(*data)] { this->on_message.emit(d); });
        }
    }
}

auto Mailbox::send_to(juce::Uuid const &target_uuid, std::string const &message) const
    -> void
{
    auto const target_address =
        build_ipc_endpoint(target_uuid.toString().toStdString());
    auto socket = SendSocket{push_socket(target_address, timeout_)};
    if (!socket.send(message))
    {
        throw std::runtime_error{"Mailbox::send_to() Timed Out.\n" + message};
    }
}

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

Broadcast::Broadcast()
    : juce::Thread{"XenBroadcast"}, bus_receive_socket_{bus_receive_socket(
                                        build_ipc_endpoint("xen_broadcast"), timeout_)},
      bus_send_socket_{bus_send_socket(build_ipc_endpoint("xen_broadcast"), timeout_)}
{
    this->startThread();
}

Broadcast::~Broadcast()
{
    this->signalThreadShouldExit();
    this->stopThread(timeout_);
}

auto Broadcast::run() -> void
{
    while (!this->threadShouldExit())
    {
        if (auto data = bus_receive_socket_.listen(); data.has_value())
        {
            juce::MessageManager::callAsync(
                [this, d = std::move(*data)]() { this->on_message.emit(d); });
        }
    }
}

auto Broadcast::send(std::string const &message) const -> void
{
    if (!bus_send_socket_.send(message))
    {
        throw std::runtime_error{"Broadcast::send() Timed Out.\n" + message};
    }
}

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

auto serialize(Message const &m) -> std::string
{
    return std::visit(sequence::utility::overload{
                          [](SessionStartMessage const &x) {
                              return nlohmann::json{
                                  {"type", "SessionStartMessage"},
                                  {"id", x.id.uuid.toString().toStdString()},
                                  {"display_name", x.id.display_name},
                              }
                                  .dump();
                          },
                          [](SessionEndMessage const &x) {
                              return nlohmann::json{
                                  {"type", "SessionEndMessage"},
                                  {"id", x.id.uuid.toString().toStdString()},
                              }
                                  .dump();
                          },
                          [](IDUpdateMessage const &x) {
                              return nlohmann::json{
                                  {"type", "IDUpdateMessage"},
                                  {"id", x.id.uuid.toString().toStdString()},
                              }
                                  .dump();
                          },
                          [](GetStateRequest const &x) {
                              return nlohmann::json{
                                  {"type", "GetStateRequest"},
                                  {"request_id", x.request_id.toString().toStdString()},
                                  {"reply_id", x.reply_id.toString().toStdString()},
                              }
                                  .dump();
                          },
                          [](GetStateResponse const &x) {
                              return nlohmann::json{
                                  {"type", "GetStateResponse"},
                                  {"state", serialize_state(x.state)},
                              }
                                  .dump();
                          },
                      },
                      m);
}

auto deserialize(std::string const &x) -> Message
{
    auto const j = nlohmann::json::parse(x);
    auto const type = j.at("type").get<std::string>();
    if (type == "SessionStartMessage")
    {
        return SessionStartMessage{SessionID{
            juce::Uuid{j.at("id").get<std::string>()},
            j.at("display_name").get<std::string>(),
        }};
    }
    else if (type == "SessionEndMessage")
    {
        return SessionEndMessage{
            SessionID{juce::Uuid{j.at("id").get<std::string>()}, ""},
        };
    }
    else if (type == "IDUpdateMessage")
    {
        return IDUpdateMessage{
            SessionID{juce::Uuid{j.at("id").get<std::string>()}, ""},
        };
    }
    else if (type == "GetStateRequest")
    {
        return GetStateRequest{
            juce::Uuid{
                j.at("request_id").get<std::string>(),
            },
            juce::Uuid{
                j.at("reply_id").get<std::string>(),
            },
        };
    }
    else if (type == "GetStateResponse")
    {
        return GetStateResponse{
            deserialize_state(j.at("state").dump()),
        };
    }
    else
    {
        throw std::runtime_error{"Invalid message type: " + type};
    }
}

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

XenIPC::XenIPC(juce::Uuid const &current_process_id)
    : mailbox_{current_process_id}, broadcast_{}
{
    mailbox_.on_message.connect([this](std::string const &message) {
        auto const msg = deserialize(message);
        this->on_receive(msg);
    });

    broadcast_.on_message.connect([this](std::string const &message) {
        auto const msg = deserialize(message);
        this->on_receive(msg);
    });
}

auto XenIPC::send_to(juce::Uuid const &uuid, Message const &m) const -> void
{
    auto const message = serialize(m);
    mailbox_.send_to(uuid, message);
}

auto XenIPC::broadcast(Message const &m) const -> void
{
    auto const message = serialize(m);
    broadcast_.send(message);
}

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

} // namespace xen

// namespace
// {

// namespace bi = boost::interprocess;

// /**
//  * @brief Receive a message from the given queue, waiting for 500ms
//  *
//  * @param queue The queue to receive from.
//  * @return std::optional<std::string> The message, or std::nullopt if no message
//  * was received.
//  */
// [[nodiscard]] auto receive_str_message(boost::interprocess::message_queue &queue)
//     -> std::optional<std::string>
// {
//     // TODO this should only need to be 1024, also this should be a single variable
//     you
//     // can change in a single place.
//     auto const max_message_size = std::size_t{102'400};
//     auto buffer = std::vector<char>(max_message_size);
//     auto const wait_time = boost::posix_time::microsec_clock::universal_time() +
//                            boost::posix_time::milliseconds(500);

//     auto received_size = std::size_t{0};
//     unsigned int priority;

//     if (queue.timed_receive(buffer.data(), max_message_size, received_size, priority,
//                             wait_time))
//     {
//         if (received_size >= 4) // Ensure that at least the size header is present
//         {
//             auto size = *reinterpret_cast<std::uint32_t *>(buffer.data());
//             if (size <= received_size - 4)
//             {
//                 return std::string(buffer.data() + 4, buffer.data() + 4 + size);
//             }
//             else
//             {
//                 throw std::runtime_error{
//                     "Message size is larger than max message length of queue."};
//             }
//         }
//     }
//     return std::nullopt;
// }

// void add_size_prefix(std::string &x)
// {
//     // Convert the message size to a 4-byte value
//     std::uint32_t size = static_cast<std::uint32_t>(x.size());

//     // Create a 4-byte buffer to hold the size
//     char size_buffer[4];

//     // Copy the size into the buffer
//     std::memcpy(size_buffer, &size, 4);

//     // Prepend the buffer to the original string
//     x.insert(0, size_buffer, 4);
// }

// auto get_reply_queue(juce::Uuid uuid) -> bi::message_queue
// {
//     return bi::message_queue{bi::open_only,
//                              ("ReplyQueue" + uuid.toString().toStdString()).c_str()};
// }

// } // namespace

// namespace xen
// {

// QueueListener::QueueListener(boost::interprocess::message_queue &queue,
//                              std::string const &name)
//     : juce::Thread{name + "QueueListener"}, queue_{queue}
// {
//     if (!this->startThread())
//     {
//         throw std::runtime_error{"Failed to start QueueListener thread"};
//     }
// }

// void QueueListener::run()
// {
//     while (!this->threadShouldExit())
//     {
//         auto const msg_str = receive_str_message(queue_);
//         if (msg_str)
//         {
//             // Emit signal on the main thread.
//             message::Message const msg = message::deserialize(*msg_str);
//             juce::MessageManager::callAsync([this, msg] { this->on_message(msg); });
//         }
//     }
// }

// //
// ----------------------------------------------------------------------------------------------

// ActiveSessions::ActiveSessions()
//     : broadcast_queue_{bi::open_or_create, "BroadcastQueue", 100, 1'024},
//       current_reply_queue_{
//           bi::open_or_create,
//           ("ReplyQueue" + current_process_uuid.toString().toStdString()).c_str(),
//           100, 1'024}
// {
//     broadcast_listener_.on_message.connect(
//         [this](message::Message const &m) { this->emit_signal(m); });

//     current_reply_listener_.on_message.connect(
//         [this](message::Message const &m) { this->emit_signal(m); });
// }

// auto ActiveSessions::get_current_session_uuid() const -> juce::Uuid
// {
//     return current_process_uuid;
// }

// void ActiveSessions::broadcast(message::Message const &m)
// {
//     auto json_str = message::serialize(m);
//     add_size_prefix(json_str);
//     broadcast_queue_.send(json_str.data(), json_str.size(), 0);
// }

// void ActiveSessions::send_to(juce::Uuid uuid, message::Message const &m)
// {
//     auto reply_queue = get_reply_queue(uuid);

//     auto json_str = message::serialize(m);
//     add_size_prefix(json_str);
//     current_reply_queue_.send(json_str.data(), json_str.size(), 0);
// }

// void ActiveSessions::emit_signal(message::Message const &m)
// {
//     std::cout << "ActiveSessions::emit_signal: " << message::serialize(m) <<
//     std::endl; std::visit(sequence::utility::overload{
//                    [this](message::SessionStart const &x) {
//                        // TODO XenProcessor will connect to this and send_to(x.id,
//                        s_id) this->on_session_start(x.id);
//                    },
//                    [this](message::SessionEnd const &x) { this->on_session_end(x.id);
//                    }, [this](message::SessionIDUpdate const &x) {
//                        this->on_session_id_update(x.id);
//                    },
//                    [this](message::GetStateRequest const &x) {
//                        if (x.request_id == current_process_uuid)
//                        {
//                            // TODO XenProcessor will handle this by calling send_to
//                            this->on_state_request(x.reply_id);
//                        }
//                    },
//                    [this](message::GetStateResponse const &x) {
//                        // We do not need uuid of source here.
//                        this->on_session_state_response(x.state);
//                    },
//                    [this](message::SessionIDResponse const &x) {
//                        this->on_session_id_update(x.id);
//                    }},
//                m);
// }

// } // namespace xen