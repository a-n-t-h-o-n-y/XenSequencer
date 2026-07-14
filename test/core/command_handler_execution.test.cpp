#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>

#include <xen/chord.hpp>
#include <xen/command.hpp>
#include <xen/command_catalog.hpp>
#include <xen/command_transaction.hpp>
#include <xen/selection.hpp>
#include <xen/state.hpp>
#include <xen/submission_effects.hpp>

using namespace xen;

namespace
{

auto make_plugin_state() -> PluginState
{
    return PluginState{.timeline = XenTimeline{ProjectState{}}};
}

auto execute(PluginState &state, std::string const &text,
             std::optional<SelectionPath> selection = std::nullopt,
             CompositionCursor cursor = {}) -> CommandApplicationResult
{
    auto const result = bind_invocation(parse_command_chain(text).front());
    REQUIRE(result.has_value());
    auto const &step = result.value();
    REQUIRE(std::holds_alternative<ExecutableCommand>(step));
    auto const &command = std::get<ExecutableCommand>(step);
    auto transaction = CommandTransaction{state, SubmissionEffects::FailurePoint::None};
    auto context = CommandExecutionContext{
        .selection = std::move(selection),
        .cursor = cursor,
    };
    auto application = command.execute(transaction, context);
    transaction.prepare();
    transaction.apply_effects();
    transaction.install();
    transaction.finalize_effects();
    return application;
}

} // namespace

static_assert(noexcept(std::declval<CommandTransaction &>().install()));

TEST_CASE("Command transactions create resource candidates lazily",
          "[core][command][transaction]")
{
    auto state = make_plugin_state();
    auto execution = CommandExecutionContext{};

    auto informational =
        CommandTransaction{state, SubmissionEffects::FailurePoint::None};
    (void)informational.make_handler_context(
        CommandPolicy{ProjectOperation::None, LibraryAccess::None,
                      WorkspaceAccess::None, FileAccess::None, TargetRequirement::None,
                      RepeatPolicy::Never, HistoryPolicy::None},
        execution);
    CHECK_FALSE(informational.has_domain_candidates());

    auto project = CommandTransaction{state, SubmissionEffects::FailurePoint::None};
    auto project_context = project.make_handler_context(
        CommandPolicy{ProjectOperation::Edit, LibraryAccess::None,
                      WorkspaceAccess::None, FileAccess::None, TargetRequirement::None,
                      RepeatPolicy::OnSuccessfulProjectChange, HistoryPolicy::Commit},
        execution);
    project_context.edit_project().composition.columns.at(0).pitch.transposition = 2;
    CHECK(project.has_project_candidate());
    CHECK_FALSE(project.has_library_candidate());
    CHECK_FALSE(project.has_workspace_candidate());

    auto library = CommandTransaction{state, SubmissionEffects::FailurePoint::None};
    auto library_context = library.make_handler_context(
        CommandPolicy{ProjectOperation::None, LibraryAccess::Mutate,
                      WorkspaceAccess::None, FileAccess::None, TargetRequirement::None,
                      RepeatPolicy::Never, HistoryPolicy::None},
        execution);
    library_context.edit_library().scales.clear();
    CHECK_FALSE(library.has_project_candidate());
    CHECK(library.has_library_candidate());
    CHECK_FALSE(library.has_workspace_candidate());
}

TEST_CASE("Command handler contexts deny undeclared capabilities",
          "[core][command][transaction]")
{
    auto state = make_plugin_state();
    auto execution = CommandExecutionContext{};
    auto transaction = CommandTransaction{state, SubmissionEffects::FailurePoint::None};
    auto context = transaction.make_handler_context(
        CommandPolicy{ProjectOperation::None, LibraryAccess::None,
                      WorkspaceAccess::None, FileAccess::None, TargetRequirement::None,
                      RepeatPolicy::Never, HistoryPolicy::None},
        execution);

    CHECK_THROWS_AS(context.edit_project(), std::logic_error);
    CHECK_THROWS_AS(context.library(), std::logic_error);
    CHECK_THROWS_AS(context.read_text({}), std::logic_error);
}

