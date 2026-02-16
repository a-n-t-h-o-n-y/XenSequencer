#include <catch2/catch_test_macros.hpp>

#include <optional>

#include <sequence/sequence.hpp>

#include <xen/engine.hpp>
#include <xen/selection.hpp>

namespace
{
auto selected_cell(xen::Engine const &engine) -> sequence::Cell
{
    auto const tracked = engine.state().timeline.get_state();
    return xen::get_selected_cell_const(tracked.sequencer.sequence_bank, tracked.aux.selected);
}
} // namespace

TEST_CASE("Engine exposes pending sequencer updates", "[unit][engine]")
{
    auto engine = xen::Engine{};

    auto first = engine.take_pending_sequencer_update();
    REQUIRE(first.has_value());

    auto second = engine.take_pending_sequencer_update();
    REQUIRE_FALSE(second.has_value());

    auto const status = engine.execute_command("note 4");
    REQUIRE(status.first == xen::MessageLevel::Info);

    auto third = engine.take_pending_sequencer_update();
    REQUIRE(third.has_value());
}

TEST_CASE("Engine note command mutates state and undo/redo restores history", "[unit][engine]")
{
    auto engine = xen::Engine{};

    auto const note_status = engine.execute_command("note 12");
    REQUIRE(note_status.first == xen::MessageLevel::Info);

    auto const &note_cell = selected_cell(engine);
    REQUIRE(std::holds_alternative<sequence::Note>(note_cell.element));
    REQUIRE(std::get<sequence::Note>(note_cell.element).pitch == 12);

    auto const undo_status = engine.execute_command("undo");
    REQUIRE(undo_status.first == xen::MessageLevel::Info);
    REQUIRE(std::holds_alternative<sequence::Rest>(selected_cell(engine).element));

    auto const redo_status = engine.execute_command("redo");
    REQUIRE(redo_status.first == xen::MessageLevel::Info);
    REQUIRE(std::holds_alternative<sequence::Note>(selected_cell(engine).element));
    REQUIRE(std::get<sequence::Note>(selected_cell(engine).element).pitch == 12);
}

TEST_CASE("Engine reports unknown commands as errors", "[unit][engine]")
{
    auto engine = xen::Engine{};

    auto const result = engine.execute_command("definitely_not_a_real_command");
    REQUIRE(result.first == xen::MessageLevel::Error);
    REQUIRE(result.second == "Command not found: definitely_not_a_real_command");
}

TEST_CASE("Engine rolls back staged state when command throws", "[unit][engine]")
{
    auto engine = xen::Engine{};
    auto const before = engine.state().timeline.get_state().sequencer;

    auto const result = engine.execute_command("note 1 2.0");
    REQUIRE(result.first == xen::MessageLevel::Error);

    auto const after = engine.state().timeline.get_state().sequencer;
    REQUIRE(after == before);
}
