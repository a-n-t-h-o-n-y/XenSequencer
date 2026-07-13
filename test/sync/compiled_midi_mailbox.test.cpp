#include <atomic>
#include <cstdint>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <xen/compiled_midi_mailbox.hpp>

namespace
{

auto make_update(std::uint64_t generation) -> xen::CompiledMidiUpdate
{
    auto update = xen::CompiledMidiUpdate{.generation = generation};
    update.schedule.generation = generation;
    update.schedule.loop_beats = static_cast<double>(generation);
    update.schedule.notes.resize(static_cast<std::size_t>(generation % 7U));
    return update;
}

static_assert(xen::CompiledMidiMailbox::control_is_always_lock_free);
static_assert(
    noexcept(std::declval<xen::CompiledMidiMailbox &>().try_consume_latest()));

} // namespace

TEST_CASE("CompiledMidiMailbox publishes immutable latest views", "[sync][mailbox]")
{
    auto mailbox = xen::CompiledMidiMailbox{};
    mailbox.publish(make_update(1));
    auto const first = mailbox.try_consume_latest();
    REQUIRE(first.has_value());
    auto const *first_address = &first->update();

    for (auto generation = std::uint64_t{2}; generation <= 100; ++generation)
    {
        mailbox.publish(make_update(generation));
        CHECK(&first->update() == first_address);
        CHECK(first->generation() == 1);
    }

    auto const latest = mailbox.try_consume_latest();
    REQUIRE(latest.has_value());
    CHECK(latest->generation() == 100);
    CHECK(latest->update().schedule.loop_beats == 100.0);
}

TEST_CASE("CompiledMidiMailbox SPSC stress keeps coherent generations",
          "[sync][mailbox][stress]")
{
    constexpr auto count = std::uint64_t{20'000};
    auto mailbox = xen::CompiledMidiMailbox{};
    auto done = std::atomic<bool>{false};
    auto producer = std::jthread{[&] {
        for (auto generation = std::uint64_t{1}; generation <= count; ++generation)
        {
            mailbox.publish(make_update(generation));
        }
        done.store(true, std::memory_order_release);
    }};

    auto last = std::uint64_t{};
    while (!done.load(std::memory_order_acquire) || last != count)
    {
        if (auto const view = mailbox.try_consume_latest())
        {
            CHECK(view->generation() > last);
            CHECK(view->update().schedule.generation == view->generation());
            last = view->generation();
        }
        else
        {
            std::this_thread::yield();
        }
    }
    CHECK(last == count);
}