TEST_CASE("Direct handlers mutate engine without requiring session editor state",
          "[core][command][handler]")
{
    auto state = make_plugin_state();

    auto const result = execute(state, "set key 12");
    CHECK(result.status.second == "Key Set to 12.");
    CHECK(state.timeline.get_state().composition.columns.at(0).pitch.transposition ==
          12);
}

TEST_CASE("Direct handlers set composition loop endpoints",
          "[core][command][handler][composition]")
{
    auto state = make_plugin_state();

    auto const start = execute(state, "composition loop start -2");
    CHECK(start.status.first == MessageLevel::Info);
    CHECK(state.timeline.get_state().composition.loop_region.start_column == -2);
    CHECK(state.timeline.get_state().composition.loop_region.end_column == 0);

    auto const end = execute(state, "composition loop end 3");
    CHECK(end.status.first == MessageLevel::Info);
    CHECK(state.timeline.get_state().composition.loop_region.start_column == -2);
    CHECK(state.timeline.get_state().composition.loop_region.end_column == 3);

    CHECK_THROWS_AS((void)execute(state, "composition loop start 4"),
                    std::invalid_argument);
}

TEST_CASE("Direct handlers edit sparse composition axis metadata",
          "[core][command][handler][composition]")
{
    auto state = make_plugin_state();

    CHECK(execute(state, "composition cell assign -1 4 S1").status.first ==
          MessageLevel::Info);
    auto project = state.timeline.get_state();
    REQUIRE(project.composition.rows.size() == 2);
    REQUIRE(project.composition.columns.size() == 2);
    CHECK(project.composition.rows.at(-1).channel_id == DEFAULT_CHANNEL_ID);
    CHECK(project.composition.columns.at(4) == project.composition.columns.at(0));

    CHECK(execute(state, "composition row rename -1 \"Drums\"").status.first ==
          MessageLevel::Info);
    CHECK(execute(state, "composition row channel -1 \"peer\"").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    REQUIRE(project.composition.rows.at(-1).name.has_value());
    CHECK(*project.composition.rows.at(-1).name == "Drums");
    CHECK(project.composition.rows.at(-1).channel_id == "peer");

    CHECK(execute(state, "set duration 7/8", std::nullopt,
                  CompositionCursor{.row_coordinate = -1,
                                    .column_coordinate = 4,
                                    .sequence_id = DEFAULT_SEQUENCE_ID})
              .status.first == MessageLevel::Info);
    CHECK(state.timeline.get_state().composition.columns.at(4).duration ==
          sequence::TimeSignature{7, 8});
}

TEST_CASE("Direct handlers assign, move, and unassign sparse composition cells",
          "[core][command][handler][composition]")
{
    auto state = make_plugin_state();

    CHECK(execute(state, "composition cell assign 0 0 S1").status.first ==
          MessageLevel::Info);
    CHECK(sequence_reference_at(state.timeline.get_state().composition, 0, 0) ==
          DEFAULT_SEQUENCE_ID);

    CHECK(execute(state, "composition cell assign -2 20 \"Verse\"").status.first ==
          MessageLevel::Info);
    auto project = state.timeline.get_state();
    auto const verse_id = sequence_reference_at(project.composition, -2, 20);
    REQUIRE(verse_id.has_value());
    CHECK(verse_id != DEFAULT_SEQUENCE_ID);
    auto const verse_entry = std::ranges::find(project.sequence_bank.sequences,
                                               *verse_id, &SequenceBankEntry::id);
    REQUIRE(verse_entry != project.sequence_bank.sequences.end());
    REQUIRE(verse_entry->name.has_value());
    CHECK(*verse_entry->name == "Verse");

    CHECK(execute(state, "composition cell assign 0 0 \"Intro Copy\"").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    auto const copy_id = sequence_reference_at(project.composition, 0, 0);
    REQUIRE(copy_id.has_value());
    CHECK(copy_id != DEFAULT_SEQUENCE_ID);
    auto const copy_entry = std::ranges::find(project.sequence_bank.sequences, *copy_id,
                                              &SequenceBankEntry::id);
    REQUIRE(copy_entry != project.sequence_bank.sequences.end());
    REQUIRE(copy_entry->name.has_value());
    CHECK(*copy_entry->name == "Intro Copy");

    CHECK(execute(state, "composition cell assign 0 0 verse").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    CHECK(sequence_reference_at(project.composition, 0, 0) == verse_id);
    CHECK(project.sequence_bank.sequences.size() == 3);

    CHECK_THROWS_AS((void)execute(state, "composition cell assign 0 0 \"\""),
                    std::invalid_argument);

    CHECK(execute(state, "composition cell move -2 20 3 -5").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    CHECK_FALSE(sequence_reference_at(project.composition, -2, 20).has_value());
    CHECK(sequence_reference_at(project.composition, 3, -5) == verse_id);

    CHECK(execute(state, "composition cell unassign 0 0").status.first ==
          MessageLevel::Info);
    CHECK_FALSE(sequence_reference_at(state.timeline.get_state().composition, 0, 0)
                    .has_value());
}

TEST_CASE("Translate direction handler applies catalog-validated values",
          "[core][command][handler]")
{
    auto state = make_plugin_state();

    CHECK(execute(state, "set translateDirection down").status.first ==
          MessageLevel::Info);
    CHECK(selected_column(state.timeline.get_state(), CompositionCursor{})
              .pitch.translation_direction == TranslateDirection::Down);

    CHECK(execute(state, "set translateDirection up").status.first ==
          MessageLevel::Info);
    CHECK(selected_column(state.timeline.get_state(), CompositionCursor{})
              .pitch.translation_direction == TranslateDirection::Up);
}

TEST_CASE("Direct edit handlers use execution-context selection",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto engine = state.timeline.get_state();
    selected_sequence(engine, xen::CompositionCursor{}).elements = {
        sequence::Note{1, 0.5f, 0.f, 1.f},
    };
    state.timeline.stage(std::move(engine));

    auto const selection = select_element_in_cell({}, 0);

    CHECK(execute(state, "note 12 0.5 0.25 0.75", selection).status.first ==
          MessageLevel::Info);
    auto const after_note = state.timeline.get_state();
    auto const &created = std::get<sequence::Note>(
        selected_sequence(after_note, xen::CompositionCursor{}).elements[0]);
    CHECK(created.pitch == 12);

    CHECK(execute(state, "delete", selection).status.first == MessageLevel::Info);
    CHECK(selected_sequence(state.timeline.get_state(), xen::CompositionCursor{})
              .elements.empty());
}

TEST_CASE("Direct chord handler preserves transform session baseline",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    state.library.chords = {
        Chord{.name = "major", .intervals = {0, 4, 7}},
        Chord{.name = "minor", .intervals = {0, 3, 7}},
    };
    auto engine = state.timeline.get_state();
    selected_sequence(engine, xen::CompositionCursor{}).elements = {
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
    };
    state.timeline.stage(std::move(engine));

    auto const selection = SelectionPath{};

    CHECK(execute(state, "chord major 0", selection).status.second ==
          "Chorded with major inversion: 0");
    REQUIRE(state.command_session.transform_cycle.has_value());
    CHECK(state.command_session.transform_cycle->previous_chord_name == "major");
    CHECK(state.command_session.transform_cycle->previous_inversion == 0);
    CHECK(state.command_session.transform_cycle->project_revision ==
          state.timeline.get_project_revision());
    CHECK(execute(state, "chord", selection).status.second ==
          "Chorded with major inversion: 1");
    CHECK(execute(state, "chord cycle 0", selection).status.second ==
          "Chorded with minor inversion: 0");
}

TEST_CASE("Timeline commit API requires explicit state", "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto engine = state.timeline.get_state();
    engine.composition.columns.at(0).pitch.transposition = 1;
    state.timeline.stage(engine);
    REQUIRE(state.timeline.commit(state.timeline.get_state()));
    engine.composition.columns.at(0).pitch.transposition = 2;
    state.timeline.stage(engine);
    REQUIRE(state.timeline.commit(state.timeline.get_state()));
    CHECK(state.timeline.undo());
    CHECK(state.timeline.get_state().composition.columns.at(0).pitch.transposition ==
          1);
    CHECK(state.timeline.redo());
    CHECK(state.timeline.get_state().composition.columns.at(0).pitch.transposition ==
          2);
}
