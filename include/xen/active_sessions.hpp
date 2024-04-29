#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <nng/nng.h>
#include <signals_light/signal.hpp>

#include <sequence/utility.hpp>

#include <xen/instance_directory.hpp>
#include <xen/inter_process_relay.hpp>
#include <xen/state.hpp>

namespace xen
{

/*.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.*/
// Messages

/**
 * Sent by an instance when it shuts down.
 */
struct InstanceShutdown
{
    juce::Uuid uuid;
};

/**
 * Sent by an instance when it starts up or changes its display name.
 */
struct IDUpdate
{
    juce::Uuid uuid;
    std::string display_name;
};

/**
 * Sent by an instance to request the current state of receivier's the timeline.
 */
struct StateRequest
{
    juce::Uuid reply_to;
};

/**
 * Sent by an instance in response to a StateRequest.
 */
struct StateResponse
{
    xen::SequencerState state;
};

/**
 * Sent by an instance to request the display name of another instance.
 */
struct DisplayNameRequest
{
    juce::Uuid reply_to;
};

using Message = std::variant<InstanceShutdown, IDUpdate, StateRequest, StateResponse,
                             DisplayNameRequest>;

/**
 * Serialize a message to a JSON string.
 */
[[nodiscard]] auto serialize(Message const &m) -> std::string;

/**
 * Deserialize a JSON string to a message.
 */
[[nodiscard]] auto deserialize(std::string const &x) -> Message;

/*.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.*/

/**
 * Sends a heartbeat to the instance directory at a regular interval via a timer.
 */
class HeartbeatSender : private juce::Timer
{
  public:
    static constexpr int PERIOD = 15'000; // ms

  public:
    HeartbeatSender(InstanceDirectory &directory, juce::Uuid const &uuid)
        : directory_{directory}, uuid_{uuid}
    {
        this->startTimer(PERIOD);
    }

    ~HeartbeatSender() override
    {
        this->stopTimer();
    }

  private:
    void timerCallback() override
    {
        try
        {
            directory_.send_heartbeat(uuid_);
        }
        catch (std::exception const &e)
        {
            std::cerr << "Exception in HeartbeatSender:\n" << e.what() << '\n';
        }
        catch (...)
        {
            std::cerr << "Unknown exception in HeartbeatSender\n";
        }
    }

  private:
    InstanceDirectory &directory_;
    juce::Uuid uuid_;
};

/*.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.*/

/**
 * Trim dead sessions from the instance directory with a timer.
 */
class DeadSessionTrimmer : private juce::Timer
{
  public:
    static constexpr int PERIOD = 30'000; // ms

  public:
    explicit DeadSessionTrimmer(InstanceDirectory &directory) : directory_{directory}
    {
        this->timerCallback();
        this->startTimer(PERIOD);
    }

    ~DeadSessionTrimmer() override
    {
        this->stopTimer();
    }

  private:
    void timerCallback() override
    {
        try
        {
            directory_.unregister_dead_instances(
                std::chrono::milliseconds{HeartbeatSender::PERIOD} * 4);
        }
        catch (std::exception const &e)
        {
            std::cerr << "Exception in DeadSessionTimer:\n" << e.what() << '\n';
        }
        catch (...)
        {
            std::cerr << "Unknown exception in DeadSessionTimer\n";
        }
    }

  private:
    InstanceDirectory &directory_;
};

/*.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.*/

/**
 * RAII style to handle registration and unregistration of this instance with the
 * instance directory. Also sends initialization and shutdown messages to other
 * instances and ownes the HeartbeatSender.
 */
class ThisInstance
{
  public:
    ThisInstance(InterProcessRelay &relay, InstanceDirectory &directory,
                 juce::Uuid const &uuid, std::string const &display_name)
        : relay_{relay}, directory_{directory}, uuid_{uuid},
          heartbeat_sender_{directory, uuid}
    {
        // Get list of other instances and add itself to instance directory in a single
        // 'atomic' step.
        auto const others = [&] {
            auto const lock = std::scoped_lock{directory_.get_mutex()};
            auto const x = directory_.get_active_instances();
            directory_.register_instance(uuid_);
            return x;
        }();

        for (auto const &other : others)
        {
            try
            {
                relay_.send_to(
                    other,
                    serialize(IDUpdate{.uuid = uuid_, .display_name = display_name}));
            }
            catch (std::exception const &e)
            {
                // TODO logging
                std::cerr << "Could not send initialization message to other instance ("
                          << other.toString().toStdString() << "):\n"
                          << e.what() << '\n'
                          << "skipping...\n";
            }
            catch (...)
            {
                std::cerr << "Unknown exception in ThisInstance\n";
            }
        }
    }

    ~ThisInstance()
    {
        try
        {
            auto const others = [&] {
                auto const lock = std::scoped_lock{directory_.get_mutex()};
                directory_.unregister_instance(uuid_);
                return directory_.get_active_instances();
            }();

            for (auto const &other : directory_.get_active_instances())
            {
                relay_.send_to(other, serialize(InstanceShutdown{.uuid = uuid_}));
            }
        }
        catch (std::exception const &e)
        {
            std::cerr << "Exception in ~ThisInstance:\n" << e.what() << '\n';
        }
        catch (...)
        {
            std::cerr << "Unknown exception in ~ThisInstance\n";
        }
    }

