#include <xen/instance_directory.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_recursive_mutex.hpp>
#include <juce_core/juce_core.h>

namespace bip = boost::interprocess;

namespace xen
{

InstanceDirectory::InstanceDirectory()
    : segment_{bip::open_or_create, "xen_instance_directory", 8'192},
      mutex_{bip::open_or_create, "xen_instance_directory_mutex"},
      directory_{find_or_construct_directory(segment_, mutex_)}
{
}

InstanceDirectory::~InstanceDirectory()
{
    auto const lock = std::lock_guard{mutex_};

    if (directory_.empty())
    {

        segment_.destroy<SharedMap>("InstanceDirectoryMap");

        if (!bip::shared_memory_object::remove("xen_instance_directory"))
        {
            std::cerr << "Failed to remove shared memory object xen_instance_directory"
                      << std::endl;
        }

        if (!bip::named_recursive_mutex::remove("xen_instance_directory_mutex"))
        {
            std::cerr << "Failed to remove named mutex xen_instance_directory_mutex"
                      << std::endl;
        }
    }
}

auto InstanceDirectory::get_active_instances() const -> std::vector<juce::Uuid>
{
    auto const lock = std::lock_guard{mutex_};

    auto uuids = std::vector<juce::Uuid>{};
    for (auto &[uuid, heartbeat] : directory_)
    {
        uuids.push_back(uuid);
    }

    return uuids;
}

auto InstanceDirectory::register_instance(juce::Uuid const &uuid) -> void
{
    auto const lock = std::lock_guard{mutex_};

    if (directory_.count(uuid) > 0)
    {
        throw std::runtime_error{"UUID already registered"};
    }

    directory_.insert({uuid, HeartbeatClock::now()});
}

auto InstanceDirectory::unregister_instance(juce::Uuid const &uuid) const -> void
{
    auto const lock = std::lock_guard{mutex_};

    directory_.erase(uuid);
}

auto InstanceDirectory::send_heartbeat(juce::Uuid const &uuid) const -> void
{
    auto const lock = std::lock_guard{mutex_};

    if (auto const it = directory_.find(uuid); it == std::end(directory_))
    {
        throw std::runtime_error{"UUID not registered"};
    }
    else
    {
        it->second = HeartbeatClock::now();
    }
}

auto InstanceDirectory::unregister_dead_instances(
    HeartbeatClock::duration const &elapsed_time) const -> void
{
    auto const lock = std::lock_guard{mutex_};

    auto now = HeartbeatClock::now();

    for (auto &[uuid, heartbeat] : directory_)
    {
        if (now - heartbeat > elapsed_time)
        {
            directory_.erase(uuid);
        }
    }
}

auto InstanceDirectory::size() const -> std::size_t
{
    auto const lock = std::lock_guard{mutex_};

    return directory_.size();
}

auto InstanceDirectory::get_mutex() const
    -> boost::interprocess::named_recursive_mutex &
{
    return mutex_;
}

auto InstanceDirectory::find_or_construct_directory(bip::managed_shared_memory &segment,
                                                    bip::named_recursive_mutex &mutex)
    -> SharedMap &
{
    auto const lock = bip::scoped_lock{mutex};

    SharedMap *map_ptr = segment.find_or_construct<SharedMap>("InstanceDirectoryMap")(
        std::less<key_t>(), segment.get_allocator<map_allocator_t>());
    if (map_ptr == nullptr)
    {
        throw std::runtime_error{"Failed to find or construct InstanceDirectoryMap"};
    }
    return *map_ptr;
}

} // namespace xen