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

    selected_sequence(project, xen::CompositionCursor{}).weight = 0.f;
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    selected_sequence(project, xen::CompositionCursor{}).elements = {
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
    selected_duration(project, xen::CompositionCursor{}) = {65, 1};
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.sequence_bank.next_id = DEFAULT_SEQUENCE_ID;
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.composition.columns.at(0).pitch.tuning.definition.intervals = {0.f, 200.f,
                                                                           100.f};
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.composition.columns.at(0).pitch.base_frequency = 0.f;
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.composition.columns.at(0).pitch.transposition = 128;
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    auto timeline = XenTimeline{ProjectState{}};
    CHECK_THROWS_AS(timeline.commit(project), std::invalid_argument);
}

TEST_CASE("Project schema 5 stores sequence bank and sparse composition",
          "[data-model][serialize]")
{
    auto const project = ProjectState{};
    auto const encoded = nlohmann::json::parse(serialize_project(project));
    CHECK(encoded.at("schema") == 5);
    CHECK(encoded.at("kind") == "xen_composition");
    CHECK_FALSE(encoded.at("project").contains("pitch"));
    CHECK(encoded.at("project").contains("sequence_bank"));
    CHECK(encoded.at("project").contains("composition"));
    CHECK(encoded.at("project").at("composition").contains("placements"));
    CHECK(encoded.at("project").at("composition").contains("default_column"));
    CHECK_FALSE(encoded.at("project").contains("measure"));
    CHECK_FALSE(encoded.at("project").contains("tuning"));
    CHECK(encoded.at("project")
              .at("sequence_bank")
              .at("sequences")
              .front()
              .at("cell")
              .contains("time_signature") == false);

    auto old_schema = encoded;
    old_schema["schema"] = 4;
    CHECK_THROWS(deserialize_project(old_schema.dump()));

    auto const old = nlohmann::json{
        {"sequence", encoded.at("project").at("sequence_bank").at("sequences").front()},
    };
    CHECK_THROWS(deserialize_project(old.dump()));
}

TEST_CASE("Default project has one channel-1 4/4 arranged sequence",
          "[data-model][composition]")
{
    auto const project = ProjectState{};
    REQUIRE(project.sequence_bank.sequences.size() == 1);
    CHECK(project.sequence_bank.sequences.front().id == DEFAULT_SEQUENCE_ID);
    CHECK_FALSE(project.sequence_bank.sequences.front().name.has_value());
    CHECK(project.sequence_bank.next_id == DEFAULT_SEQUENCE_ID + 1);
    REQUIRE(project.composition.columns.size() == 1);
    CHECK(project.composition.columns.at(0).duration == sequence::TimeSignature{4, 4});
    CHECK(project.composition.loop_region.start_column == 0);
    CHECK(project.composition.loop_region.end_column == 0);
    REQUIRE(project.composition.rows.size() == 1);
    CHECK(project.composition.rows.at(0).channel_id == DEFAULT_CHANNEL_ID);
    CHECK_FALSE(project.composition.rows.at(0).name.has_value());
    REQUIRE(project.composition.placements.size() == 1);
    CHECK(sequence_reference_at(project.composition, 0, 0) == DEFAULT_SEQUENCE_ID);
}

TEST_CASE("Sparse assignment inherits axes without materializing gaps",
          "[data-model][composition]")
{
    auto project = ProjectState{};
    project.composition.columns.at(0).duration = {7, 8};
    project.composition.columns.at(0).pitch.transposition = 9;

    assign_sequence_reference(project.composition, -4, 20, DEFAULT_SEQUENCE_ID);
    REQUIRE(project.composition.rows.size() == 2);
    REQUIRE(project.composition.columns.size() == 2);
    REQUIRE(project.composition.placements.size() == 2);
    CHECK(project.composition.rows.at(-4).channel_id == DEFAULT_CHANNEL_ID);
    CHECK(project.composition.columns.at(20) == project.composition.columns.at(0));
    CHECK(sequence_reference_at(project.composition, -4, 20) == DEFAULT_SEQUENCE_ID);
    CHECK_FALSE(project.composition.columns.contains(1));
    CHECK_FALSE(project.composition.rows.contains(-1));
}

TEST_CASE("Sparse axis inheritance and movement are deterministic",
          "[data-model][composition]")
{
    auto composition = Composition{};
    (void)ensure_composition_row(composition, -2, "left");
    (void)ensure_composition_row(composition, 2, "right");
    CHECK(ensure_composition_row(composition, 0).channel_id == "left");

    composition.columns.emplace(-2, CompositionColumn{.duration = {3, 4}});
    composition.columns.emplace(2, CompositionColumn{.duration = {5, 8}});
    CHECK(ensure_composition_column(composition, 0).duration ==
          sequence::TimeSignature{3, 4});

    auto project = ProjectState{};
    assign_sequence_reference(project.composition, 1, 1, DEFAULT_SEQUENCE_ID);
    CHECK_THROWS_AS(move_sequence_reference(project.composition, {0, 0}, {1, 1}),
                    std::invalid_argument);
    CHECK_THROWS_AS(move_sequence_reference(project.composition, {9, 9}, {8, 8}),
                    std::invalid_argument);
}

