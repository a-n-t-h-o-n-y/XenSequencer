#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
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

/**
 * @brief Listens for updates to the active sessions list.
 *
 * This list is their UUIDs and display names, not their individual states.
 */
class SessionListener : public juce::Thread
{
  public:
    /**
     * @brief Emitted when the active session list is updated.
     */
    sl::Signal<void()> on_update;

  public:
    /**
     * @brief Launches a new thread to listen for active session list updates.
     *
     * The thread will automatically be started.
     *
     * @throw std::runtime_error If the thread cannot be started.
     */
    SessionListener();

    ~SessionListener() override;

  protected:
    /**
     * @brief Posts a call to on_update() to the main GUI thread if condition variable
     * is notified.
     */
    void run() override;
};

/**
 * @brief Manages the current session's state.
 */
class CurrentSession
{
  public:
    CurrentSession(Metadata const &metadata, State const &state);

    ~CurrentSession();

  public:
    /**
     * @brief Get the UUID for the current process.
     *
     * @return juce::Uuid
     */
    [[nodiscard]] auto get_process_uuid() const -> juce::Uuid;

    /**
     * @brief Get the display name for the current process.
     *
     * @return std::string
     * @throw std::runtime_error If the session ID is not found.
     */
    [[nodiscard]] auto get_display_name() const -> std::string;

    /**
     * @brief Update the display name for the current process in shared memory.
     *
     * The current session's UUID is automatically used in this function. There must
     * already be an entry for the current session in shared memory.
     *
     * @param name The new display name.
     * @throw std::runtime_error If the session ID is not found.
     */
    auto set_display_name(std::string const &name) -> void;

    /**
     * @brief Update the State for the current session in shared memory.
     *
     * The current session's UUID is automatically used in this function. There must
     * already be an entry for the current session in shared memory.
     *
     * @param state The new state.
     * @throw std::runtime_error If the session ID is not found.
     */
    auto set_state(xen::State const &state) -> void;
};

/**
 * @brief Manages all active instances of XenSequencer via shared memory.
 */
class ActiveSessions
{
  public:
    sl::Signal<void(std::vector<SessionID> const &)> on_update;

    CurrentSession current;

  public:
    ActiveSessions(Metadata const &metadata, State const &state);

  public:
    /**
     * @brief Get a list of the active session IDs for all instances of XenSequencer
     * currently running.
     *
     * @return std::vector<SessionID>
     * @throw std::runtime_error If the shared memory is not found or initialized.
     */
    [[nodiscard]] auto get_active_ids() -> std::vector<SessionID>;

    /**
     * @brief Get the shared State for a given session ID.
     *
     * @param uuid The session ID to get the state from.
     * @return xen::State The state of the UUID session.
     * @throw std::runtime_error If the shared memory is not found or initialized.
     * @throw std::runtime_error If the UUID does not reference an active instance.
     */
    [[nodiscard]] auto get_state(juce::Uuid const &uuid) -> xen::State;

  private:
    SessionListener session_listener_;
};

} // namespace xen