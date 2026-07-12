#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>

#include <xen/selection.hpp>
#include <xen/sequencer_session.hpp>

using namespace xen;

namespace
{

auto execute(SequencerSession &session, std::string const &command,
             std::optional<SelectionPath> selection = std::nullopt)
    -> CommandApplicationResult
{
    return session.execute_command_string(
        command,
        {.selection = std::move(selection),
         .expected_project_revision = session.project_snapshot().project_revision});
}

} // namespace

TEST_CASE("Processor requires current revisions only for project-aware submissions",
          "[processor][commands][context]")
{
    auto session = SequencerSession{};
    auto const before = session.project_snapshot();

    auto const version_result =
        session.execute_command_string("version", CommandContext{});
    CHECK(version_result.status.first == MessageLevel::Info);

    auto const missing_result =
        session.execute_command_string("set key 9", CommandContext{});
    CHECK(missing_result.status.first == MessageLevel::Error);
    CHECK(missing_result.status.second == "expected project revision is required");
    CHECK(session.project_snapshot().project == before.project);
}

TEST_CASE("Processor rejects stale revisions before resolving selection",
          "[processor][commands][context]")
{
    auto session = SequencerSession{};
    auto const stale_revision = session.project_snapshot().project_revision;
    REQUIRE(execute(session, "set key 3").status.first == MessageLevel::Info);
    auto const before_rejection = session.project_snapshot();

    auto const result = session.execute_command_string(
        "set key 9", {.expected_project_revision = stale_revision});

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second ==
          "stale project revision: expected " + std::to_string(stale_revision.value()) +
              ", current " + std::to_string(before_rejection.project_revision.value()));
}

TEST_CASE("Processor rejects missing and invalid targeted selections",
          "[processor][commands][selection]")
{
    auto session = SequencerSession{};

    auto const missing = execute(session, "delete");
    CHECK(missing.status.second == "selection is required");

    auto const invalid = session.execute_command_string(
        "delete",
        {.selection = select_element_in_cell({}, 9),
         .expected_project_revision = session.project_snapshot().project_revision});
    CHECK(invalid.status.first == MessageLevel::Error);
    CHECK(invalid.status.second == "selection path does not resolve");
}

TEST_CASE("Processor rejects wrong-kind targets", "[processor][commands][selection]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "note 5", SelectionPath{}).status.first ==
            MessageLevel::Info);
    auto const result =
        execute(session, "set weight 0.5", select_element_in_cell({}, 0));
    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second == "selection must resolve to a cell");
}

TEST_CASE("Processor distinguishes invalid composition cursors from selections",
          "[processor][commands][context]")
{
    auto session = SequencerSession{};
    auto const result = session.execute_command_string(
        "delete",
        {.selection = SelectionPath{},
         .expected_project_revision = session.project_snapshot().project_revision,
         .cursor = CompositionCursor{.sequence_id = std::nullopt}});

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second == "active composition cursor does not resolve");
}

TEST_CASE("Processor applies selected-sequence commands to active composition target",
          "[processor][commands][context]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "composition cell assign 0 1 Verse").status.first ==
            MessageLevel::Info);

    auto const before = session.project_snapshot();
    auto const sequence_id =
        sequence_reference_at(before.project.composition, 0, 1).value();
    REQUIRE(sequence_id != sequence_reference_at(before.project.composition, 0, 0));

    auto const result = session.execute_command_string(
        "note 7", {
                      .selection = SelectionPath{},
                      .expected_project_revision = before.project_revision,
                      .cursor =
                          xen::CompositionCursor{
                              .row_coordinate = 0,
                              .column_coordinate = 1,
                              .sequence_id = sequence_id,
                          },
                  });

    REQUIRE(result.status.first == MessageLevel::Info);
    auto const &project = session.project_snapshot().project;
    CHECK(selected_sequence(project, xen::CompositionCursor{}).elements.empty());

    auto const *active_sequence = find_sequence(project.sequence_bank, sequence_id);
    REQUIRE(active_sequence != nullptr);
    REQUIRE(active_sequence->elements.size() == 1);
    auto const &note = std::get<sequence::Note>(active_sequence->elements[0]);
    CHECK(note.pitch == 7);
}

TEST_CASE("Sequence clear empties shared contents without changing project metadata",
          "[processor][commands][sequence][history]")
{
    auto session = SequencerSession{};
    auto project = ProjectState{};
    auto &entry = project.sequence_bank.sequences.front();
    entry.name = "Shared";
    entry.cell.weight = 0.375f;
    entry.cell.elements = {sequence::Note{7, 0.6f, 0.2f, 0.8f}};
    assign_sequence_reference(project.composition, 0, 1, entry.id);
    project.composition.rows.at(0).name = "Lead";
    project.composition.rows.at(0).channel_id = "lead-channel";
    project.composition.columns.at(0).duration = {7, 8};
    project.composition.columns.at(0).pitch.transposition = 5;
    set_loop_end(project.composition, 1);
    session.replace_project_history(project);

    auto const before = session.project_snapshot();
    auto const cursor = CompositionCursor{
        .row_coordinate = 0, .column_coordinate = 0, .sequence_id = entry.id};
    auto const cleared = session.execute_command_string(
        "sequence clear",
        {.expected_project_revision = before.project_revision, .cursor = cursor});

    REQUIRE(cleared.status.first == MessageLevel::Info);
    auto const after = session.project_snapshot();
    REQUIRE(after.project.sequence_bank.sequences.size() == 1);
    auto const &cleared_entry = after.project.sequence_bank.sequences.front();
    CHECK(cleared_entry.id == entry.id);
    CHECK(cleared_entry.name == entry.name);
    CHECK(cleared_entry.cell.weight == entry.cell.weight);
    CHECK(cleared_entry.cell.elements.empty());
    CHECK(after.project.sequence_bank.next_id == before.project.sequence_bank.next_id);
    CHECK(after.project.composition == before.project.composition);
    CHECK(sequence_reference_at(after.project.composition, 0, 0) == entry.id);
    CHECK(sequence_reference_at(after.project.composition, 0, 1) == entry.id);
    CHECK(after.history_entry_id != before.history_entry_id);

    auto const undo = session.execute_command_string(
        "undo", {.expected_project_revision = after.project_revision});
    REQUIRE(undo.status.first == MessageLevel::Info);
    CHECK(session.project_snapshot().project == before.project);
}

