#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <vector>

#include <xen/input_mode.hpp>
#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto singleton_sequence_cell_selection(std::vector<std::size_t> const &indices)
    -> SelectedState
{
    auto selected = SelectedState{};
    for (auto const index : indices)
    {
        selected.path.push_back({.kind = SelectionStepKind::Element, .index = 0});
        selected.path.push_back(
            {.kind = SelectionStepKind::SequenceCell, .index = index});
    }
    return selected;
}

} // namespace

static_assert(!std::is_same_v<HistoryEntryId, ProjectRevision>);

TEST_CASE("Mutating commands advance history identity and project revision",
          "[core][timeline][commit]")
{
    auto processor = XenProcessor{};

    auto const initial = processor.get_engine_snapshot();

    auto const [version_level, _version_message] =
        processor.execute_command_string("version");
    CHECK(version_level == MessageLevel::Info);
    CHECK(processor.get_engine_snapshot().history_entry_id == initial.history_entry_id);
    CHECK(processor.get_engine_snapshot().project_revision == initial.project_revision);

    auto const [move_level, _move_message] =
        processor.execute_command_string("move right");
    CHECK(move_level == MessageLevel::Debug);
    CHECK(processor.get_engine_snapshot().history_entry_id == initial.history_entry_id);
    CHECK(processor.get_engine_snapshot().project_revision == initial.project_revision);

    auto const [set_key_level, _set_key_message] =
        processor.execute_command_string("set key 12");
    CHECK(set_key_level == MessageLevel::Info);

    auto const after = processor.get_engine_snapshot();
    CHECK(after.history_entry_id != initial.history_entry_id);
    CHECK(after.project_revision != initial.project_revision);
    CHECK(after.engine.key == 12);
}

TEST_CASE(
    "Undo reverts engine commit while preserving current selection and input mode",
    "[core][timeline][commit]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("split 2").first == MessageLevel::Info);
    processor.plugin_state.editor.selected = singleton_sequence_cell_selection({0});
    REQUIRE(processor.execute_command_string("inputMode gate").first ==
            MessageLevel::Info);
    REQUIRE(processor.execute_command_string("set key 9").first == MessageLevel::Info);
    auto const previous_commit = processor.get_engine_snapshot();
    REQUIRE(previous_commit.editor.selected == singleton_sequence_cell_selection({0}));
    REQUIRE(previous_commit.editor.input_mode == InputMode::Gate);

    REQUIRE(processor.execute_command_string("set key 11").first == MessageLevel::Info);
    auto const current = processor.get_engine_snapshot();
    REQUIRE(current.engine.key == 11);
    REQUIRE(current.history_entry_id != previous_commit.history_entry_id);
    REQUIRE(current.project_revision != previous_commit.project_revision);

    // Editor changes are session state and survive engine history movement.
    REQUIRE(processor.execute_command_string("move right").first ==
            MessageLevel::Debug);
    REQUIRE(processor.execute_command_string("inputMode pitch").first ==
            MessageLevel::Info);

    auto const [undo_level, _undo_message] = processor.execute_command_string("undo");
    CHECK(undo_level == MessageLevel::Info);

    auto const after_undo = processor.get_engine_snapshot();
    CHECK(after_undo.history_entry_id == previous_commit.history_entry_id);
    CHECK(after_undo.project_revision != previous_commit.project_revision);
    CHECK(after_undo.project_revision != current.project_revision);
    CHECK(after_undo.engine.key == previous_commit.engine.key);
    CHECK(after_undo.editor.selected != singleton_sequence_cell_selection({0}));
    CHECK(after_undo.editor.input_mode == InputMode::Pitch);
}

TEST_CASE("New commit after undo truncates redo history", "[core][timeline][commit]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("set key 1").first == MessageLevel::Info);
    REQUIRE(processor.execute_command_string("set key 2").first == MessageLevel::Info);

    auto const commit_with_key_2 = processor.get_engine_snapshot();
    REQUIRE(commit_with_key_2.engine.key == 2);

    REQUIRE(processor.execute_command_string("undo").first == MessageLevel::Info);
    auto const after_undo = processor.get_engine_snapshot();
    REQUIRE(after_undo.engine.key == 1);

    REQUIRE(processor.execute_command_string("set key 3").first == MessageLevel::Info);
    auto const after_new_commit = processor.get_engine_snapshot();
    REQUIRE(after_new_commit.engine.key == 3);
    REQUIRE(after_new_commit.history_entry_id != commit_with_key_2.history_entry_id);
    REQUIRE(after_new_commit.project_revision != commit_with_key_2.project_revision);

    auto const [redo_level, redo_message] = processor.execute_command_string("redo");
    CHECK(redo_level == MessageLevel::Info);
    CHECK(redo_message == "Nothing to redo.");

    auto const after_redo = processor.get_engine_snapshot();
    CHECK(after_redo.engine.key == 3);
    CHECK(after_redo.history_entry_id == after_new_commit.history_entry_id);
    CHECK(after_redo.project_revision == after_new_commit.project_revision);
}

TEST_CASE("No-op commit after undo preserves redo history", "[core][timeline][commit]")
{
    auto timeline = XenTimeline{EngineState{}};
    auto engine = timeline.get_state();
    engine.key = 1;
    timeline.stage(engine);
    REQUIRE(timeline.commit());
    engine.key = 2;
    timeline.stage(engine);
    REQUIRE(timeline.commit());
    REQUIRE(timeline.undo());
    auto const entry_before_no_op = timeline.get_current_entry_id();
    auto const revision_before_no_op = timeline.get_project_revision();

    timeline.stage(timeline.get_state());
    CHECK_FALSE(timeline.commit());
    CHECK(timeline.get_current_entry_id() == entry_before_no_op);
    CHECK(timeline.get_project_revision() == revision_before_no_op);
    CHECK(timeline.redo());
    CHECK(timeline.get_state().key == 2);
}

