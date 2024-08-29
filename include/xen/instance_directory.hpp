#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_recursive_mutex.hpp>

#include <juce_core/juce_core.h>

namespace xen
{

/**
 * Manages a directory of active instances of this app. This is used to determine which
 * instances are currently running.
 */
class InstanceDirectory
{
  public:
    using HeartbeatClock = std::chrono::steady_clock;

  public:
    /**
     * Create a handle to the instance directory. This will create the directory if it
     * does not exist yet, otherwise it will open it.
     *
     * @throws std::runtime_error if any errors encountered.
     */
    InstanceDirectory();

    /**
     * Destroy the handle to the instance directory. If this is the last handle to the
     * directory, it will remove the directory and shared memory segment.
     */
    ~InstanceDirectory();

  public:
    /**
     * Returns a listing of all active instances.
     *
     * @return std::vector<juce::Uuid> containing the UUIDs of all active instances.
     * @throws std::runtime_error if any errors encountered.
     */
    [[nodiscard]] auto get_active_instances() const -> std::vector<juce::Uuid>;

    /**
     * Registers an instance with the directory.
     *
     * @param uuid UUID of the instance.
     * @throws std::runtime_error if any errors encountered.
     */
    void register_instance(juce::Uuid const &uuid);

    /**
     * Unregisters an instance from the directory.
     *
     * @details No-op if the give UUID is not registered.
     * @param uuid UUID of the instance.
     * @throws std::runtime_error if any errors encountered.
     */
    void unregister_instance(juce::Uuid const &uuid) const;

    /**
     * Update the last heartbeat time of an instance to the current time.
     *
     * @param uuid UUID of the instance.
     * @throws std::runtime_error if any errors encountered.
     */
    void send_heartbeat(juce::Uuid const &uuid) const;

    /**
     * Unregisters any instances which have not sent a heartbeat in the last
     * \p elapsed_time.
     *
     * @param elapsed_time Time since last heartbeat.
     * @throws std::runtime_error if any errors encountered.
     */
    void unregister_dead_instances(HeartbeatClock::duration const &elapsed_time) const;

    /**
     * Returns the number of instances in the directory.
     *
     * @return std::size_t Number of instances in the directory.
     * @throws std::runtime_error if any errors encountered.
     */
    [[nodiscard]] auto size() const -> std::size_t;

    /**
     * Returns a reference to the mutex used to synchronize access to the directory.
     *
     * @details This is a recursive mutex and can be used to chain multiple
     * InstanceDirectory operations together into an 'atomic' operation without
     * deadlocking.
     * @return boost::interprocess::named_recursive_mutex & Mutex.
     */
    [[nodiscard]] auto get_mutex() const
        -> boost::interprocess::named_recursive_mutex &;

  private:
    // Shared Memory Types
    template <typename T>
    using allocator_t = boost::interprocess::allocator<
        T, boost::interprocess::managed_shared_memory::segment_manager>;

    using key_t = juce::Uuid;
    using value_t = xen::InstanceDirectory::HeartbeatClock::time_point;
    using map_allocator_t = allocator_t<std::pair<const key_t, value_t>>;

    using SharedMap =
        boost::interprocess::map<key_t, value_t, std::less<key_t>, map_allocator_t>;

  private:
    [[nodiscard]] static auto find_or_construct_directory(
        boost::interprocess::managed_shared_memory &segment,
        boost::interprocess::named_recursive_mutex &mutex) -> SharedMap &;

  private:
    boost::interprocess::managed_shared_memory segment_;
    mutable boost::interprocess::named_recursive_mutex mutex_;
    SharedMap &directory_;
};

} // namespace xen