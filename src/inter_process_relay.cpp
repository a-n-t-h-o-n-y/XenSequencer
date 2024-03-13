#include <xen/inter_process_relay.hpp>

#include <optional>
#include <stdexcept>
#include <string>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>

namespace
{

/**
 * Create a new NNG REQ (request) protocol socket.
 *
 * @details Opens and dials to the given address.
 * @param address Address to connect to.
 * @param timeout Timeout in milliseconds, if zero, no timeout applied.
 * @return nng_socket New socket.
 * @throws std::runtime_error if any errors encountered creating the socket.
 */
[[nodiscard]] auto request_socket(std::string const &address, int timeout = 0)
    -> nng_socket
{
    auto socket = nng_socket{};

    if (auto const result = nng_req0_open(&socket); result != 0)
    {
        throw std::runtime_error{"Failed to open request socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (auto const result = nng_dial(socket, address.c_str(), nullptr, 0); result != 0)
    {
        throw std::runtime_error{"Failed to dial on request socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (timeout > 0)
    {
        auto const result = nng_socket_set_ms(socket, NNG_OPT_SENDTIMEO, timeout);
        if (result != 0)
        {
            throw std::runtime_error{"Failed to set request socket timeout: " +
                                     std::string{nng_strerror(result)}};
        }
    }

    return socket;
}

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

/**
 * Create a new NNG REP (reply) protocol socket.
 *
 * @details Opens and listens on the given address.
 * @param address Address to listen on.
 * @param timeout Timeout in milliseconds, if zero, no timeout applied.
 * @return nng_socket New socket.
 * @throws std::runtime_error if any errors encountered creating the socket.
 */
[[nodiscard]] auto reply_socket(std::string const &address, int timeout = 0)
    -> nng_socket
{
    auto socket = nng_socket{};

    if (auto const result = nng_rep0_open(&socket); result != 0)
    {
        throw std::runtime_error{"Failed to open reply socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (auto const result = nng_listen(socket, address.c_str(), nullptr, 0);
        result != 0)
    {
        throw std::runtime_error{"Failed to listen on reply socket: " +
                                 std::string{nng_strerror(result)}};
    }

    if (timeout > 0)
    {
        auto const result = nng_socket_set_ms(socket, NNG_OPT_RECVTIMEO, timeout);
        if (result != 0)
        {
            throw std::runtime_error{"Failed to set reply socket timeout: " +
                                     std::string{nng_strerror(result)}};
        }
    }

    return socket;
}

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

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

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

ListenSocket::ListenSocket(nng_socket const &socket) : socket_{socket}
{
}

ListenSocket::~ListenSocket()
{
    this->close();
}

auto ListenSocket::listen() const -> std::optional<std::string>
{
    auto const received_message = [this]() -> std::optional<std::string> {
        nng_msg *msg;

        if (auto const result = nng_recvmsg(socket_, &msg, 0); result == NNG_ETIMEDOUT)
        {
            return std::nullopt;
        }
        else if (result == NNG_ECLOSED)
        {
            return std::nullopt;
        }
        else if (result != 0)
        {
            throw std::runtime_error{"Failed to receive message: " +
                                     std::string{nng_strerror(result)} + "\n"};
        }

        auto const data = nng_msg_body(msg);
        auto const len = nng_msg_len(msg);
        auto const message = std::string(static_cast<char *>(data), len);
        nng_msg_free(msg);
        return message;
    }();

    if (received_message.has_value())
    {
        // Auto acknowledge the received message.
        auto const ack_message = std::string{"ack"};

        auto const result = nng_send(socket_, const_cast<char *>(ack_message.c_str()),
                                     ack_message.size(), 0);
        if (result == NNG_ETIMEDOUT)
        {
            throw std::runtime_error{"Failed to send acknowledgment: Timed Out.\n"};
        }
        else if (result == NNG_ECLOSED)
        {
            // TODO maybe you don't want this to throw? only if this is run on separate
            // thread
            throw std::runtime_error{"Failed to send acknowledgment: Socket Closed.\n"};
        }
        else if (result != 0)
        {
            throw std::runtime_error{"Failed to send acknowledgment: " +
                                     std::string{nng_strerror(result)} + "\n"};
        }
    }

    return received_message;
}

auto ListenSocket::close() const -> void
{
    nng_close(socket_);
}

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

SendSocket::SendSocket(nng_socket const &socket) : socket_{socket}
{
}

SendSocket::~SendSocket()
{
    nng_close(socket_);
}

auto SendSocket::send(std::string const &message) const -> bool
{
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
    }

    { // Wait for an acknowledgment
        char *ack_data = nullptr;
        auto ack_size = std::size_t{};

        auto const result = nng_recv(socket_, &ack_data, &ack_size, NNG_FLAG_ALLOC);

        if (result == NNG_ETIMEDOUT)
        {
            return false;
        }
        else if (result != 0)
        {
            throw std::runtime_error{"Failed to receive acknowledgment: " +
                                     std::string{nng_strerror(result)}};
        }

        auto const ack_message = std::string(ack_data, ack_size);
        nng_free(ack_data, ack_size);
        if (ack_message != "ack")
        {
            throw std::runtime_error{"Incorrect 'ack' message received: " +
                                     ack_message};
        }
    }

    return true;
}

/* ~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~ */

InterProcessRelay::InterProcessRelay(juce::Uuid const &this_process_uuid)
    : juce::Thread{"InterProcessRelayListener"},
      reply_socket_{reply_socket(
          build_ipc_endpoint(this_process_uuid.toString().toStdString()), timeout_)}
{
    this->startThread();
}

InterProcessRelay::~InterProcessRelay()
{
    reply_socket_.close();
    this->stopThread(-1);
}

auto InterProcessRelay::run() -> void
{
    while (!this->threadShouldExit())
    {
        if (auto data = reply_socket_.listen(); data.has_value())
        {
            juce::MessageManager::callAsync([this, d = std::move(*data)] {
                try
                {
                    this->on_message.emit(d);
                }
                catch (std::exception const &e)
                {
                    std::cerr << "InterProcessRelay::run() Exception:\n"
                              << e.what() << "With Message:\n"
                              << d << "\nContinuing...\n";
                }
                catch (...)
                {
                    std::cerr << "InterProcessRelay::run() Unknown Exception\n"
                              << "With Message:\n"
                              << d << "Continuing...\n";
                }
            });
        }
    }
}

auto InterProcessRelay::send_to(juce::Uuid const &target_uuid,
                                std::string const &message) const -> void
{
    auto const target_address =
        build_ipc_endpoint(target_uuid.toString().toStdString());
    auto socket = SendSocket{request_socket(target_address, timeout_)};
    if (!socket.send(message))
    {
        throw std::runtime_error{"InterProcessRelay::send_to() Timed Out.\n" + message};
    }
}

} // namespace xen