#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <juce_core/juce_core.h>
#include <nng/nng.h>
#include <signals_light/signal.hpp>

#include <xen/state.hpp>

namespace xen
{

/**
 * @brief Create a new NNG PULL protocol socket.
 *
 * Opens and listens on the given address.
 *
 * @param address Address to listen on.
 * @param timeout Timeout in milliseconds, if zero, no timeout applied.
 * @return nng_socket New socket.
 * @throws std::runtime_error if any errors encountered creating the socket.
 */
[[nodiscard]] auto pull_socket(std::string const &address, int timeout = 0)
    -> nng_socket;

/**
 * @brief Create a new NNG PUSH protocol socket.
 *
 * Opens and dials to the given address.
 *
 * @param address Address to connect to.
 * @param timeout Timeout in milliseconds, if zero, no timeout applied.
 * @return nng_socket New socket.
 * @throws std::runtime_error if any errors encountered creating the socket.
 */
[[nodiscard]] auto push_socket(std::string const &address, int timeout = 0)
    -> nng_socket;

/**
 * @brief Create a new NNG BUS protocol socket for receiving.
 *
 * Opens and listens on the given address.
 *
 * @param address Address to listen on.
 * @param timeout Timeout in milliseconds, if zero, no timeout applied.
 *
 * @return nng_socket New socket.
 * @throws std::runtime_error if any errors encountered creating the socket.
 */
[[nodiscard]] auto bus_receive_socket(std::string const &address, int timeout = 0)
    -> nng_socket;

/**
 * @brief Create a new NNG BUS protocol socket for sending.
 *
 * Opens and dials to the given address.
 *
 * @param address Address to connect to.
 * @param timeout Timeout in milliseconds, if zero, no timeout applied.
 * @return nng_socket New socket.
 * @throws std::runtime_error if any errors encountered creating the socket.
 */
[[nodiscard]] auto bus_send_socket(std::string const &address, int timeout = 0)
    -> nng_socket;

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

/**
 * @class ListenSocket
 *
 * @brief Resource-managing class for NNG socket that listens.
 */
class ListenSocket
{
  public:
    /**
     * Build a ListenSocket to take ownership of the given socket.
     *
     * @param socket Socket to listen on.
     */
    explicit ListenSocket(nng_socket const &socket);

    ~ListenSocket();

  public:
    /**
     * Receive a message from the socket.
     *
     * Blocking depends on settings of passed in socket.
     *
     * @return Received message as std::string or std::nullopt if timeout occured.
     * @throw std::runtime_error if message receiving fails.
     */
    [[nodiscard]] auto listen() const -> std::optional<std::string>;

  private:
    nng_socket socket_;
};

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

class SendSocket
{
  public:
    /**
     * Build a SendSocket to take ownership of the given socket.
     *
     * @param socket Socket to send over.
     */
    explicit SendSocket(nng_socket socket);

    ~SendSocket();

  public:
    /**
     * Send a string message over the socket.
     *
     * Blocking depends on settings of passed in socket.
     *
     * @param message Message to send.
     * @return True if the message was sent, false if timeout occurred.
     * @throws std::runtime_error if the message sending fails.
     */
    auto send(std::string const &message) const -> bool;

  private:
    nng_socket socket_;
};

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

/**
 * @class Mailbox
 *
 * Mailbox class for IPC using a Pull Socket and a Push Socket.
 */
class Mailbox : private juce::Thread
{
  public:
    sl::Signal<void(std::string const &)> on_message;

  public:
    explicit Mailbox(juce::Uuid const &uuid);

    ~Mailbox() override;

  public:
    /**
     * @brief Send a message to a specific instance.
     *
     * This is async and returns immediately. Any response will come in via the
     * on_message signal.
     *
     * @param target_uuid UUID of the instance to send to.
     * @param message Message to send.
     */
    auto send_to(juce::Uuid const &target_uuid, std::string const &message) const
        -> void;

  protected:
    auto run() -> void override;

  private:
    ListenSocket pull_socket_;
    static constexpr int timeout_ = 500;
};

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

/**
 * @class Broadcast
 *
 * Broadcast class for IPC using a Bus Send Socket and a Bus Receive Socket.
 */
class Broadcast : private juce::Thread
{
  public:
    sl::Signal<void(std::string const &)> on_message;

  public:
    /**
     * Build a Broadcast object that can send and receive messages.
     */
    Broadcast();

