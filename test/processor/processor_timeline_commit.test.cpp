#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <xen/chord.hpp>
#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/sequencer_session.hpp>

using namespace xen;

namespace
{

auto current_context(SequencerSession const &session,
                     std::optional<SelectionPath> selection = std::nullopt)
    -> CommandContext
{
    return {
        .selection = std::move(selection),
        .expected_project_revision = session.project_snapshot().project_revision,
    };
}

} // namespace

TEST_CASE("Mutating commands advance history identity and project revision",
          "[processor][timeline][commit]")
{
    auto session = SequencerSession{};
    auto const initial = session.project_snapshot();

    auto const result =
        session.execute_command_string("set key 12", current_context(session));
    CHECK(result.status.first == MessageLevel::Info);

    auto const after = session.project_snapshot();
    CHECK(after.history_entry_id != initial.history_entry_id);
    CHECK(after.project_revision != initial.project_revision);
    CHECK(after.project.composition.columns.at(0).pitch.transposition == 12);
}

TEST_CASE("Project new installs a fresh root and clears project command sessions",
          "[processor][timeline][replace]")
{
    auto session = SequencerSession{};
    session.replace_library(
        ContentLibrary{.chords = {
                           Chord{.name = "major", .intervals = {0, 4, 7}},
                       }});
    REQUIRE(
        session
            .execute_command_string("note 1", current_context(session, SelectionPath{}))
            .status.first == MessageLevel::Info);
    REQUIRE(
        session
            .execute_command_string("note 2", current_context(session, SelectionPath{}))
            .status.first == MessageLevel::Info);
    REQUIRE(session
                .execute_command_string("chord major 0",
                                        current_context(session, SelectionPath{}))
                .status.first == MessageLevel::Info);
    REQUIRE_FALSE(session.command_session().repeat_chain.empty());
    REQUIRE(session.command_session().transform_cycle.has_value());
    auto const copied_element =
        selected_sequence(session.project_snapshot().project, {}).elements.at(0);
    REQUIRE(session
                .execute_command_string(
                    "copy", current_context(session, select_element_in_cell({}, 0)))
                .status.first == MessageLevel::Info);

    auto const resources = session.library_snapshot();
    auto const edited = session.project_snapshot();
    auto const result =
        session.execute_command_string("project new", current_context(session));

    REQUIRE(result.status.first == MessageLevel::Info);
    CHECK(result.status.second == "New Project");
    REQUIRE(result.suggested_selection.has_value());
    CHECK(result.suggested_selection->path.empty());
    auto const replaced = session.project_snapshot();
    CHECK(replaced.project == ProjectState{});
    CHECK(replaced.history_entry_id != edited.history_entry_id);
    CHECK(replaced.project_revision != edited.project_revision);
    CHECK(session.command_session().repeat_chain.empty());
    CHECK_FALSE(session.command_session().transform_cycle.has_value());
    auto const surviving_resources = session.library_snapshot();
    CHECK(surviving_resources.workspace == resources.workspace);
    CHECK(surviving_resources.library_revision == resources.library_revision);
    CHECK(surviving_resources.library.scales.size() == resources.library.scales.size());
    REQUIRE(surviving_resources.library.chords.size() == 1);
    CHECK(surviving_resources.library.chords.front().name == "major");
    CHECK(surviving_resources.library.chords.front().intervals ==
          std::vector<int>{0, 4, 7});

    auto const undo = session.execute_command_string("undo", current_context(session));
    CHECK(undo.status.second == "Nothing to undo.");
    auto const redo = session.execute_command_string("redo", current_context(session));
    CHECK(redo.status.second == "Nothing to redo.");

    REQUIRE(
        session
            .execute_command_string("paste", current_context(session, SelectionPath{}))
            .status.first == MessageLevel::Info);
    auto const pasted = selected_sequence(session.project_snapshot().project, {});
    REQUIRE(pasted.elements.size() == 1);
    CHECK(pasted.elements.front() == copied_element);
}

