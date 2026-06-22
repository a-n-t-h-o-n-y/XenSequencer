#include <catch2/catch_test_macros.hpp>

#include <type_traits>

#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto current_context(XenProcessor const &processor,
                     std::optional<SelectionPath> selection = std::nullopt)
    -> CommandContext
{
    return {
        .selection = std::move(selection),
        .expected_project_revision =
            processor.get_engine_snapshot().project_revision,
    };
}

} // namespace

static_assert(!std::is_same_v<HistoryEntryId, ProjectRevision>);

TEST_CASE("Mutating commands advance history identity and project revision",
          "[core][timeline][commit]")
{
    auto processor = XenProcessor{};
    auto const initial = processor.get_engine_snapshot();

    auto const result =
        processor.execute_command_string("set key 12", current_context(processor));
    CHECK(result.status.first == MessageLevel::Info);

    auto const after = processor.get_engine_snapshot();
    CHECK(after.history_entry_id != initial.history_entry_id);
    CHECK(after.project_revision != initial.project_revision);
    CHECK(after.engine.key == 12);
}

TEST_CASE("Timeline commit requires explicit state and preserves redo on no-op",
          "[core][timeline][commit]")
{
    auto timeline = XenTimeline{EngineState{}};
    auto engine = timeline.get_state();
    engine.key = 1;
    timeline.stage(engine);
    REQUIRE(timeline.commit(timeline.get_state()));
    engine.key = 2;
    timeline.stage(engine);
    REQUIRE(timeline.commit(timeline.get_state()));
    REQUIRE(timeline.undo());

    auto const entry_before = timeline.get_current_entry_id();
    auto const revision_before = timeline.get_project_revision();
    CHECK_FALSE(timeline.commit(timeline.get_state()));
    CHECK(timeline.get_current_entry_id() == entry_before);
    CHECK(timeline.get_project_revision() == revision_before);
    CHECK(timeline.redo());
    CHECK(timeline.get_state().key == 2);
}

TEST_CASE("Guarded amendment and replacement keep history invariants",
          "[core][timeline][commit]")
{
    auto timeline = XenTimeline{EngineState{}};
    auto state = timeline.get_state();
    state.key = 1;
    timeline.stage(state);
    REQUIRE(timeline.commit(timeline.get_state()));

    auto const entry_id = timeline.get_current_entry_id();
    auto const revision = timeline.get_project_revision();
    state.key = 2;

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
