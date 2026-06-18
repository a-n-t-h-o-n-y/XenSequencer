#include <string>
#include <variant>
#include <vector>

#include <sequence/sequence.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <xen/message_level.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto check_editor_stable(EngineSnapshot const &after, EngineSnapshot const &before)
    -> void
{
    CHECK(after.editor.selected == before.editor.selected);
    CHECK(after.editor.input_mode == before.editor.input_mode);
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

auto singleton_sequence_measure(std::vector<std::size_t> const &indices) -> Measure
{
    auto measure = Measure{};
    auto *cell = &measure.cell;
    for (auto const index : indices)
    {
        cell->elements = {
            sequence::Sequence{.cells = std::vector<sequence::Cell>(index + 1)},
        };
        auto &sequence = std::get<sequence::Sequence>(cell->elements.front());
        cell = &sequence.cells[index];
    }
    return measure;
}

} // namespace

TEST_CASE("Move up changes selection path and clears nested cell selection",
          "[core][actions]")
{
    auto processor = XenProcessor{};
    auto state = processor.plugin_state.timeline.get_state();
    state.measure = singleton_sequence_measure({0, 0});
    processor.plugin_state.timeline.stage(std::move(state));
    processor.plugin_state.editor.selected = singleton_sequence_cell_selection({0, 0});

    auto before = processor.get_engine_snapshot();
    REQUIRE_FALSE(before.editor.selected.path.empty());

    auto const [level, _message] = processor.execute_command_string("move up");
    CHECK(level == MessageLevel::Debug);

    auto const after = processor.get_engine_snapshot();
    CHECK(after.editor.selected == singleton_sequence_cell_selection({0}));
}

TEST_CASE("Base frequency command clamps to supported range", "[core][actions]")
{
    auto processor = XenProcessor{};

    REQUIRE(processor.execute_command_string("set baseFrequency -5").first ==
            MessageLevel::Info);
    CHECK(processor.get_engine_snapshot().engine.base_frequency == Catch::Approx(20.f));

    REQUIRE(processor.execute_command_string("set baseFrequency 50000").first ==
            MessageLevel::Info);
    CHECK(processor.get_engine_snapshot().engine.base_frequency ==
          Catch::Approx(20'000.f));
}

TEST_CASE("Set key validates range and does not mutate on invalid input",
          "[core][actions]")
{
    auto processor = XenProcessor{};
    auto const before = processor.get_engine_snapshot();

    auto const [invalid_level, invalid_message] =
        processor.execute_command_string("set key 128");
    auto const after_invalid = processor.get_engine_snapshot();

    CHECK(invalid_level == MessageLevel::Error);
    CHECK(invalid_message == "Invalid Key Value: 128. Must be in range [-127, 127].");
    CHECK(after_invalid.engine == before.engine);
    check_editor_stable(after_invalid, before);
    CHECK(after_invalid.commit_id == before.commit_id);

    auto const [valid_level, _valid_message] =
        processor.execute_command_string("set key -127");
    CHECK(valid_level == MessageLevel::Info);
    CHECK(processor.get_engine_snapshot().engine.key == -127);
}

TEST_CASE("Set measure timeSignature validates values", "[core][actions]")
{
    auto processor = XenProcessor{};
    auto const before = processor.get_engine_snapshot();

    auto const [level, _message] =
        processor.execute_command_string("set measure timeSignature 7/8");
    REQUIRE(level == MessageLevel::Info);

    auto const after_valid = processor.get_engine_snapshot();
    CHECK(after_valid.engine.measure.time_signature.numerator == 7);
    CHECK(after_valid.engine.measure.time_signature.denominator == 8);

    auto const before_invalid = after_valid;
    auto const [invalid_level, invalid_message] =
        processor.execute_command_string("set measure timeSignature 0/4");
    auto const after_invalid = processor.get_engine_snapshot();

    CHECK(invalid_level == MessageLevel::Error);
    CHECK(invalid_message == "Invalid TimeSignature");
    CHECK(after_invalid.engine == before_invalid.engine);
    check_editor_stable(after_invalid, before_invalid);
    CHECK(after_invalid.commit_id == before_invalid.commit_id);
}
