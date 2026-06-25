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
             std::optional<SelectionPath> selection = std::nullopt)
    -> CommandApplicationResult
{
    auto const result = bind_invocation(parse_command_chain(text).front());
    REQUIRE(std::holds_alternative<BoundStep>(result));
    auto const &step = std::get<BoundStep>(result);
    REQUIRE(std::holds_alternative<ExecutableCommand>(step));
    auto const &command = std::get<ExecutableCommand>(step);
    auto transaction = CommandTransaction{state, SubmissionEffects::FailurePoint::None};
    auto context = CommandExecutionContext{
        .selection = std::move(selection),
        .valid_output_ids = {CURRENT_INSTANCE_OUTPUT_ID, "peer"},
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
    project_context.edit_project().pitch.transposition = 2;
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
    CHECK(state.timeline.get_state().pitch.transposition == 12);
}

TEST_CASE("Direct handlers set composition loop endpoints",
          "[core][command][handler][composition]")
{
    auto state = make_plugin_state();
    auto engine = state.timeline.get_state();
    insert_column(engine.composition, 1, sequence::TimeSignature{3, 4});
    state.timeline.stage(std::move(engine));

    auto const start = execute(state, "composition loop start 1");
    CHECK(start.status.first == MessageLevel::Info);
    CHECK(state.timeline.get_state().composition.loop_region.start_column == 1);
    CHECK(state.timeline.get_state().composition.loop_region.end_column == 1);

    auto const end = execute(state, "composition loop end 1");
    CHECK(end.status.first == MessageLevel::Info);
    CHECK(state.timeline.get_state().composition.loop_region.start_column == 1);
    CHECK(state.timeline.get_state().composition.loop_region.end_column == 1);

    CHECK_THROWS_AS((void)execute(state, "composition loop start 2"),
                    std::out_of_range);
}

TEST_CASE("Direct handlers edit composition rows and columns",
          "[core][command][handler][composition]")
{
    auto state = make_plugin_state();

    CHECK_THROWS_AS((void)execute(state, "composition row delete 0"),
                    std::invalid_argument);
    CHECK_THROWS_AS((void)execute(state, "composition column delete 0"),
                    std::invalid_argument);

    CHECK(execute(state, "composition row insert after 0").status.first ==
          MessageLevel::Info);
    auto project = state.timeline.get_state();
    REQUIRE(project.composition.rows.size() == 2);
    CHECK(project.composition.rows[1].output_id == CURRENT_INSTANCE_OUTPUT_ID);
    CHECK(project.composition.rows[1].cells.front() == std::nullopt);

    CHECK(execute(state, "composition row rename 1 \"Drums\"").status.first ==
          MessageLevel::Info);
    CHECK(execute(state, "composition row output 1 \"peer\"").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    REQUIRE(project.composition.rows[1].name.has_value());
    CHECK(*project.composition.rows[1].name == "Drums");
    CHECK(project.composition.rows[1].output_id == "peer");

    CHECK(execute(state, "composition row insert before 1").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    REQUIRE(project.composition.rows.size() == 3);
    CHECK(project.composition.rows[1].output_id == "peer");
    CHECK_FALSE(project.composition.rows[1].name.has_value());

    CHECK_THROWS_AS((void)execute(state, "composition row output 1 \"bus-a\""),
                    std::invalid_argument);

    CHECK(execute(state, "composition row delete 1").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    REQUIRE(project.composition.rows.size() == 2);

    CHECK(execute(state, "composition column insert after 0").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    REQUIRE(project.composition.columns.size() == 2);
    CHECK(project.composition.columns[1].length == sequence::TimeSignature{4, 4});
    CHECK(project.composition.rows[0].cells[1] == std::nullopt);
    CHECK(project.composition.rows[1].cells[1] == std::nullopt);

    CHECK(execute(state, "composition column length 1 7/8").status.first ==
          MessageLevel::Info);
    CHECK(state.timeline.get_state().composition.columns[1].length ==
          sequence::TimeSignature{7, 8});

    CHECK(execute(state, "composition column insert before 1").status.first ==
          MessageLevel::Info);
    CHECK(state.timeline.get_state().composition.columns[1].length ==
          sequence::TimeSignature{7, 8});

    CHECK(execute(state, "composition column delete 1").status.first ==
          MessageLevel::Info);
    CHECK(state.timeline.get_state().composition.columns.size() == 2);
}

TEST_CASE("Direct handlers assign and clear composition cells by measure name",
          "[core][command][handler][composition]")
{
    auto state = make_plugin_state();

    CHECK(execute(state, "composition cell assign 0 0 M1").status.first ==
          MessageLevel::Info);
    CHECK(state.timeline.get_state().composition.rows[0].cells[0] ==
          DEFAULT_MEASURE_ID);

    CHECK(execute(state, "composition column insert after 0").status.first ==
          MessageLevel::Info);
    CHECK(execute(state, "composition cell assign 0 1 \"Verse\"").status.first ==
          MessageLevel::Info);
    auto project = state.timeline.get_state();
    auto const verse_id = project.composition.rows[0].cells[1];
    REQUIRE(verse_id.has_value());
    CHECK(verse_id != DEFAULT_MEASURE_ID);
    auto const verse_entry = std::ranges::find(project.measure_bank.measures, *verse_id,
                                               &MeasureBankEntry::id);
    REQUIRE(verse_entry != project.measure_bank.measures.end());
    REQUIRE(verse_entry->name.has_value());
    CHECK(*verse_entry->name == "Verse");

    CHECK(execute(state, "composition cell assign 0 0 \"Intro Copy\"").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    auto const copy_id = project.composition.rows[0].cells[0];
    REQUIRE(copy_id.has_value());
    CHECK(copy_id != DEFAULT_MEASURE_ID);
    auto const copy_entry = std::ranges::find(project.measure_bank.measures, *copy_id,
                                              &MeasureBankEntry::id);
    REQUIRE(copy_entry != project.measure_bank.measures.end());
    REQUIRE(copy_entry->name.has_value());
    CHECK(*copy_entry->name == "Intro Copy");

    CHECK(execute(state, "composition cell assign 0 0 verse").status.first ==
          MessageLevel::Info);
    project = state.timeline.get_state();
    CHECK(project.composition.rows[0].cells[0] == verse_id);
    CHECK(project.measure_bank.measures.size() == 3);

    CHECK_THROWS_AS((void)execute(state, "composition cell assign 0 0 \"\""),
                    std::invalid_argument);

    CHECK(execute(state, "composition cell clear 0 0").status.first ==
          MessageLevel::Info);
    CHECK_FALSE(state.timeline.get_state().composition.rows[0].cells[0].has_value());
}

TEST_CASE("Direct handlers validate without retaining partial mutation",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto const before = state.timeline.get_state();

    auto const result = execute(state, "set translateDirection sideways");
    CHECK(result.status.first == MessageLevel::Error);
    CHECK(result.status.second == "Invalid TranslateDirection: sideways");
    CHECK(state.timeline.get_state() == before);
}

TEST_CASE("Direct edit handlers use execution-context selection",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto engine = state.timeline.get_state();
    default_measure(engine).cell.elements = {
        sequence::Note{1, 0.5f, 0.f, 1.f},
    };
    state.timeline.stage(std::move(engine));

    auto const selection = select_element_in_cell({}, 0);

    CHECK(execute(state, "note 12 0.5 0.25 0.75", selection).status.first ==
          MessageLevel::Info);
    auto const after_note = state.timeline.get_state();
    auto const &created =
        std::get<sequence::Note>(default_measure(after_note).cell.elements[0]);
    CHECK(created.pitch == 12);

    CHECK(execute(state, "delete", selection).status.first == MessageLevel::Info);
    CHECK(default_measure(state.timeline.get_state()).cell.elements.empty());
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
    default_measure(engine).cell.elements = {
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
    engine.pitch.transposition = 1;
    state.timeline.stage(engine);
    REQUIRE(state.timeline.commit(state.timeline.get_state()));
    engine.pitch.transposition = 2;
    state.timeline.stage(engine);
    REQUIRE(state.timeline.commit(state.timeline.get_state()));
    CHECK(state.timeline.undo());
    CHECK(state.timeline.get_state().pitch.transposition == 1);
    CHECK(state.timeline.redo());
    CHECK(state.timeline.get_state().pitch.transposition == 2);
}