TEST_CASE("Project new refreshes an already-default history root",
          "[processor][timeline][replace]")
{
    auto session = SequencerSession{};
    auto const before = session.project_snapshot();

    REQUIRE(session.execute_command_string("project new", current_context(session))
                .status.first == MessageLevel::Info);

    auto const after = session.project_snapshot();
    CHECK(after.project == before.project);
    CHECK(after.history_entry_id != before.history_entry_id);
    CHECK(after.project_revision != before.project_revision);
}

TEST_CASE("Project history replacement commands must be submitted alone",
          "[processor][timeline][replace]")
{
    auto session = SequencerSession{};
    auto const before = session.project_snapshot();

    auto const result = session.execute_command_string("project new; version",
                                                       current_context(session));

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second ==
          "Project history replacement commands must be submitted alone.");
    auto const after = session.project_snapshot();
    CHECK(after.project == before.project);
    CHECK(after.history_entry_id == before.history_entry_id);
    CHECK(after.project_revision == before.project_revision);
}

TEST_CASE("Project history replacement is rejected during previews",
          "[processor][timeline][replace][preview]")
{
    auto session = SequencerSession{};
    auto const before = session.project_snapshot();
    auto const started = session.begin_preview(before.project_revision);
    REQUIRE(started.preview_id.has_value());

    auto const result =
        session.execute_command_string("project new", current_context(session));

    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second.find("project preview is active") != std::string::npos);
    auto const after = session.project_snapshot();
    CHECK(after.project == before.project);
    CHECK(after.history_entry_id == before.history_entry_id);
    CHECK(after.project_revision == before.project_revision);
    CHECK(after.preview_active);
}

TEST_CASE("Preview updates stage repeatedly and commit one undo entry",
          "[processor][timeline][preview]")
{
    auto session = SequencerSession{};
    auto const initial = session.project_snapshot();
    auto const started = session.begin_preview(initial.project_revision);
    REQUIRE(started.preview_id.has_value());
    CHECK(session.project_snapshot().preview_active);

    auto execute_preview = [&](std::string const &command) {
        return session.execute_command_string(
            command,
            {.expected_project_revision = session.project_snapshot().project_revision,
             .preview_id = started.preview_id});
    };
    REQUIRE(execute_preview("set key 3").status.first == MessageLevel::Info);
    auto const first = session.project_snapshot();
    CHECK(first.history_entry_id == initial.history_entry_id);
    CHECK(first.project_revision != initial.project_revision);
    CHECK(first.project.composition.columns.at(0).pitch.transposition == 3);
    CHECK(session.persistent_project_snapshot().project == initial.project);

    REQUIRE(execute_preview("set key 9").status.first == MessageLevel::Info);
    auto const staged = session.project_snapshot();
    CHECK(staged.history_entry_id == initial.history_entry_id);
    CHECK(staged.project.composition.columns.at(0).pitch.transposition == 9);

    auto const committed =
        session.commit_preview(*started.preview_id, staged.project_revision);
    REQUIRE(committed.status.first == MessageLevel::Info);
    auto const final = session.project_snapshot();
    CHECK_FALSE(final.preview_active);
    CHECK(final.history_entry_id != initial.history_entry_id);
    CHECK(session.persistent_project_snapshot().project == final.project);

    auto const undone = session.execute_command_string(
        "undo", {.expected_project_revision = final.project_revision});
    REQUIRE(undone.status.first == MessageLevel::Info);
    CHECK(session.project_snapshot().project == initial.project);
}

