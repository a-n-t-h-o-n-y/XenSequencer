#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>

#include <juce_core/juce_core.h>

#include <xen/document.hpp>
#include <xen/document_storage.hpp>
#include <xen/message_level.hpp>
#include <xen/sequencer_session.hpp>
#include <xen/serialize.hpp>
#include <xen/text_file.hpp>
#include <xen/user_directory.hpp>

using namespace xen;

namespace
{

struct SessionFixture
{
    juce::File root =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("xen-document-persistence-test", "", false);
    juce::File tunings = root.getChildFile("tunings");
    juce::File settings = root.getChildFile("workspace.json");

    SessionFixture()
    {
        REQUIRE(root.createDirectory());
        REQUIRE(tunings.createDirectory());
        REQUIRE(settings.replaceWithText(
            "{\"schema\":2,\"content_directory\":\"" +
            root.getFullPathName().toStdString() + "\",\"tuning_directory\":\"" +
            tunings.getFullPathName().toStdString() + "\"}"));
    }

    ~SessionFixture()
    {
        (void)root.deleteRecursively();
    }
};

} // namespace

TEST_CASE("Project document lifecycle tracks dirty state and file conflicts",
          "[processor][document]")
{
    auto fixture = SessionFixture{};
    auto session = SequencerSession{SubmissionEffects::FailurePoint::None,
                                    fixture.settings.getFullPathName().toStdString()};

    auto snapshot = session.project_snapshot();
    CHECK_FALSE(snapshot.document.dirty);
    CHECK_FALSE(snapshot.document.relative_path.has_value());

    REQUIRE(session
                .execute_command_string(
                    "note 4", {.selection = SelectionPath{},
                               .expected_project_revision = snapshot.project_revision})
                .status.first == MessageLevel::Info);
    snapshot = session.project_snapshot();
    CHECK(snapshot.document.dirty);
    CHECK_THROWS_AS(session.create_project(snapshot.project_revision, false),
                    DocumentError);
    CHECK(session
              .execute_command_string("project new", {.expected_project_revision =
                                                          snapshot.project_revision})
              .status.first == MessageLevel::Error);

    REQUIRE_NOTHROW(session.create_project(snapshot.project_revision, true));
    snapshot = session.project_snapshot();
    CHECK_FALSE(snapshot.document.dirty);

    REQUIRE(session
                .execute_command_string(
                    "note 7", {.selection = SelectionPath{},
                               .expected_project_revision = snapshot.project_revision})
                .status.first == MessageLevel::Info);
    snapshot = session.project_snapshot();
    auto const saved = session.save_project_as("songs/example.xenproj",
                                               snapshot.project_revision, std::nullopt);
    REQUIRE(saved.file.has_value());
    CHECK(saved.file->stem == "songs/example");
    CHECK_FALSE(saved.snapshot.document.dirty);
    CHECK(saved.snapshot.document.relative_path == "songs/example.xenproj");

    REQUIRE(session
                .execute_command_string(
                    "note 11",
                    {.selection = SelectionPath{},
                     .expected_project_revision = saved.snapshot.project_revision})
                .status.first == MessageLevel::Info);
    CHECK(session.project_snapshot().document.dirty);
    REQUIRE(session
                .execute_command_string(
                    "undo", {.expected_project_revision =
                                 session.project_snapshot().project_revision})
                .status.first == MessageLevel::Info);
    auto const reverted = session.project_snapshot();
    CHECK_FALSE(reverted.document.dirty);

    REQUIRE(fixture.root.getChildFile("songs/example.xenproj")
                .replaceWithText("externally changed"));
    auto current_file_revision = std::optional<std::string>{};
    try
    {
        (void)session.save_project(reverted.project_revision);
        FAIL("Expected an external file conflict");
    }
    catch (DocumentError const &error)
    {
        CHECK(error.code == DocumentErrorCode::FileConflict);
        REQUIRE(error.current_file_revision.has_value());
        current_file_revision = error.current_file_revision;
    }
    try
    {
        (void)session.save_project_as("songs/example.xenproj",
                                      reverted.project_revision, std::nullopt);
        FAIL("Expected create-only save to reject an existing file");
    }
    catch (DocumentError const &error)
    {
        CHECK(error.code == DocumentErrorCode::FileExists);
        REQUIRE(error.current_file_revision.has_value());
    }
    CHECK_THROWS_AS(session.save_project_as("../escape.xenproj",
                                            reverted.project_revision, std::nullopt),
                    DocumentError);
    CHECK_THROWS_AS(
        session.save_project_as(".xenproj", reverted.project_revision, std::nullopt),
        DocumentError);
    CHECK_THROWS_AS(session.save_project_as("legacy.xencomp", reverted.project_revision,
                                            std::nullopt),
                    DocumentError);
    CHECK_THROWS_AS(
        session.save_project_as("CON.xenproj", reverted.project_revision, std::nullopt),
        DocumentError);

    auto const overwritten = session.save_project_as(
        "songs/example.xenproj", reverted.project_revision, current_file_revision);
    REQUIRE(overwritten.file.has_value());
    CHECK_FALSE(overwritten.snapshot.document.dirty);
}

