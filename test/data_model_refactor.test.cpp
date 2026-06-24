#include <cmath>
#include <limits>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <sequence/sequence.hpp>

#include <xen/project_validation.hpp>
#include <xen/scale.hpp>
#include <xen/sequencer_session.hpp>
#include <xen/serialize.hpp>
#include <xen/workspace_settings.hpp>

using namespace xen;

TEST_CASE("Project validation covers scalar and recursive invariants",
          "[data-model][validation]")
{
    auto project = ProjectState{};
    CHECK_NOTHROW(validate(project));

    project.measure.cell.weight = 0.f;
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.measure.cell.elements = {
        sequence::Sequence{
            .cells =
                {
                    sequence::Cell{
                        .elements = {sequence::Note{
                            .velocity = std::numeric_limits<float>::quiet_NaN()}},
                        .weight = 1.f,
                    },
                }},
    };
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.measure.time_signature = {65, 1};
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.pitch.tuning.definition.intervals = {0.f, 200.f, 100.f};
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.pitch.base_frequency = 0.f;
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.pitch.transposition = 128;
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    auto timeline = XenTimeline{ProjectState{}};
    CHECK_THROWS_AS(timeline.commit(project), std::invalid_argument);
}

TEST_CASE("Project schema 1 rejects old flat state", "[data-model][serialize]")
{
    auto const project = ProjectState{};
    auto const encoded = nlohmann::json::parse(serialize_project(project));
    CHECK(encoded.at("schema") == 1);
    CHECK(encoded.at("project").contains("pitch"));
    CHECK_FALSE(encoded.at("project").contains("tuning"));

    auto const old = nlohmann::json{
        {"measure", encoded.at("project").at("measure")},
        {"tuning", encoded.at("project").at("pitch").at("tuning")},
    };
    CHECK_THROWS(deserialize_project(old.dump()));
}

TEST_CASE("Scale library requires unique stable IDs", "[data-model][scale]")
{
    auto const valid = R"(
scales:
  - id: major
    name: Major
    tuning_length: 12
    intervals: [2, 2, 1, 2, 2, 2, 1]
)";
    auto const empty = "scales: []\n";
    auto const scales = load_scales(valid, empty);
    REQUIRE(scales.size() == 1);
    CHECK(scales.front().id == "major");

    auto const duplicate = R"(
scales:
  - id: major
    name: Other
    tuning_length: 12
    intervals: [2, 2, 1, 2, 2, 2, 1]
)";
    CHECK_THROWS_AS(load_scales(valid, duplicate), std::invalid_argument);

    auto const missing_id = R"(
scales:
  - name: Major
    tuning_length: 12
    intervals: [2, 2, 1, 2, 2, 2, 1]
)";
    CHECK_THROWS(load_scales(missing_id, empty));
}

TEST_CASE("Scale selection uses source IDs and mode shifts preserve provenance",
          "[data-model][scale]")
{
    auto session = SequencerSession{};
    auto make_scale = [](std::string name) {
        return Scale{
            .name = std::move(name),
            .tuning_length = 12,
            .intervals = {2, 2, 1, 2, 2, 2, 1},
            .mode = 1,
        };
    };
    session.replace_library(ContentLibrary{
        .scales = {
            LibraryScale{.id = "major", .definition = make_scale("major")},
            LibraryScale{.id = "other", .definition = make_scale("other")},
        }});

    auto context = CommandContext{
        .expected_project_revision = session.project_snapshot().project_revision,
    };
    REQUIRE(session.execute_command_string("set scale major", context).status.first ==
            MessageLevel::Info);
    auto snapshot = session.project_snapshot();
    REQUIRE(snapshot.project.pitch.scale.has_value());
    CHECK(snapshot.project.pitch.scale->source_id == "major");

    context.expected_project_revision = snapshot.project_revision;
    REQUIRE(session.execute_command_string("shift scaleMode 1", context).status.first ==
            MessageLevel::Info);
    snapshot = session.project_snapshot();
    REQUIRE(snapshot.project.pitch.scale.has_value());
    CHECK(snapshot.project.pitch.scale->source_id == "major");

    context.expected_project_revision = snapshot.project_revision;
    REQUIRE(session.execute_command_string("shift scale 1", context).status.first ==
            MessageLevel::Info);
    snapshot = session.project_snapshot();
    REQUIRE(snapshot.project.pitch.scale.has_value());
    CHECK(snapshot.project.pitch.scale->source_id == "other");

    context.expected_project_revision = snapshot.project_revision;
    REQUIRE(
        session.execute_command_string("set scale chromatic", context).status.first ==
        MessageLevel::Info);
    CHECK_FALSE(session.project_snapshot().project.pitch.scale.has_value());
}

