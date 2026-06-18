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

auto execute(PluginState &state, std::string const &text)
    -> std::pair<MessageLevel, std::string>
{
    auto const result = bind_invocation(parse_command_chain(text).front());
    REQUIRE(std::holds_alternative<BoundStep>(result));
    auto const &step = std::get<BoundStep>(result);
    REQUIRE(std::holds_alternative<ExecutableCommand>(step));
    auto const &command = std::get<ExecutableCommand>(step);
    auto effects = SubmissionEffects{};
    return command.execute(state, effects);
}

} // namespace

TEST_CASE("Direct handlers mutate engine and editor independently",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    state.editor.input_mode = InputMode::Velocity;

    CHECK(execute(state, "set key 12").second == "Key Set to 12.");
    CHECK(state.timeline.get_state().key == 12);
    CHECK(state.editor.input_mode == InputMode::Velocity);

    auto const engine_before_move = state.timeline.get_state();
    CHECK(execute(state, "move right").second == "Moved Right 1 Times");
    CHECK(state.timeline.get_state() == engine_before_move);
}

TEST_CASE("Direct handlers validate without retaining partial mutation",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto const before = state.timeline.get_state();

    CHECK(execute(state, "set key 128").first == MessageLevel::Error);
    CHECK(state.timeline.get_state() == before);
    CHECK(execute(state, "set measure timeSignature 0/4").second ==
          "Invalid TimeSignature");
    CHECK(state.timeline.get_state() == before);
}

TEST_CASE("Direct edit handlers use PluginState editor selection",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto engine = state.timeline.get_state();
    engine.measure.cell.elements = {
        sequence::Note{1, 0.5f, 0.f, 1.f},
    };
    state.timeline.stage(std::move(engine));
    state.editor.selected = select_element_in_cell({}, 0);

    CHECK(execute(state, "note 12 0.5 0.25 0.75").first == MessageLevel::Info);
    auto const after_note = state.timeline.get_state();
    auto const &created = std::get<sequence::Note>(after_note.measure.cell.elements[0]);
    CHECK(created.pitch == 12);

    CHECK(execute(state, "delete").first == MessageLevel::Info);
    CHECK(state.timeline.get_state().measure.cell.elements.empty());
}

TEST_CASE("Direct chord handler preserves editor cycle state",
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

    CHECK(execute(state, "chord major 0").second == "Chorded with major inversion: 0");
    CHECK(execute(state, "chord").second == "Chorded with major inversion: 1");
    CHECK(execute(state, "chord cycle 0").second == "Chorded with minor inversion: 0");
}

TEST_CASE("Direct undo and redo preserve editor state", "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto engine = state.timeline.get_state();
    engine.key = 1;
    state.timeline.stage(engine);
    REQUIRE(state.timeline.commit());
    engine.key = 2;
    state.timeline.stage(engine);
    REQUIRE(state.timeline.commit());
    state.editor.input_mode = InputMode::Gate;

    CHECK(execute(state, "undo").second == "Undone");
    CHECK(state.timeline.get_state().key == 1);
    CHECK(state.editor.input_mode == InputMode::Gate);
    CHECK(execute(state, "redo").second == "Redone");
    CHECK(state.timeline.get_state().key == 2);
    CHECK(state.editor.input_mode == InputMode::Gate);
}
