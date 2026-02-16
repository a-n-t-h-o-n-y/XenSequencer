#include <catch2/catch_test_macros.hpp>

#include <array>
#include <stdexcept>
#include <string>

#include <sequence/measure.hpp>
#include <sequence/sequence.hpp>

#include <xen/scale.hpp>
#include <xen/serialize.hpp>

TEST_CASE("Cell and Measure serialization round-trip", "[unit][serialization]")
{
    auto const cell = sequence::Cell{
        .element = sequence::Note{.pitch = 7, .velocity = 0.6f, .delay = 0.1f, .gate = 0.8f},
        .weight = 0.75f,
    };

    auto const cell_roundtrip = xen::deserialize_cell(xen::serialize_cell(cell));
    REQUIRE(cell_roundtrip == cell);

    auto const measure = sequence::Measure{
        .cell = sequence::Cell{.element = sequence::Sequence{{cell}}, .weight = 1.0f},
        .time_signature = {7, 8},
    };

    auto const measure_roundtrip =
        xen::deserialize_measure(xen::serialize_measure(measure));
    REQUIRE(measure_roundtrip == measure);
}

TEST_CASE("Sequence bank serialization round-trip", "[unit][serialization]")
{
    auto bank = xen::SequenceBank{};
    bank[0].cell = sequence::Cell{.element = sequence::Note{.pitch = 3}, .weight = 1.0f};

    auto names = std::array<std::string, 16>{};
    names[0] = "intro";
    names[1] = "verse";

    auto const payload = xen::serialize_sequence_bank(bank, names);
    auto const [bank2, names2] = xen::deserialize_sequence_bank(payload);

    REQUIRE(bank2 == bank);
    REQUIRE(names2 == names);
}

TEST_CASE("SequencerState serialization round-trip", "[unit][serialization]")
{
    auto state = xen::SequencerState{};
    state.sequence_bank[0].cell =
        sequence::Cell{.element = sequence::Note{.pitch = 11, .velocity = 0.8f,
                                                  .delay = 0.2f, .gate = 0.9f},
                       .weight = 1.0f};
    state.sequence_names[0] = "lead";
    state.tuning_name = "custom";
    state.key = 4;
    state.base_frequency = 432.0f;
    state.scale_translate_direction = xen::TranslateDirection::Down;
    state.scale = xen::Scale{
        .name = "major",
        .tuning_length = 12,
        .intervals = {2, 2, 1, 2, 2, 2, 1},
        .mode = 1,
    };

    auto const serialized = xen::serialize_plugin(state);
    auto const roundtrip = xen::deserialize_plugin(serialized);

    REQUIRE(roundtrip == state);
}

TEST_CASE("deserialize_cell throws on unknown cell type", "[unit][serialization]")
{
    auto const invalid = R"({"type":"NotACell","weight":1.0})";
    REQUIRE_THROWS_AS(xen::deserialize_cell(invalid), std::invalid_argument);
}