TEST_CASE("Guarded amendment retains entry identity and advances revision",
          "[core][timeline][commit]")
{
    auto timeline = XenTimeline{EngineState{}};
    auto state = timeline.get_state();
    state.key = 1;
    timeline.stage(state);
    REQUIRE(timeline.commit());

    auto const entry_id = timeline.get_current_entry_id();
    auto const revision = timeline.get_project_revision();
    state.key = 2;

    CHECK_FALSE(
        timeline.amend_current(HistoryEntryId{entry_id.value() + 1000}, state));
    CHECK(timeline.get_current_entry_id() == entry_id);
    CHECK(timeline.get_project_revision() == revision);

    REQUIRE(timeline.amend_current(entry_id, state));
    CHECK(timeline.get_state().key == 2);
    CHECK(timeline.get_current_entry_id() == entry_id);
    CHECK(timeline.get_project_revision() != revision);

    auto const amended_revision = timeline.get_project_revision();
    CHECK_FALSE(timeline.amend_current(entry_id, timeline.get_state()));
    CHECK(timeline.get_current_entry_id() == entry_id);
    CHECK(timeline.get_project_revision() == amended_revision);
}

TEST_CASE("Guarded amendment rejects stale, equal, and non-tip updates",
          "[core][timeline][commit]")
{
    auto timeline = XenTimeline{EngineState{}};
    auto state = timeline.get_state();
    state.key = 1;
    timeline.stage(state);
    REQUIRE(timeline.commit());
    auto const entry_with_key_1 = timeline.get_current_entry_id();

    state.key = 2;
    timeline.stage(state);
    REQUIRE(timeline.commit());
    REQUIRE(timeline.undo());

    auto const revision = timeline.get_project_revision();
    auto amended = timeline.get_state();
    amended.key = 3;
    CHECK_FALSE(timeline.amend_current(entry_with_key_1, amended));
    CHECK_FALSE(timeline.amend_current(HistoryEntryId{entry_with_key_1.value() + 1000},
                                       amended));
    CHECK_FALSE(timeline.amend_current(entry_with_key_1, timeline.get_state()));
    CHECK(timeline.get_state().key == 1);
    CHECK(timeline.get_current_entry_id() == entry_with_key_1);
    CHECK(timeline.get_project_revision() == revision);
    CHECK(timeline.redo());
    CHECK(timeline.get_state().key == 2);
}

TEST_CASE("Undo and redo retain entry identities and allocate fresh revisions",
          "[core][timeline][commit]")
{
    auto timeline = XenTimeline{EngineState{}};
    auto const root_entry = timeline.get_current_entry_id();
    auto state = timeline.get_state();
    state.key = 1;
    timeline.stage(state);
    REQUIRE(timeline.commit());
    auto const committed_entry = timeline.get_current_entry_id();
    auto const committed_revision = timeline.get_project_revision();

    REQUIRE(timeline.undo());
    auto const undo_revision = timeline.get_project_revision();
    CHECK(timeline.get_current_entry_id() == root_entry);
    CHECK(undo_revision != committed_revision);

    REQUIRE(timeline.redo());
    CHECK(timeline.get_current_entry_id() == committed_entry);
    CHECK(timeline.get_project_revision() != committed_revision);
    CHECK(timeline.get_project_revision() != undo_revision);
}

TEST_CASE("Unavailable history navigation preserves identity and revision",
          "[core][timeline][commit]")
{
    auto timeline = XenTimeline{EngineState{}};
    auto const entry_id = timeline.get_current_entry_id();
    auto const revision = timeline.get_project_revision();

    CHECK_FALSE(timeline.undo());
    CHECK_FALSE(timeline.redo());
    CHECK(timeline.get_current_entry_id() == entry_id);
    CHECK(timeline.get_project_revision() == revision);
}

TEST_CASE("History replacement allocates a fresh root for equal project data",
          "[core][timeline][commit]")
{
    auto timeline = XenTimeline{EngineState{}};
    auto const entry_id = timeline.get_current_entry_id();
    auto const revision = timeline.get_project_revision();

    timeline.replace_history(timeline.get_state());
    CHECK(timeline.get_current_entry_id() != entry_id);
    CHECK(timeline.get_project_revision() != revision);
    CHECK_FALSE(timeline.undo());
    CHECK_FALSE(timeline.redo());
}

TEST_CASE("Modulator and weight mutations commit immediately",
          "[core][timeline][commit]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("split 2").first == MessageLevel::Info);
    auto const before = processor.get_engine_snapshot();

    auto const [level, message] = processor.execute_command_string("set weights 0.5");
    CHECK(level == MessageLevel::Info);
    CHECK(message == "Weights Set");

    auto const after = processor.get_engine_snapshot();
    CHECK(after.history_entry_id != before.history_entry_id);
    CHECK(after.project_revision != before.project_revision);
}

TEST_CASE("Atomic mutation with later bind error does not commit",
          "[core][timeline][commit]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("split 2").first == MessageLevel::Info);
    auto const before = processor.get_engine_snapshot();

    auto const [level, message] =
        processor.execute_command_string("set weights 0.75; notARealCommand");
    CHECK(level == MessageLevel::Error);
    CHECK(message == "Command not found: notARealCommand");

    auto const after = processor.get_engine_snapshot();
    CHECK(after.history_entry_id == before.history_entry_id);
    CHECK(after.project_revision == before.project_revision);
}
