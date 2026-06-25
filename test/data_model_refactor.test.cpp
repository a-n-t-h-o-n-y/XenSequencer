#include <algorithm>
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

    default_measure(project).cell.weight = 0.f;
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    default_measure(project).cell.elements = {
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
    default_measure_length(project) = {65, 1};
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.measure_bank.next_id = DEFAULT_MEASURE_ID;
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

TEST_CASE("Project schema 2 stores measure bank and composition",
          "[data-model][serialize]")
{
    auto const project = ProjectState{};
    auto const encoded = nlohmann::json::parse(serialize_project(project));
    CHECK(encoded.at("schema") == 2);
    CHECK(encoded.at("project").contains("pitch"));
    CHECK(encoded.at("project").contains("measure_bank"));
    CHECK(encoded.at("project").contains("composition"));
    CHECK_FALSE(encoded.at("project").contains("measure"));
    CHECK_FALSE(encoded.at("project").contains("tuning"));
    CHECK(encoded.at("project")
              .at("measure_bank")
              .at("measures")
              .front()
              .at("measure")
              .contains("time_signature") == false);

    auto const old = nlohmann::json{
        {"measure", encoded.at("project").at("measure_bank").at("measures").front()},
        {"tuning", encoded.at("project").at("pitch").at("tuning")},
    };
    CHECK_THROWS(deserialize_project(old.dump()));
}

TEST_CASE("Default project has one current-output 4/4 arranged measure",
          "[data-model][composition]")
{
    auto const project = ProjectState{};
    REQUIRE(project.measure_bank.measures.size() == 1);
    CHECK(project.measure_bank.measures.front().id == DEFAULT_MEASURE_ID);
    CHECK_FALSE(project.measure_bank.measures.front().name.has_value());
    CHECK(project.measure_bank.next_id == DEFAULT_MEASURE_ID + 1);
    REQUIRE(project.composition.columns.size() == 1);
    CHECK(project.composition.columns.front().length == sequence::TimeSignature{4, 4});
    CHECK(project.composition.loop_region.start_column == 0);
    CHECK(project.composition.loop_region.end_column == 0);
    REQUIRE(project.composition.rows.size() == 1);
    CHECK(project.composition.rows.front().output_id == CURRENT_INSTANCE_OUTPUT_ID);
    CHECK_FALSE(project.composition.rows.front().name.has_value());
    REQUIRE(project.composition.rows.front().cells.size() == 1);
    CHECK(project.composition.rows.front().cells.front() == DEFAULT_MEASURE_ID);
}

TEST_CASE("Measure bank and composition API covers editing operations",
          "[data-model][composition]")
{
    auto project = ProjectState{};

    auto const duplicate_id =
        duplicate_measure(project.measure_bank, DEFAULT_MEASURE_ID);
    REQUIRE(duplicate_id != DEFAULT_MEASURE_ID);
    REQUIRE(find_measure(project.measure_bank, duplicate_id) != nullptr);

    insert_column(project.composition, 1, sequence::TimeSignature{3, 4});
    assign_measure_reference(project.composition, 0, 1, duplicate_id);
    CHECK(measure_reference_at(project.composition, 0, 1) == duplicate_id);
    set_column_length(project.composition, 1, sequence::TimeSignature{5, 8});
    CHECK(project.composition.columns[1].length == sequence::TimeSignature{5, 8});
    set_loop_start(project.composition, 1);
    set_loop_end(project.composition, 0);
    CHECK(project.composition.loop_region.start_column == 1);
    CHECK(project.composition.loop_region.end_column == 0);

    insert_row(project.composition, 1, "peer");
    assign_measure_reference(project.composition, 1, 0, duplicate_id);
    move_row(project.composition, 1, 0);
    CHECK(project.composition.rows.front().output_id == "peer");
    assign_row_output(project.composition, 0, CURRENT_INSTANCE_OUTPUT_ID);
    move_column(project.composition, 1, 0);
    CHECK(project.composition.columns.front().length == sequence::TimeSignature{5, 8});
    CHECK(project.composition.loop_region.start_column == 0);
    CHECK(project.composition.loop_region.end_column == 1);

    clear_measure_reference(project.composition, 0, 0);
    CHECK_FALSE(measure_reference_at(project.composition, 0, 0).has_value());
    remove_column(project.composition, 0);
    CHECK(project.composition.loop_region.start_column == 0);
    CHECK(project.composition.loop_region.end_column == 0);
    remove_row(project.composition, 0);
    CHECK(remove_measure(project.measure_bank, duplicate_id));
    CHECK(find_measure(project.measure_bank, duplicate_id) == nullptr);
}

TEST_CASE("Column insertion adjusts inclusive loop bounds", "[data-model][composition]")
{
    auto project = ProjectState{};
    insert_column(project.composition, 1, sequence::TimeSignature{4, 4});
    insert_column(project.composition, 2, sequence::TimeSignature{4, 4});
    insert_column(project.composition, 3, sequence::TimeSignature{4, 4});
    set_loop_start(project.composition, 1);
    set_loop_end(project.composition, 2);

    auto inside = project;
    insert_column(inside.composition, 2, sequence::TimeSignature{4, 4});
    CHECK(inside.composition.loop_region.start_column == 1);
    CHECK(inside.composition.loop_region.end_column == 3);

    auto before = project;
    insert_column(before.composition, 0, sequence::TimeSignature{4, 4});
    CHECK(before.composition.loop_region.start_column == 2);
    CHECK(before.composition.loop_region.end_column == 3);

    auto after = project;
    insert_column(after.composition, 4, sequence::TimeSignature{4, 4});
    CHECK(after.composition.loop_region.start_column == 1);
    CHECK(after.composition.loop_region.end_column == 2);

    auto start_edge = project;
    insert_column(start_edge.composition, 1, sequence::TimeSignature{4, 4});
    CHECK(start_edge.composition.loop_region.start_column == 1);
    CHECK(start_edge.composition.loop_region.end_column == 3);

    auto end_edge = project;
    insert_column(end_edge.composition, 3, sequence::TimeSignature{4, 4});
    CHECK(end_edge.composition.loop_region.start_column == 1);
    CHECK(end_edge.composition.loop_region.end_column == 3);
}

TEST_CASE("Measure and composition row names serialize and validate",
          "[data-model][composition][serialize]")
{
    auto project = ProjectState{};
    project.measure_bank.measures.front().name = "Intro";
    project.composition.rows.front().name = "Lead";

    auto const encoded = nlohmann::json::parse(serialize_project(project));
    CHECK(encoded.at("project").at("measure_bank").at("measures").front().at("name") ==
          "Intro");
    CHECK(encoded.at("project").at("composition").at("rows").front().at("name") ==
          "Lead");

    auto decoded = deserialize_project(encoded.dump());
    REQUIRE(decoded.measure_bank.measures.front().name.has_value());
    CHECK(*decoded.measure_bank.measures.front().name == "Intro");
    REQUIRE(decoded.composition.rows.front().name.has_value());
    CHECK(*decoded.composition.rows.front().name == "Lead");

    auto legacy = encoded;
    legacy.at("project").at("measure_bank").at("measures").front().erase("name");
    legacy.at("project").at("composition").at("rows").front().erase("name");
    decoded = deserialize_project(legacy.dump());
    CHECK_FALSE(decoded.measure_bank.measures.front().name.has_value());
    CHECK_FALSE(decoded.composition.rows.front().name.has_value());

    project.measure_bank.measures.front().name = "";
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    auto const duplicate_id = create_measure(project.measure_bank, Measure{});
    CHECK(duplicate_id != DEFAULT_MEASURE_ID);
    project.measure_bank.measures.back().name = "M1";
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.measure_bank.measures.front().name = "Wow";
    auto const duplicate_case_id = create_measure(project.measure_bank, Measure{});
    CHECK(duplicate_case_id != DEFAULT_MEASURE_ID);
    project.measure_bank.measures.back().name = "wow";
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.measure_bank.measures.front().name = "Named";
    auto const duplicated_named_id =
        duplicate_measure(project.measure_bank, DEFAULT_MEASURE_ID);
    auto const duplicated_named = std::ranges::find(
        project.measure_bank.measures, duplicated_named_id, &MeasureBankEntry::id);
    REQUIRE(duplicated_named != project.measure_bank.measures.end());
    CHECK_FALSE(duplicated_named->name.has_value());

    project = ProjectState{};
    project.composition.rows.front().name = "";
    CHECK_THROWS_AS(validate(project), std::invalid_argument);
}

TEST_CASE("Composition validation allows empty arranged cells",
          "[data-model][composition]")
{
    auto project = ProjectState{};
    clear_measure_reference(project.composition, 0, 0);
    CHECK_NOTHROW(validate(project));
}

TEST_CASE("Composition loop region serializes, defaults, and validates",
          "[data-model][composition][serialize]")
{
    auto project = ProjectState{};
    insert_column(project.composition, 1, sequence::TimeSignature{3, 4});
    set_loop_start(project.composition, 1);
    set_loop_end(project.composition, 0);

    auto const encoded = nlohmann::json::parse(serialize_project(project));
    CHECK(encoded.at("project")
              .at("composition")
              .at("loop_region")
              .at("start_column")
              .get<std::size_t>() == 1);
    CHECK(encoded.at("project")
              .at("composition")
              .at("loop_region")
              .at("end_column")
              .get<std::size_t>() == 0);

    auto decoded = deserialize_project(encoded.dump());
    CHECK(decoded.composition.loop_region.start_column == 1);
    CHECK(decoded.composition.loop_region.end_column == 0);

    auto legacy = encoded;
    legacy.at("project").at("composition").erase("loop_region");
    decoded = deserialize_project(legacy.dump());
    CHECK(decoded.composition.loop_region.start_column == 0);
    CHECK(decoded.composition.loop_region.end_column == 1);

    project.composition.loop_region.start_column = project.composition.columns.size();
    CHECK_THROWS_AS(validate(project), std::invalid_argument);
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
        auto session = SequencerSession{SubmissionEffects::FailurePoint::None,
                                        settings_file.getFullPathName().toStdString()};
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

    auto const restored =
        WorkspaceSettingsStore{settings_file.getFullPathName().toStdString()}
            .load_or_initialize();
    CHECK(restored.sequence_directory == sequences.getFullPathName().toStdString());
    CHECK(restored.tuning_directory == tunings.getFullPathName().toStdString());
    CHECK(nlohmann::json::parse(serialize_project(ProjectState{}))
              .dump()
              .find(sequences.getFullPathName().toStdString()) == std::string::npos);
    CHECK(root.deleteRecursively());
}

TEST_CASE("Workspace settings default construction is pure", "[data-model][workspace]")
{
    auto const settings = WorkspaceSettings{};
    CHECK(settings.sequence_directory.empty());
    CHECK(settings.tuning_directory.empty());
}

TEST_CASE("Transform cycles amend one history entry and again remains compatible",
          "[data-model][transform]")
{
    auto session = SequencerSession{};
    auto project = session.project_snapshot().project;
    default_measure(project).cell.elements = {
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