    ~Broadcast() override;

  public:
    /**
     * Broadcast a message to all other Broadcast instances across processes.
     *
     * @param message Message to send.
     * @throw std::runtime_error if the message sending fails.
     */
    auto send(std::string const &message) const -> void;

  protected:
    /**
     * Thread's run loop.
     */
    auto run() -> void override;

  private:
    static constexpr int timeout_ = 500;

    // Notice: Member initialization order matters here.
    ListenSocket bus_receive_socket_;
    SendSocket bus_send_socket_;
};

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

/**
 * @brief Identifying information for a XenSequencer instance.
 */
struct SessionID
{
    juce::Uuid uuid;
    std::string display_name;
};

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

struct SessionStartMessage
{
    SessionID id;
};

struct SessionEndMessage
{
    SessionID id;
};

struct IDUpdateMessage
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

using Message = std::variant<SessionStartMessage, SessionEndMessage, IDUpdateMessage,
                             GetStateRequest, GetStateResponse>;

[[nodiscard]] auto serialize(Message const &m) -> std::string;

[[nodiscard]] auto deserialize(std::string const &x) -> Message;

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

/**
 * @class XenIPC
 * @brief Class for IPC between XenSequencer instances dealing in Messages
 */
class XenIPC
{
  public:
    sl::Signal<void(Message const &)> on_receive;

  public:
    XenIPC(juce::Uuid const &current_process_id);

  public:
    /**
     * @brief Send a message to a specific instance.
     *
     * @param uuid UUID of the instance to send to.
     * @param m Message to send.
     * @throw std::runtime_error if the message sending fails.
     */
    auto send_to(juce::Uuid const &uuid, Message const &m) const -> void;

    /**
     * @brief Broadcast a message to all other XenSequencer instances.
     *
     * @param m Message to send.
     * @throw std::runtime_error if the message sending fails.
     */
    auto broadcast(Message const &m) const -> void;

  private:
    Mailbox mailbox_;
    Broadcast broadcast_;
};

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

class ActiveSessions
{
  public:
    explicit ActiveSessions(juce::Uuid const &current_process_id)
        : ipc_{current_process_id}
    {
        // TODO connect to on_receive and visit the message object
        // to emit other signals and use their return values to send a response if
        // needed

        // TODO connect to these signals in XenProcessor constructor to update GUI
        // not sure how to push updates to the gui from the processor... there isnt
        // 'one' gui so you cant just ask for a reference.
    }

  private:
    XenIPC ipc_;
};

// - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - //

} // namespace xen

// namespace xen
// {

// // TODO emit SessionStart on XenProcessor constructor manually? and others?

// /**
//  * @brief Creates a new thread and listens for messages on the given queue.
//  */
// class QueueListener : public juce::Thread
// {
//   public:
//     /**
//      * @brief Emitted when a message is received.
//      */
//     sl::Signal<void(message::Message const &)> on_message;

//   public:
//     explicit QueueListener(boost::interprocess::message_queue &queue,
//                            std::string const &name);

//   public:
//     void run() override;

//   private:
//     boost::interprocess::message_queue &queue_;
// };

// // The instance of this class is owned by XenProcessor
// class ActiveSessions
// {
//   public:
//     // These are all emitted by the Listeners
//     // These will be connected to in XenProcessor to various GUI elements
//     sl::Signal<void(SessionID const &)> on_session_start;
//     sl::Signal<void(SessionID const &)> on_session_end;
//     sl::Signal<void(SessionID const &)> on_session_id_update; // create or replace
//     sl::Signal<void(juce::Uuid const &)> on_state_request;
//     sl::Signal<void(State const &)> on_session_state_response;

//   public:
//     ActiveSessions();

//   public:
//     [[nodiscard]] auto get_current_session_uuid() const -> juce::Uuid;

//     /// Send message to all instances
//     void broadcast(message::Message const &m);

//     /// Send message to specific instance.
//     void send_to(juce::Uuid uuid, message::Message const &m);

//   private:
//     void emit_signal(message::Message const &m);

//   private:
//     boost::interprocess::message_queue broadcast_queue_;
//     boost::interprocess::message_queue current_reply_queue_;

//     QueueListener broadcast_listener_{broadcast_queue_, "Broadcast"};
//     QueueListener current_reply_listener_{current_reply_queue_, "CurrentReply"};
// };

// } // namespace xen