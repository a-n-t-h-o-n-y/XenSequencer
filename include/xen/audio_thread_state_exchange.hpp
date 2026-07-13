#pragma once

#include <atomic>
#include <cstdint>

#include <xen/state.hpp>

namespace xen
{

class AudioThreadStateExchange
{
  public:
    void write(AudioThreadStateForGUI const &state) noexcept
    {
        auto const next_sequence =
            sequence_.load(std::memory_order_relaxed) + std::uint64_t{1};
        sequence_.store(next_sequence, std::memory_order_release);

        bpm_.store(state.daw.bpm, std::memory_order_relaxed);
        sample_rate_.store(state.daw.sample_rate, std::memory_order_relaxed);
        daw_playing_.store(state.daw.is_playing, std::memory_order_relaxed);
        loop_phase_.store(state.loop_phase, std::memory_order_relaxed);
        transport_active_.store(state.transport_active, std::memory_order_relaxed);
        midi_current_fault_.store(state.midi_status.current_fault,
                                  std::memory_order_relaxed);
        midi_last_fault_.store(state.midi_status.last_fault, std::memory_order_relaxed);
        midi_fault_count_.store(state.midi_status.fault_count,
                                std::memory_order_relaxed);

        sequence_.store(next_sequence + std::uint64_t{1}, std::memory_order_release);
    }

    [[nodiscard]] auto read() const noexcept -> AudioThreadStateForGUI
    {
        auto out = AudioThreadStateForGUI{};
        for (;;)
        {
            auto const before = sequence_.load(std::memory_order_acquire);
            if ((before & std::uint64_t{1}) != 0)
            {
                continue;
            }

            out = {
                .daw =
                    {
                        .bpm = bpm_.load(std::memory_order_relaxed),
                        .sample_rate = sample_rate_.load(std::memory_order_relaxed),
                        .is_playing = daw_playing_.load(std::memory_order_relaxed),
                    },
                .loop_phase = loop_phase_.load(std::memory_order_relaxed),
                .transport_active = transport_active_.load(std::memory_order_relaxed),
                .midi_status =
                    {
                        .current_fault =
                            midi_current_fault_.load(std::memory_order_relaxed),
                        .last_fault = midi_last_fault_.load(std::memory_order_relaxed),
                        .fault_count =
                            midi_fault_count_.load(std::memory_order_relaxed),
                    },
            };

            auto const after = sequence_.load(std::memory_order_acquire);
            if (before == after)
            {
                return out;
            }
        }
    }

  private:
    alignas(64) mutable std::atomic<std::uint64_t> sequence_{0};
    alignas(64) std::atomic<float> bpm_{0.f};
    std::atomic<std::uint32_t> sample_rate_{0};
    std::atomic<bool> daw_playing_{false};
    std::atomic<double> loop_phase_{0.0};
    std::atomic<bool> transport_active_{false};
    std::atomic<RealtimeMidiFault> midi_current_fault_{RealtimeMidiFault::None};
    std::atomic<RealtimeMidiFault> midi_last_fault_{RealtimeMidiFault::None};
    std::atomic<std::uint64_t> midi_fault_count_{0};
};

} // namespace xen
