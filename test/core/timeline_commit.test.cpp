#include <catch2/catch_test_macros.hpp>

#include <type_traits>

#include <xen/state.hpp>
#include <xen/timeline.hpp>

using namespace xen;

static_assert(!std::is_same_v<HistoryEntryId, ProjectRevision>);

TEST_CASE("Timeline commit requires explicit state and preserves redo on no-op",
          "[core][timeline][commit]")
{
    auto timeline = XenTimeline{ProjectState{}};
    auto engine = timeline.get_state();
    engine.composition.columns.front().pitch.transposition = 1;
    timeline.stage(engine);
    REQUIRE(timeline.commit(timeline.get_state()));
    engine.composition.columns.front().pitch.transposition = 2;
    timeline.stage(engine);
    REQUIRE(timeline.commit(timeline.get_state()));
    REQUIRE(timeline.undo());

    auto const entry_before = timeline.get_current_entry_id();
    auto const revision_before = timeline.get_project_revision();
    CHECK_FALSE(timeline.commit(timeline.get_state()));
    CHECK(timeline.get_current_entry_id() == entry_before);
    CHECK(timeline.get_project_revision() == revision_before);
    CHECK(timeline.redo());
    CHECK(timeline.get_state().composition.columns.front().pitch.transposition == 2);
}

TEST_CASE("Guarded amendment and replacement keep history invariants",
          "[core][timeline][commit]")
{
    auto timeline = XenTimeline{ProjectState{}};
    auto state = timeline.get_state();
    state.composition.columns.front().pitch.transposition = 1;
    timeline.stage(state);
    REQUIRE(timeline.commit(timeline.get_state()));

    auto const entry_id = timeline.get_current_entry_id();
    auto const revision = timeline.get_project_revision();
    state.composition.columns.front().pitch.transposition = 2;

    REQUIRE(timeline.amend_current(entry_id, state));
    CHECK(timeline.get_current_entry_id() == entry_id);
    CHECK(timeline.get_project_revision() != revision);

    auto const replaced_entry = timeline.get_current_entry_id();
    auto const replaced_revision = timeline.get_project_revision();
    timeline.replace_history(timeline.get_state());
    CHECK(timeline.get_current_entry_id() != replaced_entry);
    CHECK(timeline.get_project_revision() != replaced_revision);
    CHECK_FALSE(timeline.undo());
    CHECK_FALSE(timeline.redo());
}
