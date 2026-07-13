#include <atomic>
#include <cstdint>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <xen/audio_thread_state_exchange.hpp>

TEST_CASE("AudioThreadStateExchange reads default state", "[sync][audio-thread-state]")
{
    auto exchange = xen::AudioThreadStateExchange{};
    auto const snapshot = exchange.read();

    CHECK(snapshot.daw.bpm == 0.f);
    CHECK(snapshot.daw.sample_rate == 0);
    CHECK_FALSE(snapshot.daw.is_playing);
    CHECK(snapshot.loop_phase == 0.0);
    CHECK_FALSE(snapshot.transport_active);
}

TEST_CASE("AudioThreadStateExchange preserves coherent concurrent snapshots",
          "[sync][audio-thread-state]")
{
    auto exchange = xen::AudioThreadStateExchange{};
    auto done = std::atomic<bool>{false};
    auto failed = std::atomic<bool>{false};

    auto writer = std::thread{[&] {
        for (auto i = std::uint32_t{1}; i < 100'000; ++i)
        {
            exchange.write({
                .daw =
                    {
                        .bpm = static_cast<float>(i),
                        .sample_rate = i,
                        .is_playing = (i % 2U) == 0U,
                    },
                .loop_phase = static_cast<double>(i),
                .transport_active = (i % 2U) == 0U,
                .midi_status =
                    {
                        .current_fault = (i % 2U) == 0U
                                             ? xen::RealtimeMidiFault::MissingPpq
                                             : xen::RealtimeMidiFault::InvalidTransport,
                        .last_fault = xen::RealtimeMidiFault::EventCapacityExceeded,
                        .fault_count = i,
                    },
            });
        }
        done.store(true, std::memory_order_release);
    }};

    auto reader = std::thread{[&] {
        while (!done.load(std::memory_order_acquire))
        {
            auto const snapshot = exchange.read();
            auto const sample_rate = snapshot.daw.sample_rate;
            if (sample_rate == 0)
            {
                continue;
            }

            auto const expected_playing = (sample_rate % 2U) == 0U;
            if (snapshot.daw.bpm != static_cast<float>(sample_rate) ||
                snapshot.loop_phase != static_cast<double>(sample_rate) ||
                snapshot.daw.is_playing != expected_playing ||
                snapshot.transport_active != expected_playing ||
                snapshot.midi_status.fault_count != sample_rate ||
                snapshot.midi_status.last_fault !=
                    xen::RealtimeMidiFault::EventCapacityExceeded ||
                snapshot.midi_status.current_fault !=
                    (expected_playing ? xen::RealtimeMidiFault::MissingPpq
                                      : xen::RealtimeMidiFault::InvalidTransport))
            {
                failed.store(true, std::memory_order_release);
                return;
            }
        }
    }};

    writer.join();
    reader.join();

    CHECK_FALSE(failed.load(std::memory_order_acquire));
}
