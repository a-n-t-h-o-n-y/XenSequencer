#include <xen/active_sessions.hpp>

#include <stdexcept>
#include <string>
#include <variant>

#include <boost/interprocess/ipc/message_queue.hpp>
#include <juce_events/juce_events.h>
#include <nlohmann/json.hpp>

#include <sequence/utility.hpp>

#include <xen/serialize.hpp>

namespace
{

namespace bi = boost::interprocess;

auto const current_process_uuid = juce::Uuid{};

/**
 * @brief Receive a message from the given queue, waiting for 500ms
 *
 * @param queue The queue to receive from.
 * @return std::optional<std::string> The message, or std::nullopt if no message
 * was received.
 */
[[nodiscard]] auto receive_str_message(boost::interprocess::message_queue &queue)
    -> std::optional<std::string>
{
    // TODO this should only need to be 1024, also this should be a single variable you
    // can change in a single place.
    auto const max_message_size = std::size_t{102'400};
    auto buffer = std::vector<char>(max_message_size);
    auto const wait_time = boost::posix_time::microsec_clock::universal_time() +
                           boost::posix_time::milliseconds(500);

    auto received_size = std::size_t{0};
    unsigned int priority;

    if (queue.timed_receive(buffer.data(), max_message_size, received_size, priority,
                            wait_time))
    {
        if (received_size >= 4) // Ensure that at least the size header is present
        {
            auto size = *reinterpret_cast<std::uint32_t *>(buffer.data());
            if (size <= received_size - 4)
            {
                return std::string(buffer.data() + 4, buffer.data() + 4 + size);
            }
            else
            {
                throw std::runtime_error{
                    "Message size is larger than max message length of queue."};
            }
        }
    }
    return std::nullopt;
}

void add_size_prefix(std::string &x)
{
    // Convert the message size to a 4-byte value
    std::uint32_t size = static_cast<std::uint32_t>(x.size());

    // Create a 4-byte buffer to hold the size
    char size_buffer[4];

    // Copy the size into the buffer
    std::memcpy(size_buffer, &size, 4);

    // Prepend the buffer to the original string
    x.insert(0, size_buffer, 4);
}

auto get_reply_queue(juce::Uuid uuid) -> bi::message_queue
{
    return bi::message_queue{bi::open_only,
                             ("ReplyQueue" + uuid.toString().toStdString()).c_str()};
}

} // namespace

namespace xen::message
{

auto serialize(Message const &m) -> std::string
{
    return std::visit(
        sequence::utility::overload{
            [](SessionStart const &x) {
                return nlohmann::json{{"type", "SessionStart"},
                                      {"id", x.id.uuid.toString().toStdString()},
                                      {"display_name", x.id.display_name}}
                    .dump();
            },
            [](SessionEnd const &x) {
                return nlohmann::json{{"type", "SessionEnd"},
                                      {"id", x.id.uuid.toString().toStdString()}}
                    .dump();
            },
            [](SessionIDUpdate const &x) {
                return nlohmann::json{{"type", "SessionIDUpdate"},
                                      {"id", x.id.uuid.toString().toStdString()}}
                    .dump();
            },
            [](GetStateRequest const &x) {
                return nlohmann::json{
                    {"type", "GetStateRequest"},
                    {"request_id", x.request_id.toString().toStdString()},
                    {"reply_id", x.reply_id.toString().toStdString()}}
                    .dump();
            },
            [](GetStateResponse const &x) {
                return nlohmann::json{{"type", "GetStateResponse"},
                                      {"state", ::xen::serialize_state(x.state)}}
                    .dump();
            },
            [](SessionIDResponse const &x) {
                return nlohmann::json{{"type", "SessionIDResponse"},
                                      {"id", x.id.uuid.toString().toStdString()}}
                    .dump();
            }},
        m);
}

auto deserialize(std::string const &x) -> Message
{
    auto const j = nlohmann::json::parse(x);
    auto const type = j.at("type").get<std::string>();
    if (type == "SessionStart")
    {
        return SessionStart{SessionID{juce::Uuid{j.at("id").get<std::string>()},
                                      j.at("display_name").get<std::string>()}};
    }
    else if (type == "SessionEnd")
    {
        return SessionEnd{SessionID{juce::Uuid{j.at("id").get<std::string>()}, ""}};
    }
    else if (type == "SessionIDUpdate")
    {
        return SessionIDUpdate{
            SessionID{juce::Uuid{j.at("id").get<std::string>()}, ""}};
    }
    else if (type == "GetStateRequest")
    {
        return GetStateRequest{juce::Uuid{j.at("request_id").get<std::string>()},
                               juce::Uuid{j.at("reply_id").get<std::string>()}};
    }
    else if (type == "GetStateResponse")
    {
        return GetStateResponse{::xen::deserialize_state(j.at("state").dump())};
    }
    else if (type == "SessionIDResponse")
    {
        return SessionIDResponse{
            SessionID{juce::Uuid{j.at("id").get<std::string>()}, ""}};
    }
    else
    {
        throw std::runtime_error{"Invalid message type: " + type};
    }
}

} // namespace xen::message

namespace xen
{

QueueListener::QueueListener(boost::interprocess::message_queue &queue,
                             std::string const &name)
    : juce::Thread{name + "QueueListener"}, queue_{queue}
{
    if (!this->startThread())
    {
        throw std::runtime_error{"Failed to start QueueListener thread"};
    }
}

void QueueListener::run()
{
    while (!this->threadShouldExit())
    {
        auto const msg_str = receive_str_message(queue_);
        if (msg_str)
        {
            // Emit signal on the main thread.
            message::Message const msg = message::deserialize(*msg_str);
            juce::MessageManager::callAsync([this, msg] { this->on_message(msg); });
        }
    }
}

// ----------------------------------------------------------------------------------------------

ActiveSessions::ActiveSessions()
    : broadcast_queue_{bi::open_or_create, "BroadcastQueue", 100, 1'024},
      current_reply_queue_{
          bi::open_or_create,
          ("ReplyQueue" + current_process_uuid.toString().toStdString()).c_str(), 100,
          1'024}
{
    broadcast_listener_.on_message.connect(
        [this](message::Message const &m) { this->emit_signal(m); });

    current_reply_listener_.on_message.connect(
        [this](message::Message const &m) { this->emit_signal(m); });
}

auto ActiveSessions::get_current_session_uuid() const -> juce::Uuid
{
    return current_process_uuid;
}

void ActiveSessions::broadcast(message::Message const &m)
{
    auto json_str = message::serialize(m);
    add_size_prefix(json_str);
    broadcast_queue_.send(json_str.data(), json_str.size(), 0);
}

void ActiveSessions::send_to(juce::Uuid uuid, message::Message const &m)
{
    auto reply_queue = get_reply_queue(uuid);

    auto json_str = message::serialize(m);
    add_size_prefix(json_str);
    current_reply_queue_.send(json_str.data(), json_str.size(), 0);
}

void ActiveSessions::emit_signal(message::Message const &m)
{
    std::cout << "ActiveSessions::emit_signal: " << message::serialize(m) << std::endl;
    std::visit(sequence::utility::overload{
                   [this](message::SessionStart const &x) {
                       // TODO XenProcessor will connect to this and send_to(x.id, s_id)
                       this->on_session_start(x.id);
                   },
                   [this](message::SessionEnd const &x) { this->on_session_end(x.id); },
                   [this](message::SessionIDUpdate const &x) {
                       this->on_session_id_update(x.id);
                   },
                   [this](message::GetStateRequest const &x) {
                       if (x.request_id == current_process_uuid)
                       {
                           // TODO XenProcessor will handle this by calling send_to
                           this->on_state_request(x.reply_id);
                       }
                   },
                   [this](message::GetStateResponse const &x) {
                       // We do not need uuid of source here.
                       this->on_session_state_response(x.state);
                   },
                   [this](message::SessionIDResponse const &x) {
                       this->on_session_id_update(x.id);
                   }},
               m);
}

} // namespace xen