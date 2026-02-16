#include <catch2/catch_test_macros.hpp>

#include <xen/timeline.hpp>

namespace
{
struct IntState
{
    int value{0};
    auto operator==(IntState const &) const -> bool = default;
};
} // namespace

TEST_CASE("Timeline stage, commit, and commit IDs", "[unit][timeline]")
{
    auto timeline = xen::Timeline<IntState>{IntState{0}};

    REQUIRE(timeline.get_state().value == 0);
    REQUIRE(timeline.get_current_commit_id() == 0);

    timeline.stage(IntState{11});
    REQUIRE(timeline.get_state().value == 11);

    timeline.commit();
    REQUIRE(timeline.get_state().value == 11);
    REQUIRE(timeline.get_current_commit_id() == 1);
}

TEST_CASE("Timeline undo, redo, and future truncation", "[unit][timeline]")
{
    auto timeline = xen::Timeline<IntState>{IntState{0}};

    timeline.stage(IntState{1});
    timeline.commit();
    timeline.stage(IntState{2});
    timeline.commit();

    REQUIRE(timeline.undo());
    REQUIRE(timeline.get_state().value == 1);

    REQUIRE(timeline.redo());
    REQUIRE(timeline.get_state().value == 2);

    REQUIRE(timeline.undo());
    timeline.stage(IntState{99});
    timeline.commit();

    REQUIRE_FALSE(timeline.redo());
    REQUIRE(timeline.get_state().value == 99);
}

TEST_CASE("Timeline commit flag and stage reset", "[unit][timeline]")
{
    auto timeline = xen::Timeline<IntState>{IntState{5}};

    REQUIRE_FALSE(timeline.get_commit_flag());

    timeline.set_commit_flag();
    REQUIRE(timeline.get_commit_flag());

    timeline.stage(IntState{42});
    REQUIRE(timeline.get_state().value == 42);

    timeline.reset_stage();
    REQUIRE(timeline.get_state().value == 5);

    timeline.commit();
    REQUIRE_FALSE(timeline.get_commit_flag());
}
