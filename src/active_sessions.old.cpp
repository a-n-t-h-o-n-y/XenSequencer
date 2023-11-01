#include <xen/active_sessions.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <xen/serialize.hpp>
#include <xen/state.hpp>

namespace
{

namespace bi = boost::interprocess;

auto current_session_uuid = juce::Uuid{};

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

/**
 * @brief Manages the shared memory segment for active sessions. Singleton
 */
class ActiveSessionsSharedMemory
{
  public:
    boost::interprocess::named_mutex mutex;
    boost::interprocess::named_condition condition_var;
    boost::interprocess::managed_shared_memory segment;
    shared_map_t &active_sessions_map;

  public:
    /**
     * @brief Construct a new Active Sessions Shared Memory object.
     *
     * @param segment_size The size of the shared memory segment in bytes.
     * @throw std::runtime_error If the shared memory segment cannot be created.
     */
    ActiveSessionsSharedMemory(std::size_t segment_size = 65'536)
        : mutex{bi::open_or_create, "ActiveSessionsMutex"},
          condition_var{bi::open_or_create, "ActiveSessionsConditionVar"},
          segment{bi::managed_shared_memory{bi::open_or_create, "ActiveSessionsSegment",
                                            segment_size}},
          active_sessions_map{this->create_or_find_map_handle("ActiveSessionsMap")}
    {
    }

    /**
     * @brief Destroy the Active Sessions Shared Memory object if no more active
     * sessions exist.
     */
    ~ActiveSessionsSharedMemory()
    {
        if (auto const lock = bi::scoped_lock<bi::named_mutex>(mutex);
            !active_sessions_map.empty())
        {
            return;
        }

        bi::named_condition::remove("ActiveSessionsConditionVar");
        bi::named_mutex::remove("ActiveSessionsMutex");
        bi::shared_memory_object::remove("ActiveSessionsSegment");
    }

  private:
    /**
     * @brief Create or find a shared memory map.
     *
     * @param id The name identifier for the shared map.
     * @returns A reference to the shared map.
     * @throws std::runtime_error if an interprocess error occurs.
     */
    [[nodiscard]] auto create_or_find_map_handle(char const *id) -> shared_map_t &
    {
        auto const result = segment.find_or_construct<shared_map_t>(id)(
            std::less<key_t>(), segment.get_allocator<map_alloc_t>());
        if (!result)
        {
            throw std::runtime_error("Failed to create or find shared map");
        }
        return *result;
    }
};

auto shared_memory = ActiveSessionsSharedMemory{65'536};

} // namespace

// ----------------------------------------------------------------------------------------------

namespace xen
{

SessionListener::SessionListener() : juce::Thread{"SessionListener"}
{
    if (!this->startThread())
    {
        throw std::runtime_error{"Failed to start SessionListener thread"};
    }
}

SessionListener::~SessionListener()
{
    this->signalThreadShouldExit();
    this->stopThread(500);
}

void SessionListener::run()
{
    while (!this->threadShouldExit())
    {
        auto const timeout_time = boost::posix_time::microsec_clock::universal_time() +
                                  boost::posix_time::milliseconds(500);
        auto lock = bi::scoped_lock{shared_memory.mutex};
        if (shared_memory.condition_var.timed_wait(lock, timeout_time))
        {
            // Some session state has changed, emit signal on the main thread.
            juce::MessageManager::callAsync([this] { this->on_update(); });
        }
    }
}

// ----------------------------------------------------------------------------------------------

CurrentSession::CurrentSession(Metadata const &metadata, State const &state)
{
    auto const lock = bi::scoped_lock{shared_memory.mutex};

    shared_memory.active_sessions_map.try_emplace(
        this->get_process_uuid(),
        SharedPayload{
            string_t{metadata.display_name.c_str(),
                     char_alloc_t{shared_memory.segment.get_segment_manager()}},
            string_t{serialize_state(state).c_str(),
                     char_alloc_t{shared_memory.segment.get_segment_manager()}}});

    shared_memory.condition_var.notify_all();
}

CurrentSession::~CurrentSession()
{
    auto const lock = bi::scoped_lock{shared_memory.mutex};

    shared_memory.active_sessions_map.erase(this->get_process_uuid());
    shared_memory.condition_var.notify_all();
}

auto CurrentSession::get_process_uuid() const -> juce::Uuid
{
    return current_session_uuid;
}

auto CurrentSession::get_display_name() const -> std::string
{
    auto &map = shared_memory.active_sessions_map;
    auto const lock = bi::scoped_lock{shared_memory.mutex};

    // Locate the session with the given UUID
    if (auto it = map.find(current_session_uuid); it != std::cend(map))
    {
        return std::string{it->second.display_name.c_str()};
    }
    throw std::runtime_error("Shared state is uninitialized for the current process.");
}

auto CurrentSession::set_display_name(std::string const &name) -> void
{
    auto &map = shared_memory.active_sessions_map;
    auto const lock = bi::scoped_lock{shared_memory.mutex};

    if (auto it = map.find(current_session_uuid); it != std::end(map))
    {
        it->second.display_name = name.c_str();
        shared_memory.condition_var.notify_all();
        return;
    }
    throw std::runtime_error("Shared state is uninitialized for the current process.");
}

auto CurrentSession::set_state(xen::State const &state) -> void
{
    auto &map = shared_memory.active_sessions_map;
    auto const lock = bi::scoped_lock{shared_memory.mutex};

    // Locate the session with the given UUID
    if (auto it = map.find(current_session_uuid); it != std::end(map))
    {
        it->second.json_state = serialize_state(state).c_str();
        shared_memory.condition_var.notify_all();
        return;
    }
    throw std::runtime_error("Shared state is uninitialized for the current process.");
}

// ----------------------------------------------------------------------------------------------

ActiveSessions::ActiveSessions(Metadata const &metadata, State const &state)
    : current{metadata, state}
{
    session_listener_.on_update.connect(
        [this] { this->on_update(this->get_active_ids()); });
}

auto ActiveSessions::get_active_ids() -> std::vector<SessionID>
{
    auto &map = shared_memory.active_sessions_map;
    auto const lock = bi::scoped_lock{shared_memory.mutex};

    auto keys = std::vector<SessionID>{};
    keys.reserve(map.size());
    std::transform(
        std::begin(map), std::end(map), std::back_inserter(keys), [](auto const &pair) {
            return SessionID{pair.first, std::string{pair.second.display_name.c_str()}};
        });
    return keys;
}

auto ActiveSessions::get_state(juce::Uuid const &uuid) -> xen::State
{
    auto &map = shared_memory.active_sessions_map;
    auto const lock = bi::scoped_lock{shared_memory.mutex};

    // Locate the session with the given UUID
    if (auto it = map.find(uuid); it != std::cend(map))
    {
        return deserialize_state(it->second.json_state.c_str());
    }

    throw std::runtime_error("Session not found.");
}

} // namespace xen