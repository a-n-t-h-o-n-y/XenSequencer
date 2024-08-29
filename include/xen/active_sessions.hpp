#pragma once

#include <cstddef>
#include <string>
#include <variant>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <signals_light/signal.hpp>

#include <sequence/measure.hpp>

#include <xen/instance_directory.hpp>
#include <xen/inter_process_relay.hpp>

namespace xen
{

// Messages
// -------------------------------------------------------------------------------------

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
 * Sent by an instance to request a specific Measure of receivier's the timeline.
 */
struct MeasureRequest
{
    std::size_t measure_index;
    juce::Uuid reply_to;
};

/**
 * Sent by an instance in response to a MeasureRequest.
 */
struct MeasureResponse
{
    sequence::Measure measure;
};

/**
 * Sent by an instance to request the display name of another instance.
 */
struct DisplayNameRequest
{
    juce::Uuid reply_to;
};

using Message = std::variant<InstanceShutdown, IDUpdate, MeasureRequest,
                             MeasureResponse, DisplayNameRequest>;

/**
 * Serialize a message to a JSON string.
 */
[[nodiscard]] auto serialize(Message const &m) -> std::string;

/**
 * Deserialize a JSON string to a message.
 */
[[nodiscard]] auto deserialize(std::string const &x) -> Message;

// -------------------------------------------------------------------------------------

/**
 * Sends a heartbeat to the instance directory at a regular interval via a timer.
 */
class HeartbeatSender : private juce::Timer
{
  public:
    static constexpr int PERIOD = 15'000; // ms

  public:
    HeartbeatSender(InstanceDirectory &directory, juce::Uuid const &uuid);

    ~HeartbeatSender() override;

  private:
    void timerCallback() override;

  private:
    InstanceDirectory &directory_;
    juce::Uuid uuid_;
};

// -------------------------------------------------------------------------------------

/**
 * Trim dead sessions from the instance directory with a timer.
 */
class DeadSessionTrimmer : private juce::Timer
{
  public:
    static constexpr int PERIOD = 30'000; // ms

  public:
    explicit DeadSessionTrimmer(InstanceDirectory &directory);

    ~DeadSessionTrimmer() override;

  private:
    void timerCallback() override;

  private:
    InstanceDirectory &directory_;
};

// -------------------------------------------------------------------------------------

/**
 * RAII style to handle registration and unregistration of this instance with the
 * instance directory. Also sends initialization and shutdown messages to other
 * instances and ownes the HeartbeatSender.
 */
class ThisInstance
{
  public:
    ThisInstance(InterProcessRelay &relay, InstanceDirectory &directory,
                 juce::Uuid const &uuid, std::string const &display_name);

    ~ThisInstance();

  public:
    [[nodiscard]] auto get_uuid() const -> juce::Uuid const &;

  private:
    InterProcessRelay &relay_;
    InstanceDirectory &directory_;
    juce::Uuid const &uuid_;
    HeartbeatSender heartbeat_sender_;
};

// -------------------------------------------------------------------------------------

/**
 * Class for managing active sessions across processes.
 *
 * @details This class owns the InterProcessRelay, InstanceDirectory, ThisInstance, and
 * DeadSessionTrimmer. It translates messages from the relay into signals and emits
 * them. It also provides a public interface for sending messages to other instances.
 */
class ActiveSessions
{
  public:
    sl::Signal<void(juce::Uuid const &)> on_instance_shutdown;
    sl::Signal<void(juce::Uuid const &, std::string const &)> on_id_update;
    sl::Signal<sequence::Measure(std::size_t)> on_measure_request;
    sl::Signal<void(sequence::Measure const &)> on_measure_response;
    sl::Signal<std::string()> on_display_name_request;

  public:
    ActiveSessions(juce::Uuid const &current_process_id,
                   std::string const &display_name);

  public:
    /**
     * Puts in a request to each instance for its display name.
     *
     * @details This does not block until the request is fulfilled. Instead, the
     * on_id_update signal will be emitted when the response is received.
     * This should be called in the XenEditor constructor after connecting to
     * on_id_update.
     * @throws std::runtime_error if any errors encountered.
     */
    void request_other_session_ids() const;

    /**
     * Puts in a request to the given instance for a specific Measure.
     *
     * @details This does not block until the request is fulfilled. Instead, the
     * on_measure_response signal will be emitted when the response is received.
     * @param uuid UUID of the instance to request the state from.
     * @param index Index of the Measure to request from the sequence bank.
     * @throws std::runtime_error if the given UUID is not registered.
     */
    void request_measure(juce::Uuid const &uuid, std::size_t index) const;

    /**
     * Sends an IDUpdate message to all other instances.
     *
     * @param name New display name.
     * @throws std::runtime_error if any errors encountered.
     */
    void notify_display_name_update(std::string const &name);

  private:
    InterProcessRelay relay_;
    InstanceDirectory instance_directory_;
    ThisInstance this_instance_;
    DeadSessionTrimmer dead_session_trimmer_;
};

} // namespace xen