#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <xen/engine_state_mailbox.hpp>

using namespace xen;

namespace
{

auto make_state(int id) -> EngineState
{
    auto state = EngineState{};
    state.key = id;
    state.base_frequency = 400.f + static_cast<float>(id);
    state.tuning_name = "tuning-" + std::to_string(id);
    state.tuning.description = "description-" + std::to_string(id);
    state.tuning.intervals = {
        static_cast<float>(id),
        static_cast<float>(id + 10),
        static_cast<float>(id + 20),
    };
    state.measure.time_signature.numerator = static_cast<unsigned>((id % 7) + 1);
    state.scale = Scale{
        .name = "scale-" + std::to_string(id),
        .tuning_length = 12,
        .intervals = std::vector<std::uint8_t>{
            static_cast<std::uint8_t>(id % 12),
            static_cast<std::uint8_t>((id + 1) % 12),
            static_cast<std::uint8_t>((id + 2) % 12),
        },
        .mode = 1,
    };
    return state;
}

void check_state(EngineState const &state, int id)
{
    CHECK(state.key == id);
    CHECK(static_cast<int>(state.base_frequency) == 400 + id);
    CHECK(state.tuning_name == "tuning-" + std::to_string(id));
    CHECK(state.tuning.description == "description-" + std::to_string(id));
    CHECK(state.tuning.intervals ==
          std::vector<float>{
              static_cast<float>(id),
              static_cast<float>(id + 10),
              static_cast<float>(id + 20),
          });
    CHECK(state.measure.time_signature.numerator ==
          static_cast<unsigned>((id % 7) + 1));
    REQUIRE(state.scale.has_value());
    CHECK(state.scale->name == "scale-" + std::to_string(id));
    CHECK(state.scale->intervals ==
          std::vector<std::uint8_t>{
              static_cast<std::uint8_t>(id % 12),
              static_cast<std::uint8_t>((id + 1) % 12),
              static_cast<std::uint8_t>((id + 2) % 12),
          });
}

static_assert(EngineStateMailbox::control_is_always_lock_free);
static_assert(
    noexcept(std::declval<EngineStateMailbox &>().try_consume_latest()));

} // namespace

TEST_CASE("EngineStateMailbox publishes and consumes a read view",
          "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};

    CHECK(mailbox.version() == 0);
    CHECK_FALSE(mailbox.try_consume_latest().has_value());

    auto const published = make_state(7);
    mailbox.publish(published);

    CHECK(mailbox.version() == 1);
    auto const view = mailbox.try_consume_latest();
    REQUIRE(view.has_value());
    CHECK(view->version() == 1);
    CHECK(view->state() == published);
    CHECK_FALSE(mailbox.try_consume_latest().has_value());
}

TEST_CASE("EngineStateMailbox version increments per publish", "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};

    CHECK(mailbox.version() == 0);
    mailbox.publish(make_state(1));
    CHECK(mailbox.version() == 1);
    mailbox.publish(make_state(2));
    CHECK(mailbox.version() == 2);
    mailbox.publish(make_state(3));
    CHECK(mailbox.version() == 3);
}

TEST_CASE("EngineStateMailbox consumes only latest when publications are skipped",
          "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};
    auto const first = make_state(10);
    auto const second = make_state(20);
    auto const third = make_state(30);

    mailbox.publish(first);
    mailbox.publish(second);
    mailbox.publish(third);

    CHECK(mailbox.version() == 3);
    auto const view = mailbox.try_consume_latest();
    REQUIRE(view.has_value());
    CHECK(view->version() == 3);
    CHECK(view->state() == third);
    CHECK(view->state() != first);
    CHECK(view->state() != second);
    CHECK_FALSE(mailbox.try_consume_latest().has_value());
}

TEST_CASE("EngineStateMailbox snapshots are immutable to source mutations",
          "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};
    auto published = make_state(42);
    auto const expected = published;

    mailbox.publish(published);

    published.key = -1;
    published.base_frequency = 999.f;
    published.tuning_name = "mutated";
    published.measure.time_signature = {3, 4};
    published.tuning.description = "after-publish";
    published.tuning.intervals = {0, 1, 2, 3};
    REQUIRE(published.scale.has_value());
    published.scale->name = "mutated";
    published.scale->intervals = {9, 9, 9};

    auto const view = mailbox.try_consume_latest();
    REQUIRE(view.has_value());
    CHECK(view->version() == 1);
    CHECK(view->state() == expected);
    CHECK(view->state() != published);
}

TEST_CASE("Consumed EngineStateMailbox view remains stable across publications",
          "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};
    auto const first = make_state(1);
    mailbox.publish(first);

    auto const first_view = mailbox.try_consume_latest();
    REQUIRE(first_view.has_value());
    auto const *first_address = &first_view->state();

    for (auto id = 2; id <= 100; ++id)
    {
        mailbox.publish(make_state(id));
        CHECK(&first_view->state() == first_address);
        CHECK(first_view->version() == 1);
        CHECK(first_view->state() == first);
    }

    auto const latest_view = mailbox.try_consume_latest();
    REQUIRE(latest_view.has_value());
    CHECK(latest_view->version() == 100);
    CHECK(latest_view->state() == make_state(100));
}

TEST_CASE("EngineStateMailbox supports sequential producers on different threads",
          "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};

    {
        auto producer = std::jthread{[&mailbox] { mailbox.publish(make_state(1)); }};
    }
    {
        auto producer = std::jthread{[&mailbox] { mailbox.publish(make_state(2)); }};
    }

    auto const view = mailbox.try_consume_latest();
    REQUIRE(view.has_value());
    CHECK(view->version() == 2);
    CHECK(view->state() == make_state(2));
}

TEST_CASE("EngineStateMailbox SPSC stress preserves coherent latest states",
          "[sync][mailbox][stress]")
{
    constexpr auto publication_count = 20'000;
    auto mailbox = EngineStateMailbox{};
    auto producer_done = std::atomic<bool>{false};

    auto producer = std::jthread{[&mailbox, &producer_done] {
        for (auto id = 1; id <= publication_count; ++id)
        {
            mailbox.publish(make_state(id));
        }
        producer_done.store(true, std::memory_order_release);
    }};

    auto last_version = std::uint64_t{0};
    auto consumed_final = false;
    auto const deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds{10};

    while (!consumed_final && std::chrono::steady_clock::now() < deadline)
    {
        if (auto const view = mailbox.try_consume_latest())
        {
            CHECK(view->version() > last_version);
            last_version = view->version();
            auto const id = static_cast<int>(view->version());
            check_state(view->state(), id);
            consumed_final = view->version() == publication_count;
        }
        else if (!producer_done.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
    }

    producer.join();
    CHECK(producer_done.load(std::memory_order_acquire));
    CHECK(consumed_final);
    CHECK(last_version == publication_count);
    CHECK(mailbox.version() == publication_count);
}