TEST_CASE("Previews allow copy buffer reads but reject writes",
          "[processor][timeline][preview][copy-buffer]")
{
    auto session = SequencerSession{};
    REQUIRE(
        session
            .execute_command_string("note 5", current_context(session, SelectionPath{}))
            .status.first == MessageLevel::Info);
    REQUIRE(session
                .execute_command_string(
                    "copy", current_context(session, select_element_in_cell({}, 0)))
                .status.first == MessageLevel::Info);

    auto const before = session.project_snapshot();
    auto const started = session.begin_preview(before.project_revision);
    REQUIRE(started.preview_id.has_value());
    auto preview_context = [&](SelectionPath selection) {
        return CommandContext{
            .selection = std::move(selection),
            .expected_project_revision = session.project_snapshot().project_revision,
            .preview_id = started.preview_id,
        };
    };

    auto const cut = session.execute_command_string(
        "cut", preview_context(select_element_in_cell({}, 0)));
    CHECK(cut.status.first == MessageLevel::Error);
    CHECK(cut.status.second ==
          "Project previews accept only reversible project-edit commands.");
    CHECK(session.project_snapshot().project == before.project);

    auto const paste =
        session.execute_command_string("paste", preview_context(SelectionPath{}));
    REQUIRE(paste.status.first == MessageLevel::Info);
    CHECK(selected_sequence(session.project_snapshot().project, {}).elements.size() ==
          2);

    REQUIRE(session
                .cancel_preview(*started.preview_id,
                                session.project_snapshot().project_revision)
                .status.first == MessageLevel::Info);
}

TEST_CASE("Preview cancellation restores baseline and blocks ordinary edits",
          "[processor][timeline][preview]")
{
    auto session = SequencerSession{};
    REQUIRE(session.execute_command_string("set key 4", current_context(session))
                .status.first == MessageLevel::Info);
    REQUIRE(session.execute_command_string("set key 8", current_context(session))
                .status.first == MessageLevel::Info);
    REQUIRE(
        session.execute_command_string("undo", current_context(session)).status.first ==
        MessageLevel::Info);
    auto const baseline = session.project_snapshot();

    auto const started = session.begin_preview(baseline.project_revision);
    REQUIRE(started.preview_id.has_value());
    REQUIRE(session
                .execute_command_string(
                    "set key 6", {.expected_project_revision =
                                      session.project_snapshot().project_revision,
                                  .preview_id = started.preview_id})
                .status.first == MessageLevel::Info);

    auto const rejected =
        session.execute_command_string("set key 7", current_context(session));
    CHECK(rejected.status.first == MessageLevel::Error);
    CHECK(rejected.status.second.find("project preview is active") !=
          std::string::npos);
    auto const undo_rejected =
        session.execute_command_string("undo", current_context(session));
    CHECK(undo_rejected.status.first == MessageLevel::Error);

    auto const cancelled = session.cancel_preview(
        *started.preview_id, session.project_snapshot().project_revision);
    REQUIRE(cancelled.status.first == MessageLevel::Info);
    CHECK(session.project_snapshot().project == baseline.project);
    CHECK(session.project_snapshot().history_entry_id == baseline.history_entry_id);

    REQUIRE(
        session.execute_command_string("redo", current_context(session)).status.first ==
        MessageLevel::Info);
    CHECK(session.project_snapshot()
              .project.composition.columns.at(0)
              .pitch.transposition == 8);
}

TEST_CASE("Empty previews do not create history entries",
          "[processor][timeline][preview]")
{
    auto session = SequencerSession{};
    auto const baseline = session.project_snapshot();
    auto const started = session.begin_preview(baseline.project_revision);
    REQUIRE(started.preview_id.has_value());

    auto const stale = session.commit_preview(*started.preview_id, ProjectRevision{});
    CHECK(stale.status.first == MessageLevel::Error);
    CHECK(session.project_snapshot().preview_active);

    auto const committed = session.commit_preview(
        *started.preview_id, session.project_snapshot().project_revision);
    REQUIRE(committed.status.first == MessageLevel::Info);
    CHECK(session.project_snapshot().history_entry_id == baseline.history_entry_id);
    CHECK(session.project_snapshot().project_revision == baseline.project_revision);
    CHECK_FALSE(session.project_snapshot().preview_active);
}