TEST_CASE("No-op sequence clear preserves redo and history identity",
          "[processor][commands][sequence][history]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "set key 4").status.first == MessageLevel::Info);
    REQUIRE(execute(session, "undo").status.first == MessageLevel::Info);
    auto const before = session.project_snapshot();

    auto const cleared = execute(session, "sequence clear");
    REQUIRE(cleared.status.first == MessageLevel::Info);
    auto const after = session.project_snapshot();
    CHECK(after.project == before.project);
    CHECK(after.history_entry_id == before.history_entry_id);
    CHECK(after.project_revision == before.project_revision);

    REQUIRE(execute(session, "redo").status.first == MessageLevel::Info);
    CHECK(session.project_snapshot()
              .project.composition.columns.at(0)
              .pitch.transposition == 4);
}

TEST_CASE("Sequence clear rejects an unassigned composition cursor",
          "[processor][commands][sequence]")
{
    auto session = SequencerSession{};
    REQUIRE(execute(session, "composition cell unassign 0 0").status.first ==
            MessageLevel::Info);
    auto const before = session.project_snapshot();

    auto const result = session.execute_command_string(
        "sequence clear", {.expected_project_revision = before.project_revision,
                           .cursor = CompositionCursor{.sequence_id = std::nullopt}});

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second == "Active composition placement is empty.");
    auto const after = session.project_snapshot();
    CHECK(after.project == before.project);
    CHECK(after.history_entry_id == before.history_entry_id);
    CHECK(after.project_revision == before.project_revision);
}

TEST_CASE("Composition cell unassign preserves shared sequences and is undoable",
          "[processor][commands][composition][history]")
{
    auto session = SequencerSession{};
    auto project = ProjectState{};
    project.sequence_bank.sequences.front().name = "Shared";
    project.sequence_bank.sequences.front().cell.elements = {
        sequence::Note{3, 0.5f, 0.f, 1.f},
    };
    assign_sequence_reference(project.composition, 0, 1, DEFAULT_SEQUENCE_ID);
    session.replace_project_history(project);
    auto const before = session.project_snapshot();

    REQUIRE(execute(session, "composition cell unassign 0 0").status.first ==
            MessageLevel::Info);
    auto const unassigned = session.project_snapshot();
    CHECK(unassigned.project.sequence_bank == before.project.sequence_bank);
    CHECK_FALSE(
        sequence_reference_at(unassigned.project.composition, 0, 0).has_value());
    CHECK(sequence_reference_at(unassigned.project.composition, 0, 1) ==
          DEFAULT_SEQUENCE_ID);
    CHECK(unassigned.project.composition.rows == before.project.composition.rows);
    CHECK(unassigned.project.composition.columns == before.project.composition.columns);

    REQUIRE(execute(session, "undo").status.first == MessageLevel::Info);
    CHECK(session.project_snapshot().project == before.project);

    REQUIRE(execute(session, "composition cell unassign 0 0").status.first ==
            MessageLevel::Info);
    auto const once = session.project_snapshot();
    REQUIRE(execute(session, "composition cell unassign 0 0").status.first ==
            MessageLevel::Info);
    auto const twice = session.project_snapshot();
    CHECK(twice.project == once.project);
    CHECK(twice.history_entry_id == once.history_entry_id);
    CHECK(twice.project_revision == once.project_revision);
}

TEST_CASE("Processor reports unchanged-selection suggestions for transforms",
          "[processor][commands][selection]")
{
    auto session = SequencerSession{};
    auto const selection = SelectionPath{};

    REQUIRE(execute(session, "note 5", selection).status.first == MessageLevel::Info);
    auto const result = execute(session, "set velocity 0.5", selection);
    CHECK(result.status.first == MessageLevel::Info);
    REQUIRE(result.suggested_selection.has_value());
    CHECK(*result.suggested_selection == selection);
}

TEST_CASE("Removed navigation and input-mode commands are unknown",
          "[processor][commands][selection]")
{
    auto session = SequencerSession{};

    auto const move = session.execute_command_string("move right", CommandContext{});
    CHECK(move.status.first == MessageLevel::Error);
    CHECK(move.status.second == "Command not found: move");

    auto const input_mode =
        session.execute_command_string("inputMode gate", CommandContext{});
    CHECK(input_mode.status.first == MessageLevel::Error);
    CHECK(input_mode.status.second == "Command not found: inputMode");

    auto const reset = session.execute_command_string("reset", CommandContext{});
    CHECK(reset.status.first == MessageLevel::Error);
    CHECK(reset.status.second == "Command not found: reset");
}