TEST_CASE("Cell documents export selected cells and import as sequences",
          "[processor][document][cell]")
{
    auto fixture = SessionFixture{};
    auto session = SequencerSession{SubmissionEffects::FailurePoint::None,
                                    fixture.settings.getFullPathName().toStdString()};
    auto snapshot = session.project_snapshot();
    auto const exported =
        session.save_cell("assets/example.xencell", snapshot.project_revision,
                          CompositionCursor{}, SelectionPath{}, std::nullopt);
    REQUIRE(exported.file.has_value());
    CHECK(exported.file->file_revision.starts_with("sha256:"));

    auto const imported = session.import_cell(
        "assets/example.xencell", snapshot.project_revision,
        CompositionCursor{.row_coordinate = 1, .column_coordinate = 1});
    CHECK(imported.snapshot.project.sequence_bank.sequences.size() == 2);
    CHECK(imported.snapshot.document.dirty);
    REQUIRE(imported.suggested_selection.has_value());
    CHECK(imported.suggested_selection->path.empty());
}

TEST_CASE("Dirty projects produce explicit recoverable snapshots",
          "[processor][document][recovery]")
{
    auto fixture = SessionFixture{};
    auto const binding = InstanceBinding{
        .session_id = "document-recovery-" + juce::Uuid{}.toString().toStdString(),
        .instance_id = "instance-test",
        .channel_id = DEFAULT_CHANNEL_ID,
    };
    auto persisted_before_edit = PersistedProcessorState{};
    {
        auto session =
            SequencerSession{SubmissionEffects::FailurePoint::None,
                             fixture.settings.getFullPathName().toStdString()};
        session.replace_instance_binding(binding);
        auto const clean = session.project_snapshot();
        persisted_before_edit = PersistedProcessorState{
            .binding = binding,
            .project = clean.project,
            .saved_project_revision = clean.project_revision,
            .saved_state_revision = clean.state_revision,
            .document = clean.document,
        };
        REQUIRE(session
                    .execute_command_string(
                        "note 9", {.selection = SelectionPath{},
                                   .expected_project_revision = clean.project_revision})
                    .status.first == MessageLevel::Info);
        session.perform_recovery_maintenance(10'000, true);
        CHECK_FALSE(session.project_snapshot().recovery.has_value());
        auto const autosaved_revision = session.project_snapshot().state_revision;
        session.perform_recovery_maintenance(11'000, true);
        CHECK(session.project_snapshot().state_revision == autosaved_revision);
    }

    auto const recovery_file =
        get_user_settings_directory()
            .getChildFile("recovery")
            .getChildFile(text_revision(binding.session_id).substr(7) + ".xenrecovery");
    auto const interrupted_recovery =
        recovery_file.getSiblingFile(recovery_file.getFileName() + ".xen-tmp.test");
    REQUIRE(recovery_file.moveFileTo(interrupted_recovery));

    {
        auto session_without_host_state =
            SequencerSession{SubmissionEffects::FailurePoint::None,
                             fixture.settings.getFullPathName().toStdString()};
        session_without_host_state.replace_instance_binding(binding);
        CHECK(session_without_host_state.project_snapshot().recovery.has_value());
        CHECK(recovery_file.existsAsFile());
        CHECK_FALSE(interrupted_recovery.exists());
    }

    auto restored_session =
        SequencerSession{SubmissionEffects::FailurePoint::None,
                         fixture.settings.getFullPathName().toStdString()};
    restored_session.restore_persisted_state(persisted_before_edit);
    auto snapshot = restored_session.project_snapshot();
    REQUIRE(snapshot.recovery.has_value());
    auto const restored = restored_session.restore_recovery(
        snapshot.recovery->revision, snapshot.project_revision, false);
    CHECK(restored.snapshot.document.dirty);
    CHECK_FALSE(restored.snapshot.recovery.has_value());
    restored_session.perform_recovery_maintenance(20'000, true);
    CHECK_FALSE(restored_session.project_snapshot().recovery.has_value());

    auto third_session =
        SequencerSession{SubmissionEffects::FailurePoint::None,
                         fixture.settings.getFullPathName().toStdString()};
    third_session.restore_persisted_state(persisted_before_edit);
    auto const pending = third_session.project_snapshot().recovery;
    REQUIRE(pending.has_value());
    CHECK(third_session
              .execute_command_string(
                  "note 3", {.selection = SelectionPath{},
                             .expected_project_revision =
                                 third_session.project_snapshot().project_revision})
              .status.first == MessageLevel::Error);
    third_session.perform_recovery_maintenance(30'000, true);
    REQUIRE(third_session.project_snapshot().recovery.has_value());
    CHECK(third_session.project_snapshot().recovery->revision == pending->revision);
    REQUIRE_NOTHROW(third_session.discard_recovery(pending->revision));
    CHECK_FALSE(third_session.project_snapshot().recovery.has_value());
}

TEST_CASE("Interrupted document replacements recover without losing canonical data",
          "[processor][document][recovery]")
{
    auto fixture = SessionFixture{};
    auto const backup =
        fixture.root.getChildFile("interrupted.xenproj.xen-backup.test");
    REQUIRE(backup.replaceWithText(serialize_project(ProjectState{})));
    auto const invalid_temporary =
        fixture.root.getChildFile("invalid.xenproj.xen-tmp.test");
    REQUIRE(invalid_temporary.replaceWithText("partial"));

    REQUIRE_NOTHROW(
        recover_content_directory(fixture.root.getFullPathName().toStdString()));
    CHECK(fixture.root.getChildFile("interrupted.xenproj").existsAsFile());
    CHECK_FALSE(backup.exists());
    CHECK_FALSE(invalid_temporary.exists());
}
