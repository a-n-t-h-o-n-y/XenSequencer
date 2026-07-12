#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <utility>

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
