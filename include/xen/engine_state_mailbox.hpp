#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include <xen/project_validation.hpp>

namespace xen
{

/**
 * Single-producer/single-consumer mailbox for validated audio project snapshots.
 *
 * Publishing copies into a producer-owned slot and may allocate for dynamic state.
 * Consuming only exchanges slot ownership and returns a view into the consumer-owned
 * slot. Intermediate publications may be skipped.
 */
class EngineStateMailbox
{
  private:
    using ControlWord = std::uint8_t;

    static constexpr auto DIRTY_MASK = ControlWord{0x04};
    static constexpr auto INDEX_MASK = ControlWord{0x03};

    static_assert(std::atomic<ControlWord>::is_always_lock_free,
                  "EngineStateMailbox control word must always be lock-free");

    struct Snapshot
    {
        AudioProjectSnapshot state{};
        std::uint64_t version{0};
    };

  public:
    static constexpr bool control_is_always_lock_free =
        std::atomic<ControlWord>::is_always_lock_free;

    class ReadView
    {
      public:
        [[nodiscard]] auto state() const noexcept -> AudioProjectSnapshot const &
        {
            return snapshot_->state;
        }

        [[nodiscard]] auto version() const noexcept -> std::uint64_t
        {
            return snapshot_->version;
        }

      private:
        friend class EngineStateMailbox;

        explicit ReadView(Snapshot const *snapshot) noexcept : snapshot_{snapshot}
        {
        }

        Snapshot const *snapshot_;
    };

    void publish(AudioProjectSnapshot const &state)
    {
        validate(state.project);
        if (producer_active_.test_and_set(std::memory_order_acquire))
        {
            throw std::logic_error{"EngineStateMailbox::publish() called concurrently"};
        }

        struct ProducerGuard
        {
            std::atomic_flag &active;

            ~ProducerGuard()
            {
                active.clear(std::memory_order_release);
            }
        } guard{producer_active_};

        auto const next_version = version_.load(std::memory_order_relaxed) + 1;
        auto &snapshot = snapshots_[producer_slot_];
        snapshot.state = state;
        snapshot.version = next_version;

        auto const previous_control =
            control_.exchange(encode(producer_slot_, true), std::memory_order_acq_rel);
        producer_slot_ = decode_index(previous_control);
        version_.store(next_version, std::memory_order_release);
    }

    [[nodiscard]] auto version() const noexcept -> std::uint64_t
    {
        return version_.load(std::memory_order_acquire);
    }

    /**
     * Consume the latest published snapshot, if one is pending.
     *
     * The returned view remains valid and unchanged until this consumer's next
     * successful call.
     */
    [[nodiscard]] auto try_consume_latest() noexcept -> std::optional<ReadView>
    {
        auto const observed = control_.load(std::memory_order_acquire);
        if (!is_dirty(observed))
        {
            return std::nullopt;
        }

        auto const previous_control =
            control_.exchange(encode(consumer_slot_, false), std::memory_order_acq_rel);
        consumer_slot_ = decode_index(previous_control);
        return ReadView{&snapshots_[consumer_slot_]};
    }

  private:
    [[nodiscard]] static constexpr auto encode(std::size_t index, bool dirty) noexcept
        -> ControlWord
    {
        return static_cast<ControlWord>(static_cast<ControlWord>(index) |
                                        (dirty ? DIRTY_MASK : 0));
    }

    [[nodiscard]] static constexpr auto decode_index(ControlWord control) noexcept
        -> std::size_t
    {
        return static_cast<std::size_t>(control & INDEX_MASK);
    }

    [[nodiscard]] static constexpr auto is_dirty(ControlWord control) noexcept -> bool
    {
        return (control & DIRTY_MASK) != 0;
    }

    std::array<Snapshot, 3> snapshots_{};
    std::size_t producer_slot_{1};
    std::size_t consumer_slot_{0};
    alignas(64) std::atomic<ControlWord> control_{encode(2, false)};
    alignas(64) std::atomic_flag producer_active_ = ATOMIC_FLAG_INIT;
    std::atomic<std::uint64_t> version_{0};
};

} // namespace xen
