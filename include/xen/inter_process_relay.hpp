#pragma once

#include <optional>
#include <string>

#include <juce_core/juce_core.h>
#include <nng/nng.h>
#include <signals_light/signal.hpp>

namespace xen
{

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

    /**
     * Close the socket.
     *
     * This will cause calls to listen() to return immediately with std::nullopt.
     */
    auto close() const -> void;

  private:
    nng_socket socket_;
};

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

class SendSocket
{
  public:
    /**
     * Build a SendSocket to take ownership of the given socket.
     *
     * @param socket Socket to send over.
     */
    explicit SendSocket(nng_socket const &socket);

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

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

/**
 * @class InterProcessRelay
 *
 * Conduit for sending messages between instances. This launches a listener thread and
 * receives messages via the on_message signal emitted by the main juce thread.
 */
class InterProcessRelay : private juce::Thread
{
  public:
    sl::Signal<void(std::string const &)> on_message;

  public:
    explicit InterProcessRelay(juce::Uuid const &this_process_uuid);

    ~InterProcessRelay() override;

  public:
    /**
     * @brief Send a message to a specific instance.
     *
     * This does not wait for a response, any response will come in via the
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
    ListenSocket reply_socket_;
    static constexpr int timeout_ = 5000;
};

} // namespace xen