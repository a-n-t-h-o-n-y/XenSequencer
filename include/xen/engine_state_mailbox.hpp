#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include <xen/state.hpp>

namespace xen
{

/**
 * Single-producer/single-consumer mailbox for latest EngineState snapshots.
 *
 * Producer thread publishes immutable snapshots. Consumer thread only reads the latest
 * coherent snapshot and can skip intermediate updates.
 */
class EngineStateMailbox
{
  public:
    void publish(EngineState const &state)
    {
        auto snapshot = std::make_shared<EngineState const>(state);
        std::atomic_store_explicit(&latest_, std::move(snapshot),
                                   std::memory_order_release);
        version_.fetch_add(1, std::memory_order_release);
    }

    [[nodiscard]] auto version() const noexcept -> std::uint64_t
    {
        return version_.load(std::memory_order_acquire);
    }

    /**
     * Try consuming the most recent snapshot if it is newer than `last_seen_version`.
     *
     * Returns true when `out` has been updated.
     */
    auto try_consume_latest(EngineState &out,
                            std::uint64_t &last_seen_version) const -> bool
    {
        for (auto i = 0; i < 4; ++i)
        {
            auto const begin_version = version_.load(std::memory_order_acquire);
            if (begin_version == 0 || begin_version == last_seen_version)
            {
                return false;
            }

            auto ptr = std::atomic_load_explicit(&latest_, std::memory_order_acquire);
            auto const end_version = version_.load(std::memory_order_acquire);
            if (begin_version != end_version)
            {
                continue;
            }

            if (!ptr)
            {
                return false;
            }

            out = *ptr;
            last_seen_version = end_version;
            return true;
        }

        return false;
    }

  private:
    mutable std::shared_ptr<EngineState const> latest_{};
    std::atomic<std::uint64_t> version_{0};
};

} // namespace xen
