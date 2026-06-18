#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>

#include <xen/chord.hpp>
#include <xen/command.hpp>
#include <xen/command_catalog.hpp>
#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/state.hpp>

using namespace xen;

namespace
{

auto make_plugin_state() -> PluginState
{
    return PluginState{
        .timeline = XenTimeline{TimelineState{.sequencer = {}, .aux = {}}},
    };
}

auto execute(PluginState &state, ExecutionContext context, std::string const &text)
    -> CommandExecutionResult
{
    auto const result = bind_invocation(parse_command_chain(text).front());
    REQUIRE(std::holds_alternative<BoundCommand>(result));
    auto const &command = std::get<BoundCommand>(result);
    REQUIRE(command.control == BoundCommandControl::Execute);
    REQUIRE(command.execute);
    return command.execute(state, std::move(context));
}

auto singleton_sequence_cell_selection(std::vector<std::size_t> const &indices)
    -> SelectedState
{
    auto selected = SelectedState{};
    for (auto const index : indices)
    {
        selected.path.push_back({.kind = SelectionStepKind::Element, .index = 0});
        selected.path.push_back(
            {.kind = SelectionStepKind::SequenceCell, .index = index});
    }
    return selected;
}

} // namespace

TEST_CASE("Direct handlers report context, mutation, and commit intent",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto context = ExecutionContext{};
    context.input_mode = InputMode::Velocity;

    auto const key = execute(state, context, "set key 12");
    CHECK(key.status.second == "Key Set to 12.");
    CHECK(key.context.input_mode == InputMode::Velocity);
    CHECK(key.engine_mutated);
    CHECK(key.commit_intent == CommitIntent::Auto);

    auto const move = execute(state, key.context, "move right");
    CHECK(move.status.second == "Moved Right 1 Times");
    CHECK_FALSE(move.engine_mutated);

    auto const commit = execute(state, move.context, "commit");
    CHECK(commit.commit_intent == CommitIntent::Force);

    auto selected = state.timeline.get_state();
    selected.sequencer.measure.cell.elements = {
        sequence::Note{1, 0.5f, 0.f, 1.f},
    };
    state.timeline.stage(std::move(selected));
    auto const weights = execute(state, commit.context, "+0 set weights 0.5");
    CHECK(weights.commit_intent == CommitIntent::Defer);
}

TEST_CASE("Direct handlers validate keys and time signatures",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    CHECK(execute(state, {}, "set key 128").status.first == MessageLevel::Error);
    CHECK(execute(state, {}, "set measure timeSignature 0/4").status.second ==
          "Invalid TimeSignature");
    CHECK(execute(state, {}, "set measure timeSignature 65/1").status.second ==
          "TimeSignature Too Large, Max length is 64 Whole Notes.");
}

TEST_CASE("Direct edit handlers create and delete notes", "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto timeline_state = state.timeline.get_state();
    timeline_state.aux.selected = singleton_sequence_cell_selection({0});
    timeline_state.sequencer.measure.cell.elements = {
        sequence::Sequence{.cells = {sequence::Cell{
                               .elements = {},
                               .weight = 0.37f,
                           }}},
    };
    state.timeline.stage(std::move(timeline_state));
    auto const context = state.timeline.get_state().aux;

    auto const note = execute(state, context, "note 12 0.5 0.25 0.75");
    CHECK(note.engine_mutated);
    auto const state_after_note = state.timeline.get_state();
    auto const &cell =
        get_selected_cell_const(state_after_note.sequencer.measure, context.selected);
    REQUIRE(cell.elements.size() == 1);
    auto const &created = std::get<sequence::Note>(cell.elements.front());
    CHECK(created.pitch == 12);
    CHECK(created.velocity == Catch::Approx(0.5f));

    auto const deleted = execute(state, note.context, "delete");
    CHECK(deleted.engine_mutated);
    auto const state_after_delete = state.timeline.get_state();
    CHECK(get_selected_cell_const(state_after_delete.sequencer.measure,
                                  deleted.context.selected)
              .elements.empty());
}

TEST_CASE("Direct transform handlers execute parsed defaults and arguments",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    auto timeline_state = state.timeline.get_state();
    timeline_state.sequencer.measure.cell.elements = {
        sequence::Note{5, 0.5f, 0.2f, 0.8f},
    };
    state.timeline.stage(std::move(timeline_state));

    CHECK(execute(state, {}, "+0 randomize pitch 4 4").status.first ==
          MessageLevel::Info);
    CHECK(execute(state, {}, "+0 stretch").status.second == "Stretched Selection by 2");
    CHECK(execute(state, {}, "+1 compress").status.second == "Compressed Selection");
    CHECK(execute(state, {}, "rotate -1").status.second == "Selection Rotated");
    CHECK(execute(state, {}, "+0 mirror 10").status.second == "Selection Mirrored");
}

TEST_CASE("Direct chord handler preserves cycle state", "[core][command][handler]")
{
    auto state = make_plugin_state();
    state.library.chords = {
        Chord{.name = "major", .intervals = {0, 4, 7}},
        Chord{.name = "minor", .intervals = {0, 3, 7}},
    };
    auto timeline_state = state.timeline.get_state();
    timeline_state.sequencer.measure.cell.elements = {
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
    };
    state.timeline.stage(std::move(timeline_state));

    auto first = execute(state, {}, "chord major 0");
    CHECK(first.status.second == "Chorded with major inversion: 0");
    auto second = execute(state, first.context, "chord");
    CHECK(second.status.second == "Chorded with major inversion: 1");
    auto third = execute(state, second.context, "chord cycle 0");
    CHECK(third.status.second == "Chorded with minor inversion: 0");
}

TEST_CASE("Direct chord handler requires whole-cell selection",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    state.library.chords = {
        Chord{.name = "major", .intervals = {0, 4, 7}},
    };
    auto timeline_state = state.timeline.get_state();
    timeline_state.sequencer.measure.cell.elements = {
        sequence::Note{10, 0.5f, 0.1f, 0.8f},
    };
    timeline_state.aux.selected = select_element_in_cell({}, 0);
    state.timeline.stage(std::move(timeline_state));
    CHECK_THROWS_AS(execute(state, state.timeline.get_state().aux, "chord major 0"),
                    std::runtime_error);
}

TEST_CASE("Direct scale, file metadata, undo, and redo handlers execute",
          "[core][command][handler]")
{
    auto state = make_plugin_state();
    state.library.scales = {
        Scale{.name = "major",
              .tuning_length = 12,
              .intervals = {2, 2, 1, 2, 2, 2, 1},
              .mode = 1},
    };

    CHECK(execute(state, {}, "set scale major").status.first == MessageLevel::Info);
    CHECK(execute(state, {}, "set mode 2").status.first == MessageLevel::Info);
    CHECK(execute(state, {}, "load keys").status.first == MessageLevel::Warning);
    CHECK(execute(state, {}, "libraryDirectory").status.first == MessageLevel::Info);

    auto timeline_state = state.timeline.get_state();
    timeline_state.sequencer.key = 1;
    state.timeline.stage(std::move(timeline_state));
    state.timeline.commit();
    timeline_state = state.timeline.get_state();
    timeline_state.sequencer.key = 2;
    state.timeline.stage(std::move(timeline_state));
    state.timeline.commit();

    auto const undo = execute(state, {}, "undo");
    CHECK(undo.status.second == "Undone");
    CHECK(state.timeline.get_state().sequencer.key == 1);
    auto const redo = execute(state, undo.context, "redo");
    CHECK(redo.status.second == "Redone");
    CHECK(state.timeline.get_state().sequencer.key == 2);
}