TEST_CASE("Typed Cell and Composition documents round-trip", "[data-model][serialize]")
{
    auto cell = sequence::Cell{
        .elements = {sequence::Note{.pitch = 7}},
        .weight = 2.f,
    };
    CHECK(deserialize_cell_file(serialize_cell_file(cell)) == cell);

    auto project = ProjectState{};
    project.composition.columns.at(0).pitch.transposition = 11;
    CHECK(deserialize_composition(serialize_composition(project)) == project);
}

TEST_CASE("Sequence bank and sparse composition API covers editing operations",
          "[data-model][composition]")
{
    auto project = ProjectState{};

    auto const duplicate_id =
        duplicate_sequence(project.sequence_bank, DEFAULT_SEQUENCE_ID);
    REQUIRE(duplicate_id != DEFAULT_SEQUENCE_ID);
    REQUIRE(find_sequence(project.sequence_bank, duplicate_id) != nullptr);

    assign_sequence_reference(project.composition, -2, 5, duplicate_id);
    CHECK(sequence_reference_at(project.composition, -2, 5) == duplicate_id);
    set_column_duration(project.composition, 5, sequence::TimeSignature{5, 8});
    CHECK(project.composition.columns.at(5).duration == sequence::TimeSignature{5, 8});
    assign_row_channel(project.composition, -2, "peer");
    move_sequence_reference(project.composition, {-2, 5}, {-3, 8});
    CHECK_FALSE(sequence_reference_at(project.composition, -2, 5).has_value());
    CHECK(sequence_reference_at(project.composition, -3, 8) == duplicate_id);
    CHECK(project.composition.rows.at(-3).channel_id == "peer");
    CHECK_FALSE(project.composition.rows.contains(-2));
    CHECK_FALSE(project.composition.columns.contains(5));

    set_loop_start(project.composition, -4);
    set_loop_end(project.composition, 8);
    CHECK(project.composition.loop_region == LoopRegion{-4, 8});

    unassign_sequence_reference(project.composition, -3, 8);
    CHECK(project.composition.rows.contains(-3));
    CHECK(project.composition.columns.contains(8));
    CHECK(remove_sequence(project.sequence_bank, duplicate_id));
    CHECK(find_sequence(project.sequence_bank, duplicate_id) == nullptr);
}

TEST_CASE("Sparse assignments leave explicit loop coordinates stable",
          "[data-model][composition]")
{
    auto project = ProjectState{};
    set_loop_start(project.composition, -2);
    set_loop_end(project.composition, 3);
    assign_sequence_reference(project.composition, 0, -100, DEFAULT_SEQUENCE_ID);
    assign_sequence_reference(project.composition, 100, 100, DEFAULT_SEQUENCE_ID);
    CHECK(project.composition.loop_region == LoopRegion{-2, 3});
}

TEST_CASE("Sequence and composition row names serialize and validate",
          "[data-model][composition][serialize]")
{
    auto project = ProjectState{};
    project.sequence_bank.sequences.front().name = "Intro";
    project.composition.rows.at(0).name = "Lead";

    auto const encoded = nlohmann::json::parse(serialize_project(project));
    CHECK(
        encoded.at("project").at("sequence_bank").at("sequences").front().at("name") ==
        "Intro");
    CHECK(encoded.at("project").at("composition").at("rows").front().at("name") ==
          "Lead");

    auto decoded = deserialize_project(encoded.dump());
    REQUIRE(decoded.sequence_bank.sequences.front().name.has_value());
    CHECK(*decoded.sequence_bank.sequences.front().name == "Intro");
    REQUIRE(decoded.composition.rows.at(0).name.has_value());
    CHECK(*decoded.composition.rows.at(0).name == "Lead");

    auto legacy = encoded;
    legacy.at("project").at("sequence_bank").at("sequences").front().erase("name");
    legacy.at("project").at("composition").at("rows").front().erase("name");
    decoded = deserialize_project(legacy.dump());
    CHECK_FALSE(decoded.sequence_bank.sequences.front().name.has_value());
    CHECK_FALSE(decoded.composition.rows.at(0).name.has_value());

    project.sequence_bank.sequences.front().name = "";
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    auto const duplicate_id = create_sequence(project.sequence_bank, sequence::Cell{});
    CHECK(duplicate_id != DEFAULT_SEQUENCE_ID);
    project.sequence_bank.sequences.back().name = "S1";
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.sequence_bank.sequences.front().name = "Wow";
    auto const duplicate_case_id =
        create_sequence(project.sequence_bank, sequence::Cell{});
    CHECK(duplicate_case_id != DEFAULT_SEQUENCE_ID);
    project.sequence_bank.sequences.back().name = "wow";
    CHECK_THROWS_AS(validate(project), std::invalid_argument);

    project = ProjectState{};
    project.sequence_bank.sequences.front().name = "Named";
    auto const duplicated_named_id =
        duplicate_sequence(project.sequence_bank, DEFAULT_SEQUENCE_ID);
    auto const duplicated_named = std::ranges::find(
        project.sequence_bank.sequences, duplicated_named_id, &SequenceBankEntry::id);
    REQUIRE(duplicated_named != project.sequence_bank.sequences.end());
    CHECK_FALSE(duplicated_named->name.has_value());

    project = ProjectState{};
    project.composition.rows.at(0).name = "";
    CHECK_THROWS_AS(validate(project), std::invalid_argument);
}

