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
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto make_plugin_state() -> PluginState
{
    return PluginState{.timeline = XenTimeline{EngineState{}}};
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
    auto context = CommandExecutionContext{.selection = std::move(selection)};
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
    project_context.edit_project().key = 2;
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

TEST_CASE("Effect failures leave backend state unchanged and report rollback failures",
          "[core][command][transaction][effects]")
{
    auto const directory =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("xen-transaction-test", "", false);
    REQUIRE(directory.createDirectory());
    REQUIRE(directory.getChildFile("effect-test.xss").replaceWithText("baseline"));

    for (auto const failure : {SubmissionEffects::FailurePoint::Prepare,
                               SubmissionEffects::FailurePoint::Apply,
                               SubmissionEffects::FailurePoint::ApplyAndRollback})
    {
        auto processor = XenProcessor{failure};
        processor.plugin_state.config.current_sequence_directory = directory;
        auto const before = processor.get_engine_snapshot();
        auto const result = processor.execute_command_string(
            "save measure effect-test",
            {.expected_project_revision = before.project_revision});

        CHECK(result.status.first == MessageLevel::Error);
        CHECK(processor.get_engine_snapshot().project_revision ==
              before.project_revision);
        CHECK(processor.get_engine_snapshot().engine == before.engine);
        if (failure == SubmissionEffects::FailurePoint::ApplyAndRollback)
        {
            CHECK(result.status.second.find("rollback failed for:") !=
                  std::string::npos);
        }
    }

    CHECK(directory.deleteRecursively());
}

TEST_CASE("Direct handlers mutate engine without requiring session editor state",
          "[core][command][handler]")
{
    auto state = make_plugin_state();

    auto const result = execute(state, "set key 12");
    CHECK(result.status.second == "Key Set to 12.");
    CHECK(state.timeline.get_state().key == 12);
}

TEST_CASE("Direct handlers validate without retaining partial mutation",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto const before = state.timeline.get_state();

    CHECK(execute(state, "set key 128").status.first == MessageLevel::Error);
    CHECK(state.timeline.get_state() == before);
    CHECK(execute(state, "set measure timeSignature 0/4").status.second ==
          "Invalid TimeSignature");
    CHECK(state.timeline.get_state() == before);
}

TEST_CASE("Direct edit handlers use execution-context selection",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto engine = state.timeline.get_state();
    engine.measure.cell.elements = {
        sequence::Note{1, 0.5f, 0.f, 1.f},
    };
    state.timeline.stage(std::move(engine));

    auto const selection = select_element_in_cell({}, 0);

    CHECK(execute(state, "note 12 0.5 0.25 0.75", selection).status.first ==
          MessageLevel::Info);
    auto const after_note = state.timeline.get_state();
    auto const &created = std::get<sequence::Note>(after_note.measure.cell.elements[0]);
    CHECK(created.pitch == 12);

    CHECK(execute(state, "delete", selection).status.first == MessageLevel::Info);
    CHECK(state.timeline.get_state().measure.cell.elements.empty());
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
    engine.measure.cell.elements = {
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
    };
    state.timeline.stage(std::move(engine));

    auto const selection = SelectionPath{};

    CHECK(execute(state, "chord major 0", selection).status.second ==
          "Chorded with major inversion: 0");
    CHECK(state.sessions.chord_state.previous_chord_name == "major");
    CHECK(state.sessions.chord_state.previous_inversion == 0);
    CHECK(state.sessions.chord_state.previous_project_revision ==
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
    engine.key = 1;
    state.timeline.stage(engine);
    REQUIRE(state.timeline.commit(state.timeline.get_state()));
    engine.key = 2;
    state.timeline.stage(engine);
    REQUIRE(state.timeline.commit(state.timeline.get_state()));
    CHECK(state.timeline.undo());
    CHECK(state.timeline.get_state().key == 1);
    CHECK(state.timeline.redo());
    CHECK(state.timeline.get_state().key == 2);
}