TEST_CASE("Workspace settings persist outside project state", "[data-model][workspace]")
{
    auto const root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("xen-workspace-settings", "", false);
    REQUIRE(root.createDirectory());
    auto const sequences = root.getChildFile("sequences");
    auto const tunings = root.getChildFile("tunings");
    REQUIRE(sequences.createDirectory());
    REQUIRE(tunings.createDirectory());
    auto const settings_file = root.getChildFile("settings.json");

    {
        auto session =
            SequencerSession{SubmissionEffects::FailurePoint::None, settings_file};
        CHECK(session
                  .execute_command_string(
                      "set sequenceDirectory \"" +
                          sequences.getFullPathName().toStdString() + "\"",
                      CommandContext{})
                  .status.first == MessageLevel::Info);
        CHECK(session
                  .execute_command_string("set tuningDirectory \"" +
                                              tunings.getFullPathName().toStdString() +
                                              "\"",
                                          CommandContext{})
                  .status.first == MessageLevel::Info);
    }

    auto const restored = WorkspaceSettingsStore{settings_file}.load_or_initialize();
    CHECK(restored.sequence_directory == sequences);
    CHECK(restored.tuning_directory == tunings);
    CHECK(nlohmann::json::parse(serialize_project(ProjectState{}))
              .dump()
              .find(sequences.getFullPathName().toStdString()) == std::string::npos);
    CHECK(root.deleteRecursively());
}

TEST_CASE("Transform cycles amend one history entry and again remains compatible",
          "[data-model][transform]")
{
    auto session = SequencerSession{};
    auto project = session.project_snapshot().project;
    project.measure.cell.elements = {
        sequence::Note{.pitch = 10},
        sequence::Note{.pitch = 10},
        sequence::Note{.pitch = 10},
    };
    session.replace_project_history(project);

    auto context = CommandContext{
        .selection = SelectionPath{},
        .expected_project_revision = session.project_snapshot().project_revision,
    };
    REQUIRE(session.execute_command_string("chord Major 0", context).status.first ==
            MessageLevel::Info);
    auto const first = session.project_snapshot();

    context.expected_project_revision = first.project_revision;
    REQUIRE(session.execute_command_string("chord", context).status.first ==
            MessageLevel::Info);
    auto const second = session.project_snapshot();
    CHECK(second.history_entry_id == first.history_entry_id);
    CHECK(second.project_revision != first.project_revision);

    context.expected_project_revision = second.project_revision;
    REQUIRE(session.execute_command_string("again", context).status.first ==
            MessageLevel::Info);
    auto const repeated = session.project_snapshot();
    CHECK(repeated.history_entry_id == first.history_entry_id);
    CHECK(repeated.project_revision != second.project_revision);
}

TEST_CASE("No-op transform preserves session without history or publication",
          "[data-model][transform]")
{
    auto session = SequencerSession{};
    auto const before = session.project_snapshot();
    auto const mailbox_version = session.audio_project_update_version();
    auto const result = session.execute_command_string(
        "chord Major 0", {
                             .selection = SelectionPath{},
                             .expected_project_revision = before.project_revision,
                         });

    CHECK(result.status.first == MessageLevel::Info);
    auto const after = session.project_snapshot();
    CHECK(after.project_revision == before.project_revision);
    CHECK(after.history_entry_id == before.history_entry_id);
    CHECK(session.audio_project_update_version() == mailbox_version);
    REQUIRE(session.command_session().transform_cycle.has_value());
    CHECK_FALSE(session.command_session().transform_cycle->committed);
    CHECK(session.command_session().repeat_chain.empty());
}

TEST_CASE("Library reloads always advance library revision", "[data-model][library]")
{
    auto session = SequencerSession{};
    auto const before = session.library_snapshot().library_revision;
    REQUIRE(
        session.execute_command_string("load chords", CommandContext{}).status.first ==
        MessageLevel::Info);
    auto const after = session.library_snapshot().library_revision;
    CHECK(after != before);
}
