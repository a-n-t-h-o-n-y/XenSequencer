#pragma once

#include <string>
#include <vector>

#include <juce_core/juce_core.h>

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

class SharedState
{
  public:
};

/**
 * @brief Get the UUID for the current process.
 *
 * @return juce::Uuid The UUID for the current process.
 */
[[nodiscard]] auto get_current_process_uuid() -> juce::Uuid;

/**
 * @brief Get a list of the active session IDs for all instances of XenSequencer
 * currently running.
 *
 * @return std::vector<SessionID>
 */
[[nodiscard]] auto get_active_ids() -> std::vector<SessionID>;

/**
 * @brief Get the shared memory State for a given session ID.
 *
 * This uses a hardcoded shared memory ID string to find the shared memory.
 *
 * @param uuid The session ID to get the state from.
 * @return xen::State The state of the session.
 * @throw std::runtime_error If the session ID is not found.
 * @throw std::runtime_error If the UUID is not part of the shared memory.
 */
[[nodiscard]] auto get_shared_state(juce::Uuid const &uuid) -> xen::State;

/**
 * @brief Update the State for the current session in shared memory.
 *
 * The current session's UUID is automatically used in this function. There must already
 * be an entry for the current session in shared memory.
 *
 * @param state The new state.
 * @throw std::runtime_error If the session ID is not found.
 */
auto update_shared_state(xen::State const &state) -> void;

/**
 * @brief Update the display name for the current process in shared memory.
 *
 * @param name The new display name.
 */
auto update_display_name(std::string const &name) -> void;

} // namespace xen