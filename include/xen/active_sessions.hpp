#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <juce_core/juce_core.h>
#include <signals_light/signal.hpp>

#include <xen/state.hpp>

namespace xen
{

/**
 * @brief Identifying information for a XenSequencer instance.
 */
struct SessionID
{
    juce::Uuid uuid;
    std::string display_name;
};

namespace message
{

// TODO emit SessionStart on XenProcessor constructor manually? and others?

struct SessionStart
{
    SessionID id;
};

struct SessionEnd
{
    SessionID id;
};

struct SessionIDUpdate
{
    SessionID id;
};

struct GetStateRequest
{
    juce::Uuid request_id;
    juce::Uuid reply_id;
};

struct GetStateResponse
{
    xen::State state;
};

struct SessionIDResponse
{
    SessionID id;
};

using Message = std::variant<SessionStart, SessionEnd, SessionIDUpdate, GetStateRequest,
                             GetStateResponse, SessionIDResponse>;

[[nodiscard]] auto serialize(Message const &m) -> std::string;

[[nodiscard]] auto deserialize(std::string const &x) -> Message;

} // namespace message

/**
 * @brief Creates a new thread and listens for messages on the given queue.
 */
class QueueListener : public juce::Thread
{
  public:
    /**
     * @brief Emitted when a message is received.
     */
    sl::Signal<void(message::Message const &)> on_message;

  public:
    explicit QueueListener(boost::interprocess::message_queue &queue,
                           std::string const &name);

  public:
    void run() override;

  private:
    boost::interprocess::message_queue &queue_;
};

// The instance of this class is owned by XenProcessor
class ActiveSessions
{
  public:
    // These are all emitted by the Listeners
    // These will be connected to in XenProcessor to various GUI elements
    sl::Signal<void(SessionID const &)> on_session_start;
    sl::Signal<void(SessionID const &)> on_session_end;
    sl::Signal<void(SessionID const &)> on_session_id_update; // create or replace
    sl::Signal<void(juce::Uuid const &)> on_state_request;
    sl::Signal<void(State const &)> on_session_state_response;

  public:
    ActiveSessions();

  public:
    [[nodiscard]] auto get_current_session_uuid() const -> juce::Uuid;

    /// Send message to all instances
    void broadcast(message::Message const &m);

    /// Send message to specific instance.
    void send_to(juce::Uuid uuid, message::Message const &m);

  private:
    void emit_signal(message::Message const &m);

  private:
    boost::interprocess::message_queue broadcast_queue_;
    boost::interprocess::message_queue current_reply_queue_;

    QueueListener broadcast_listener_{broadcast_queue_, "Broadcast"};
    QueueListener current_reply_listener_{current_reply_queue_, "CurrentReply"};
};

} // namespace xen