#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <utility>

#include <xen/chord.hpp>
#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/sequencer_session.hpp>
#include <xen/submission_effects.hpp>

using namespace xen;

TEST_CASE("Effect failures leave backend state unchanged and report rollback failures",
          "[processor][command][transaction][effects]")
{
    auto const directory =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("xen-transaction-test", "", false);
    REQUIRE(directory.createDirectory());
    auto const tunings = directory.getChildFile("tunings");
    REQUIRE(tunings.createDirectory());
    auto const settings_file = directory.getChildFile("workspace.json");
    REQUIRE(settings_file.replaceWithText(
        "{\"schema\":2,\"content_directory\":\"" +
        directory.getFullPathName().toStdString() + "\",\"tuning_directory\":\"" +
        tunings.getFullPathName().toStdString() + "\"}"));
    REQUIRE(directory.getChildFile("effect-test.xencell").replaceWithText("baseline"));

    for (auto const failure : {SubmissionEffects::FailurePoint::Prepare,
                               SubmissionEffects::FailurePoint::Apply,
                               SubmissionEffects::FailurePoint::ApplyAndRollback})
    {
        auto session =
            SequencerSession{failure, settings_file.getFullPathName().toStdString()};
        auto const before = session.project_snapshot();
        auto const result = session.execute_command_string(
            "save cell effect-test",
            {.expected_project_revision = before.project_revision});

        CHECK(result.status.first == MessageLevel::Error);
        CHECK(session.project_snapshot().project_revision == before.project_revision);
        CHECK(session.project_snapshot().project == before.project);
        if (failure == SubmissionEffects::FailurePoint::ApplyAndRollback)
        {
            CHECK(result.status.second.find("rollback failed for:") !=
                  std::string::npos);
        }
    }

    CHECK(directory.deleteRecursively());
}

TEST_CASE("Project open and save use document-boundary history semantics",
          "[processor][command][project][effects]")
{
    auto const directory =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("xen-project-document-test", "", false);
    REQUIRE(directory.createDirectory());
    auto const tunings = directory.getChildFile("tunings");
    REQUIRE(tunings.createDirectory());
    auto const settings_file = directory.getChildFile("workspace.json");
    REQUIRE(settings_file.replaceWithText(
        "{\"schema\":2,\"content_directory\":\"" +
        directory.getFullPathName().toStdString() + "\",\"tuning_directory\":\"" +
        tunings.getFullPathName().toStdString() + "\"}"));

    {
        auto session = SequencerSession{
            SubmissionEffects::FailurePoint::None,
            settings_file.getFullPathName().toStdString(),
        };
        session.replace_library(
            ContentLibrary{.chords = {
                               Chord{.name = "major", .intervals = {0, 4, 7}},
                           }});
        auto execute = [&](std::string const &command,
                           std::optional<SelectionPath> selection = std::nullopt) {
            return session.execute_command_string(
                command, {.selection = std::move(selection),
                          .expected_project_revision =
                              session.project_snapshot().project_revision});
        };
        REQUIRE(execute("note 1", SelectionPath{}).status.first == MessageLevel::Info);
        REQUIRE(execute("note 2", SelectionPath{}).status.first == MessageLevel::Info);
        REQUIRE(execute("chord major 0", SelectionPath{}).status.first ==
                MessageLevel::Info);
        REQUIRE(session.command_session().transform_cycle.has_value());
        REQUIRE_FALSE(session.command_session().repeat_chain.empty());

        auto const before_save = session.project_snapshot();
        REQUIRE(execute("project save song").status.first == MessageLevel::Info);
        auto const after_save = session.project_snapshot();
        CHECK(after_save.project == before_save.project);
        CHECK(after_save.history_entry_id == before_save.history_entry_id);
        CHECK(after_save.project_revision == before_save.project_revision);
        CHECK(directory.getChildFile("song.xencomp").existsAsFile());

        auto const before_failure = session.project_snapshot();
        auto const repeat_command =
            session.command_session().repeat_chain.front().canonical_segment;
        auto const transform = *session.command_session().transform_cycle;
        auto const missing = execute("project open missing");
        CHECK(missing.status.first == MessageLevel::Error);
        CHECK(session.project_snapshot().project == before_failure.project);
        CHECK(session.project_snapshot().history_entry_id ==
              before_failure.history_entry_id);
        CHECK(session.project_snapshot().project_revision ==
              before_failure.project_revision);
        REQUIRE_FALSE(session.command_session().repeat_chain.empty());
        CHECK(session.command_session().repeat_chain.front().canonical_segment ==
              repeat_command);
        REQUIRE(session.command_session().transform_cycle.has_value());
        CHECK(session.command_session().transform_cycle->history_entry_id ==
              transform.history_entry_id);
        CHECK(session.command_session().transform_cycle->previous_chord_name ==
              transform.previous_chord_name);

        REQUIRE(directory.getChildFile("malformed.xencomp")
                    .replaceWithText("not valid project json"));
        auto const malformed = execute("project open malformed");
        CHECK(malformed.status.first == MessageLevel::Error);
        CHECK(session.project_snapshot().history_entry_id ==
              before_failure.history_entry_id);
        CHECK(session.project_snapshot().project_revision ==
              before_failure.project_revision);
        REQUIRE(session.command_session().transform_cycle.has_value());

        auto const copied_element =
            selected_sequence(session.project_snapshot().project, {}).elements.at(0);
        REQUIRE(execute("copy", select_element_in_cell({}, 0)).status.first ==
                MessageLevel::Info);
        auto const opened = execute("project open song");
        REQUIRE(opened.status.first == MessageLevel::Info);
        auto const after_open = session.project_snapshot();
        CHECK(after_open.project == before_save.project);
        CHECK(after_open.history_entry_id != before_failure.history_entry_id);
        CHECK(after_open.project_revision != before_failure.project_revision);
        CHECK(session.command_session().repeat_chain.empty());
        CHECK_FALSE(session.command_session().transform_cycle.has_value());
        CHECK(execute("undo").status.second == "Nothing to undo.");
        CHECK(execute("redo").status.second == "Nothing to redo.");

        auto const element_count =
            selected_sequence(session.project_snapshot().project, {}).elements.size();
        REQUIRE(execute("paste", SelectionPath{}).status.first == MessageLevel::Info);
        auto const pasted = selected_sequence(session.project_snapshot().project, {});
        REQUIRE(pasted.elements.size() == element_count + 1);
        CHECK(pasted.elements.back() == copied_element);
    }

    CHECK(directory.deleteRecursively());
}