TEST_CASE("Composition validation allows empty arranged cells",
          "[data-model][composition]")
{
    auto project = ProjectState{};
    unassign_sequence_reference(project.composition, 0, 0);
    CHECK_NOTHROW(validate(project));
}

TEST_CASE("Composition loop region serializes, defaults, and validates",
          "[data-model][composition][serialize]")
{
    auto project = ProjectState{};
    set_loop_start(project.composition, -2);
    set_loop_end(project.composition, 3);

    auto const encoded = nlohmann::json::parse(serialize_project(project));
    CHECK(encoded.at("project")
              .at("composition")
              .at("loop_region")
              .at("start_column")
              .get<CompositionCoordinate>() == -2);
    CHECK(encoded.at("project")
              .at("composition")
              .at("loop_region")
              .at("end_column")
              .get<CompositionCoordinate>() == 3);

    auto decoded = deserialize_project(encoded.dump());
    CHECK(decoded.composition.loop_region.start_column == -2);
    CHECK(decoded.composition.loop_region.end_column == 3);

    project.composition.loop_region.start_column = 4;
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
    REQUIRE(snapshot.project.composition.columns.at(0).pitch.scale.has_value());
    CHECK(snapshot.project.composition.columns.at(0).pitch.scale->source_id == "major");

    context.expected_project_revision = snapshot.project_revision;
    REQUIRE(session.execute_command_string("shift scaleMode 1", context).status.first ==
            MessageLevel::Info);
    snapshot = session.project_snapshot();
    REQUIRE(snapshot.project.composition.columns.at(0).pitch.scale.has_value());
    CHECK(snapshot.project.composition.columns.at(0).pitch.scale->source_id == "major");

    context.expected_project_revision = snapshot.project_revision;
    REQUIRE(session.execute_command_string("shift scale 1", context).status.first ==
            MessageLevel::Info);
    snapshot = session.project_snapshot();
    REQUIRE(snapshot.project.composition.columns.at(0).pitch.scale.has_value());
    CHECK(snapshot.project.composition.columns.at(0).pitch.scale->source_id == "other");

    context.expected_project_revision = snapshot.project_revision;
    REQUIRE(
        session.execute_command_string("set scale chromatic", context).status.first ==
        MessageLevel::Info);
    CHECK_FALSE(session.project_snapshot()
                    .project.composition.columns.at(0)
                    .pitch.scale.has_value());
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
                      "set contentDirectory \"" +
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
    CHECK(restored.content_directory == sequences.getFullPathName().toStdString());
    CHECK(restored.tuning_directory == tunings.getFullPathName().toStdString());
    CHECK(nlohmann::json::parse(serialize_project(ProjectState{}))
              .dump()
              .find(sequences.getFullPathName().toStdString()) == std::string::npos);
    CHECK(root.deleteRecursively());
}

TEST_CASE("Workspace settings default construction is pure", "[data-model][workspace]")
{
    auto const settings = WorkspaceSettings{};
    CHECK(settings.content_directory.empty());
    CHECK(settings.tuning_directory.empty());
}

TEST_CASE("Transform cycles amend one history entry and again remains compatible",
          "[data-model][transform]")
{
    auto session = SequencerSession{};
    auto project = session.project_snapshot().project;
    selected_sequence(project, xen::CompositionCursor{}).elements = {
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

TEST_CASE("No-op transform preserves session without history mutation",
          "[data-model][transform]")
{
    auto session = SequencerSession{};
    auto const before = session.project_snapshot();
    auto const result = session.execute_command_string(
        "chord Major 0", {
                             .selection = SelectionPath{},
                             .expected_project_revision = before.project_revision,
                         });

    CHECK(result.status.first == MessageLevel::Info);
    auto const after = session.project_snapshot();
    CHECK(after.project_revision == before.project_revision);
    CHECK(after.history_entry_id == before.history_entry_id);
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
