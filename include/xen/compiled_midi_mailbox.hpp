#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>

#include <xen/midi_schedule.hpp>

namespace xen
{

class CompiledMidiMailbox
{
  private:
    using ControlWord = std::uint8_t;
    static constexpr auto DIRTY_MASK = ControlWord{0x04};
    static constexpr auto INDEX_MASK = ControlWord{0x03};

    struct Snapshot
    {
        CompiledMidiUpdate update{};
    };

  public:
    static constexpr bool control_is_always_lock_free =
        std::atomic<ControlWord>::is_always_lock_free;
    static_assert(control_is_always_lock_free);

    class ReadView
    {
      public:
        [[nodiscard]] auto update() const noexcept -> CompiledMidiUpdate const &
        {
            return snapshot_->update;
        }

        [[nodiscard]] auto generation() const noexcept -> std::uint64_t
        {
            return snapshot_->update.generation;
        }

      private:
        friend class CompiledMidiMailbox;
        explicit ReadView(Snapshot const *snapshot) noexcept : snapshot_{snapshot}
        {
        }
        Snapshot const *snapshot_{};
    };

    void publish(CompiledMidiUpdate update)
    {
        if (producer_active_.test_and_set(std::memory_order_acquire))
        {
            throw std::logic_error{"CompiledMidiMailbox::publish called concurrently"};
        }
        struct Guard
        {
            std::atomic_flag &flag;
            ~Guard()
            {
                flag.clear(std::memory_order_release);
            }
        } guard{producer_active_};

        auto const generation = update.generation;
        snapshots_[producer_slot_].update = std::move(update);
        auto const previous =
            control_.exchange(encode(producer_slot_, true), std::memory_order_acq_rel);
        producer_slot_ = decode_index(previous);
        generation_.store(generation, std::memory_order_release);
    }

    [[nodiscard]] auto generation() const noexcept -> std::uint64_t
    {
        return generation_.load(std::memory_order_acquire);
    }

    // A returned view remains valid until this consumer next calls this method.
    [[nodiscard]] auto try_consume_latest() noexcept -> std::optional<ReadView>
    {
        auto const observed = control_.load(std::memory_order_acquire);
        if ((observed & DIRTY_MASK) == 0)
        {
            return std::nullopt;
        }
        auto const previous =
            control_.exchange(encode(consumer_slot_, false), std::memory_order_acq_rel);
        consumer_slot_ = decode_index(previous);
        return ReadView{&snapshots_[consumer_slot_]};
    }

  private:
    [[nodiscard]] static constexpr auto encode(std::size_t index, bool dirty) noexcept
        -> ControlWord
    {
        return static_cast<ControlWord>(index | (dirty ? DIRTY_MASK : 0));
    }

    [[nodiscard]] static constexpr auto decode_index(ControlWord word) noexcept
        -> std::size_t
    {
        return static_cast<std::size_t>(word & INDEX_MASK);
    }

    std::array<Snapshot, 3> snapshots_{};
    std::size_t producer_slot_{1};
    std::size_t consumer_slot_{0};
    alignas(64) std::atomic<ControlWord> control_{encode(2, false)};
    alignas(64) std::atomic_flag producer_active_ = ATOMIC_FLAG_INIT;
    std::atomic<std::uint64_t> generation_{0};
};

} // namespace xen
