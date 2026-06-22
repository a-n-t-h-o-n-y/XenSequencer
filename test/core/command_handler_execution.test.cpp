#include <stdexcept>
#include <string>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>

#include <xen/chord.hpp>
#include <xen/command.hpp>
#include <xen/command_catalog.hpp>
#include <xen/selection.hpp>
#include <xen/state.hpp>
#include <xen/submission_effects.hpp>

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
    auto effects = SubmissionEffects{};
    auto context = CommandExecutionContext{.selection = std::move(selection)};
    return command.execute(state, effects, context);
}

} // namespace

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
