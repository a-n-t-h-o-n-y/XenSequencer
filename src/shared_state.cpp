#include <xen/shared_state.hpp>

#include <functional>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <juce_core/juce_core.h>

#include <xen/serialize_state.hpp>
#include <xen/state.hpp>

namespace
{

namespace bi = boost::interprocess;

using segment_alloc_t = bi::allocator<void, bi::managed_shared_memory::segment_manager>;
using char_alloc_t = bi::allocator<char, bi::managed_shared_memory::segment_manager>;
using string_t = bi::basic_string<char, std::char_traits<char>, char_alloc_t>;

struct SharedPayload
{
    string_t display_name;
    string_t json_state;
};

using key_t = juce::Uuid;
using value_t = SharedPayload;
using map_alloc_t = bi::allocator<std::pair<key_t, value_t>,
                                  bi::managed_shared_memory::segment_manager>;
using shared_map_t = bi::map<key_t, value_t, std::less<key_t>, map_alloc_t>;

struct SharedStateHandle
{
    static constexpr char const *ACTIVE_SESSIONS_ID = "ActiveSessions";
    bi::named_mutex mutex{bi::open_or_create, "ActiveSessionsMutex"};
    bi::named_condition condition_var{bi::open_or_create, "ActiveSessionsConditionVar"};
};

auto shared_state_handle = SharedStateHandle{};

auto current_session_uuid = juce::Uuid{};

} // namespace

namespace xen
{

auto get_current_process_uuid() -> juce::Uuid
{
    return current_session_uuid;
}

auto get_shared_state(juce::Uuid uuid) -> xen::State
{
    try
    {
        // Open the existing shared memory object.
        auto segment = bi::managed_shared_memory{bi::open_only,
                                                 SharedStateHandle::ACTIVE_SESSIONS_ID};

        // Locate the map
        auto const map =
            segment.find<shared_map_t>(SharedStateHandle::ACTIVE_SESSIONS_ID).first;
        if (!map)
        {
            throw std::runtime_error("Shared memory map not found.");
        }

        // Lock the mutex for safe access
        auto lock = bi::scoped_lock{shared_state_handle.mutex};

        // Locate the session with the given UUID
        if (auto it = map->find(uuid); it != map->end())
        {
            return deserialize(it->second.json_state.c_str());
        }

        throw std::runtime_error("Session not found.");
    }
    catch (bi::interprocess_exception const &)
    {
        throw std::runtime_error("Shared memory segment does not exist");
    }
}

} // namespace xen