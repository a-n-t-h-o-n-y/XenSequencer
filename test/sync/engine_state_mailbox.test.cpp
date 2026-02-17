#include <cstdint>
#include <string>
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
    state.sequence_names[0] = "seq-" + std::to_string(id);
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

} // namespace

TEST_CASE("EngineStateMailbox publishes and consumes latest state", "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};
    auto out = EngineState{};
    auto last_seen_version = std::uint64_t{0};

    CHECK(mailbox.version() == 0);
    CHECK_FALSE(mailbox.try_consume_latest(out, last_seen_version));
    CHECK(last_seen_version == 0);

    auto published = EngineState{};
    published.key = 7;
    published.base_frequency = 432.f;
    published.tuning_name = "test tuning";
    published.sequence_names[0] = "lead";

    mailbox.publish(published);

    CHECK(mailbox.version() == 1);
    CHECK(mailbox.try_consume_latest(out, last_seen_version));
    CHECK(last_seen_version == 1);
    CHECK(out == published);

    CHECK_FALSE(mailbox.try_consume_latest(out, last_seen_version));
    CHECK(last_seen_version == 1);
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

TEST_CASE("EngineStateMailbox consumes only latest when publishes are skipped",
          "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};
    auto out = EngineState{};
    auto last_seen_version = std::uint64_t{0};

    auto const first = make_state(10);
    auto const second = make_state(20);
    auto const third = make_state(30);

    mailbox.publish(first);
    mailbox.publish(second);
    mailbox.publish(third);

    CHECK(mailbox.version() == 3);
    CHECK(mailbox.try_consume_latest(out, last_seen_version));
    CHECK(last_seen_version == 3);
    CHECK(out == third);
    CHECK(out != first);
    CHECK(out != second);
    CHECK_FALSE(mailbox.try_consume_latest(out, last_seen_version));
}

TEST_CASE("EngineStateMailbox publish snapshots are immutable to later source mutations",
          "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};
    auto out = EngineState{};
    auto last_seen_version = std::uint64_t{0};

    auto published = make_state(42);
    published.tuning.description = "before-publish";
    published.tuning.intervals = {0, 50, 120};
    auto const expected = published;

    mailbox.publish(published);

    // Mutate source object after publish; mailbox snapshot should not change.
    published.key = -1;
    published.base_frequency = 999.f;
    published.tuning_name = "mutated";
    published.sequence_names[0] = "mutated";
    published.tuning.description = "after-publish";
    published.tuning.intervals = {0, 1, 2, 3};
    REQUIRE(published.scale.has_value());
    published.scale->name = "mutated";
    published.scale->intervals = {9, 9, 9};

    CHECK(mailbox.try_consume_latest(out, last_seen_version));
    CHECK(last_seen_version == 1);
    CHECK(out == expected);
    CHECK(out != published);
}

TEST_CASE("EngineStateMailbox respects last seen version gating", "[sync][mailbox]")
{
    auto mailbox = EngineStateMailbox{};
    auto out = EngineState{};
    auto last_seen_version = std::uint64_t{0};

    auto const first = make_state(5);
    auto const second = make_state(6);

    mailbox.publish(first);
    REQUIRE(mailbox.try_consume_latest(out, last_seen_version));
    CHECK(last_seen_version == 1);
    CHECK(out == first);
    CHECK_FALSE(mailbox.try_consume_latest(out, last_seen_version));

    mailbox.publish(second);
    CHECK(mailbox.try_consume_latest(out, last_seen_version));
    CHECK(last_seen_version == 2);
    CHECK(out == second);
    CHECK_FALSE(mailbox.try_consume_latest(out, last_seen_version));
}