  public:
    [[nodiscard]] auto get_uuid() const -> juce::Uuid const &
    {
        return uuid_;
    }

  private:
    InterProcessRelay &relay_;
    InstanceDirectory &directory_;
    juce::Uuid const &uuid_;
    HeartbeatSender heartbeat_sender_;
};

/*.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.=.~.*/

/**
 * Class for managing active sessions across processes.
 *
 * This class owns the InterProcessRelay, InstanceDirectory, ThisInstance,
 * and DeadSessionTrimmer. It translates messages from the relay into signals
 * and emits them. It also provides a public interface for sending messages to
 * other instances.
 */
class ActiveSessions
{
  public:
    sl::Signal<void(juce::Uuid const &)> on_instance_shutdown;
    sl::Signal<void(juce::Uuid const &, std::string const &)> on_id_update;
    sl::Signal<SequencerState()> on_state_request;
    sl::Signal<void(SequencerState const &)> on_state_response;
    sl::Signal<std::string()> on_display_name_request;

  public:
    explicit ActiveSessions(juce::Uuid const &current_process_id,
                            std::string const &display_name)
        : relay_{current_process_id}, instance_directory_{},
          this_instance_{relay_, instance_directory_, current_process_id, display_name},
          dead_session_trimmer_{instance_directory_}
    {
        relay_.on_message.connect([this](std::string const &json) {
            std::visit(
                sequence::utility::overload{
                    [this](InstanceShutdown const &x) { on_instance_shutdown(x.uuid); },
                    [this](IDUpdate const &x) { on_id_update(x.uuid, x.display_name); },
                    [this](StateRequest const &x) {
                        auto state = on_state_request();
                        if (!state.has_value())
                        {
                            throw std::logic_error{"on_state_request() returned "
                                                   "std::nullopt, no Slot connected"};
                        }
                        relay_.send_to(x.reply_to, serialize(StateResponse{
                                                       .state = std::move(*state)}));
                    },
                    [this](StateResponse const &x) { on_state_response(x.state); },
                    [this](DisplayNameRequest const &x) {
                        auto name = on_display_name_request();
                        if (!name.has_value())
                        {
                            throw std::logic_error{"on_display_name_request() returned "
                                                   "std::nullopt, no Slot connected"};
                        }
                        relay_.send_to(
                            x.reply_to,
                            serialize(IDUpdate{.uuid = this_instance_.get_uuid(),
                                               .display_name = std::move(*name)}));
                    },
                },
                deserialize(json));
        });
    }

  public:
    /**
     * Puts in a request to each instance for its display name.
     *
     * This does not block until the request is fulfilled. Instead, the
     * on_id_update signal will be emitted when the response is received.
     *
     * This should be called in the XenEditor constructor after connecting to
     * on_id_update.
     *
     * @throws std::runtime_error if any errors encountered.
     */
    auto request_other_session_ids() const -> void
    {
        auto const instances = instance_directory_.get_active_instances();

        for (auto const &instance : instances)
        {
            if (instance != this_instance_.get_uuid())
            {
                try
                {
                    relay_.send_to(instance,
                                   serialize(DisplayNameRequest{
                                       .reply_to = this_instance_.get_uuid()}));
                }
                catch (std::exception const &e)
                {
                    // TODO change this to use Logging with timestamp.
                    std::cerr
                        << "Could not send display name request to other instance ("
                        << instance.toString().toStdString() << "):\n"
                        << e.what() << '\n'
                        << "skipping...\n";
                }
                catch (...)
                {
                    std::cerr << "Unknown exception in request_other_session_ids\n";
                }
            }
        }
    }

    /**
     * Puts in a request to the given instance for its current state.
     *
     * This does not block until the request is fulfilled. Instead, the
     * on_state_response signal will be emitted when the response is received.
     *
     * @param uuid UUID of the instance to request the state from.
     * @throws std::runtime_error if the given UUID is not registered.
     */
    auto request_state(juce::Uuid const &uuid) const -> void
    {
        relay_.send_to(uuid,
                       serialize(StateRequest{.reply_to = this_instance_.get_uuid()}));
    }

    /**
     * Sends an IDUpdate message to all other instances.
     *
     * @param name New display name.
     * @throws std::runtime_error if any errors encountered.
     */
    auto notify_display_name_update(std::string const &name) -> void
    {
        auto const instances = instance_directory_.get_active_instances();
        for (auto const &instance : instances)
        {
            if (instance != this_instance_.get_uuid())
            {
                relay_.send_to(instance,
                               serialize(IDUpdate{.uuid = this_instance_.get_uuid(),
                                                  .display_name = name}));
            }
        }
    }

  private:
    InterProcessRelay relay_;
    InstanceDirectory instance_directory_;
    ThisInstance this_instance_;
    DeadSessionTrimmer dead_session_trimmer_;
};

} // namespace xen