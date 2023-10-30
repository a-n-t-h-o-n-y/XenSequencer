#include <xen/shared_state.hpp>

#include <algorithm>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <juce_core/juce_core.h>

#include <xen/serialize.hpp>
#include <xen/state.hpp>

namespace
{

namespace bi = boost::interprocess;

using char_alloc_t = bi::allocator<char, bi::managed_shared_memory::segment_manager>;
using string_t = bi::basic_string<char, std::char_traits<char>, char_alloc_t>;

struct SharedPayload
{
    string_t display_name;
    string_t json_state;
};

using key_t = juce::Uuid;
using value_t = SharedPayload;
using map_alloc_t = bi::allocator<std::pair<const key_t, value_t>,
                                  bi::managed_shared_memory::segment_manager>;
using shared_map_t = bi::map<key_t, value_t, std::less<key_t>, map_alloc_t>;

// TODO this is the beginnings of the class.
struct SharedStateHandle
{
    static constexpr char const *ACTIVE_SESSIONS_SEGMENT_ID = "ActiveSessionsSegment";
    static constexpr char const *ACTIVE_SESSIONS_MAP_ID = "ActiveSessionsMap";
    bi::named_mutex mutex{bi::open_or_create, "ActiveSessionsMutex"};
    bi::named_condition condition_var{bi::open_or_create, "ActiveSessionsConditionVar"};
};

// TODO eventually remove once class exists
auto shared_state_handle = SharedStateHandle{};

auto current_session_uuid = juce::Uuid{};

/**
 * @brief Get the shared memory segment for active sessions.
 *
 * @return bi::managed_shared_memory The shared memory segment.
 * @throw std::runtime_error If the shared memory segment does not exist.
 */
[[nodiscard]] auto get_shared_segment() -> bi::managed_shared_memory
{
    try
    {
        return bi::managed_shared_memory{bi::open_only,
                                         SharedStateHandle::ACTIVE_SESSIONS_SEGMENT_ID};
    }
    catch (bi::interprocess_exception const &e)
    {
        throw std::runtime_error("Interprocess Error: " + std::string{e.what()});
    }
}

/**
 * @brief Get the shared memory map for active sessions.
 *
 * @param segment The shared memory segment to get the map from.
 * @return shared_map_t& The shared memory map.
 * @throw std::runtime_error If the shared memory map does not exist.
 */
auto get_active_sessions_map(bi::managed_shared_memory &segment) -> shared_map_t &
{
    try
    {
        auto const map =
            segment.find<shared_map_t>(SharedStateHandle::ACTIVE_SESSIONS_MAP_ID).first;
        return map ? *map
                   : throw std::runtime_error(
                         "Shared Memory Object Not Found: " +
                         std::string{SharedStateHandle::ACTIVE_SESSIONS_MAP_ID});
    }
    catch (bi::interprocess_exception const &e)
    {
        throw std::runtime_error("Interprocess Error: " + std::string{e.what()});
    }
}

} // namespace

namespace xen
{

auto get_current_process_uuid() -> juce::Uuid
{
    return current_session_uuid;
}

auto get_active_ids() -> std::vector<SessionID>
{
    auto segment = get_shared_segment();
    auto &map = get_active_sessions_map(segment);
    auto lock = bi::scoped_lock{shared_state_handle.mutex};

    auto keys = std::vector<SessionID>{};
    keys.reserve(map.size());
    std::transform(
        std::begin(map), std::end(map), std::back_inserter(keys), [](auto const &pair) {
            return SessionID{pair.first, std::string{pair.second.display_name.c_str()}};
        });
    return keys;
}

auto get_shared_state(juce::Uuid const &uuid) -> xen::State
{
    auto segment = get_shared_segment();
    auto &map = get_active_sessions_map(segment);
    auto lock = bi::scoped_lock{shared_state_handle.mutex};

    // Locate the session with the given UUID
    if (auto it = map.find(uuid); it != std::cend(map))
    {
        return deserialize_state(it->second.json_state.c_str());
    }

    throw std::runtime_error("Session not found.");
}

auto update_shared_state(xen::State const &state) -> void
{
    auto segment = get_shared_segment();
    auto &map = get_active_sessions_map(segment);
    auto lock = bi::scoped_lock{shared_state_handle.mutex};

    // Locate the session with the given UUID
    if (auto it = map.find(current_session_uuid); it != std::end(map))
    {
        it->second.json_state = serialize_state(state).c_str();
        shared_state_handle.condition_var.notify_all();
        return;
    }
    throw std::runtime_error("Shared state is uninitialized for the current process.");
}

auto update_display_name(std::string const &name) -> void
{
    auto segment = get_shared_segment();
    auto &map = get_active_sessions_map(segment);
    auto lock = bi::scoped_lock{shared_state_handle.mutex};

    if (auto it = map.find(current_session_uuid); it != std::end(map))
    {
        it->second.display_name = name.c_str();
        shared_state_handle.condition_var.notify_all();
        return;
    }
    throw std::runtime_error("Shared state is uninitialized for the current process.");
}

} // namespace xen