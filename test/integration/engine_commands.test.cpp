#include <catch2/catch_test_macros.hpp>

#include <sequence/random.hpp>
#include <sequence/sequence.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/engine.hpp>
#include <xen/runtime_command_tree.hpp>
#include <xen/runtime_state.hpp>
#include <xen/selection.hpp>
#include <xen/serialize.hpp>

namespace
{
auto selected_note_pitch(xen::Engine const &engine) -> int
{
    auto const tracked = engine.state().timeline.get_state();
    auto const &cell =
        xen::get_selected_cell_const(tracked.sequencer.sequence_bank, tracked.aux.selected);
    return std::get<sequence::Note>(cell.element).pitch;
}

auto run_randomized_pitch(std::uint32_t seed) -> int
{
    auto engine = xen::Engine{};
    sequence::random::set_seed(seed);

    auto const create_status = engine.execute_command("note 0");
    REQUIRE(create_status.first == xen::MessageLevel::Info);

    auto const random_status = engine.execute_command("randomize pitch -12 12");
    REQUIRE(random_status.first == xen::MessageLevel::Info);

    auto const pitch = selected_note_pitch(engine);
    sequence::random::clear_seed();
    return pitch;
}
} // namespace

TEST_CASE("Randomized engine commands are reproducible with fixed seed",
          "[integration][engine][random]")
{
    auto const first = run_randomized_pitch(1337);
    auto const second = run_randomized_pitch(1337);

    REQUIRE(first == second);
}

TEST_CASE("Bridge routing keeps runtime commands out of engine state",
          "[integration][bridge]")
{
    auto engine = xen::Engine{};
    auto runtime = xen::RuntimeState{};
    auto runtime_tree = xen::RuntimeCommandTree{};

    auto const dispatch = [&](std::string const &command) {
        if (auto runtime_result = runtime_tree.execute(runtime, command);
            runtime_result.has_value())
        {
            return *runtime_result;
        }
        return engine.execute_command(command);
    };

    auto const before = engine.state().timeline.get_state().sequencer;

    auto const runtime_status = dispatch("focus command_bar");
    REQUIRE(runtime_status.first == xen::MessageLevel::Debug);
    REQUIRE(engine.state().timeline.get_state().sequencer == before);

    auto const engine_status = dispatch("note 5");
    REQUIRE(engine_status.first == xen::MessageLevel::Info);
    REQUIRE(engine.state().timeline.get_state().sequencer != before);
}

TEST_CASE("Full-state snapshot contract round-trips after command mutation",
          "[integration][bridge][serialization]")
{
    auto engine = xen::Engine{};

    auto const before_json =
        xen::serialize_plugin(engine.state().timeline.get_state().sequencer);

    auto const status = engine.execute_command("note 7");
    REQUIRE(status.first == xen::MessageLevel::Info);

    auto const sequencer_state = engine.state().timeline.get_state().sequencer;
    auto const after_json = xen::serialize_plugin(sequencer_state);

    REQUIRE(after_json != before_json);
    REQUIRE(xen::deserialize_plugin(after_json) == sequencer_state);
}
