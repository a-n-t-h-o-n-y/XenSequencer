#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include <nlohmann/json.hpp>

#include <sequence/sequence.hpp>

#include <xen/actions.hpp>
#include <xen/midi_compiler.hpp>
#include <xen/modulation.hpp>
#include <xen/scale.hpp>
#include <xen/serialize.hpp>

namespace
{

[[nodiscard]] auto valid_scale() -> xen::Scale
{
    return {
        .name = "major",
        .tuning_length = 12,
        .intervals = {2, 2, 1, 2, 2, 2, 1},
        .mode = 1,
    };
}

[[nodiscard]] auto one_note_cell(int pitch) -> sequence::Cell
{
    return {
        .elements = {sequence::Note{.pitch = pitch}},
        .weight = 1.f,
    };
}

} // namespace

TEST_CASE("Scale validation rejects broken invariants", "[numeric][scale]")
{
    auto scale = valid_scale();
    CHECK_NOTHROW(xen::validate_scale(scale));

    scale.tuning_length = 0;
    CHECK_THROWS_AS(xen::validate_scale(scale), std::invalid_argument);

    scale = valid_scale();
    scale.tuning_length = static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1;
    CHECK_THROWS_AS(xen::validate_scale(scale), std::invalid_argument);

    scale = valid_scale();
    scale.intervals.clear();
    CHECK_THROWS_AS(xen::validate_scale(scale), std::invalid_argument);

    scale = valid_scale();
    scale.intervals.assign(256, 1);
    CHECK_THROWS_AS(xen::validate_scale(scale), std::invalid_argument);

    scale = valid_scale();
    scale.intervals[0] = 0;
    CHECK_THROWS_AS(xen::validate_scale(scale), std::invalid_argument);

    scale = valid_scale();
    scale.mode = 0;
    CHECK_THROWS_AS(xen::validate_scale(scale), std::invalid_argument);

    scale = valid_scale();
    scale.mode = 8;
    CHECK_THROWS_AS(xen::validate_scale(scale), std::invalid_argument);
}

TEST_CASE("Scale mapping validates preconditions and checked arithmetic",
          "[numeric][scale]")
{
    CHECK_THROWS_AS(xen::map_pitch_to_scale(0, {}, 12, xen::TranslateDirection::Up),
                    std::invalid_argument);
    CHECK_THROWS_AS(xen::map_pitch_to_scale(0, {0}, 0, xen::TranslateDirection::Up),
                    std::invalid_argument);
    CHECK_THROWS_AS(xen::map_pitch_to_scale(
                        0, {0},
                        static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1,
                        xen::TranslateDirection::Up),
                    std::invalid_argument);
    CHECK_THROWS_AS(xen::map_pitch_to_scale(std::numeric_limits<int>::max(),
                                            {std::numeric_limits<int>::max()}, 1,
                                            xen::TranslateDirection::Up),
                    std::overflow_error);
}

TEST_CASE("Plugin state loading rejects scale values before narrowing",
          "[numeric][scale][serialize]")
{
    auto state = xen::ProjectState{};
    state.composition.columns.at(0).pitch.scale =
        xen::ActiveScale{.source_id = std::nullopt, .definition = valid_scale()};
    auto json = nlohmann::json::parse(xen::serialize_project(state));

    json["project"]["composition"]["columns"][0]["pitch"]["scale"]["definition"]
        ["intervals"][0] = 256;
    CHECK_THROWS_AS(xen::deserialize_project(json.dump()), std::invalid_argument);

    json = nlohmann::json::parse(xen::serialize_project(state));
    json["project"]["composition"]["columns"][0]["pitch"]["scale"]["definition"]
        ["mode"] = 257;
    CHECK_THROWS_AS(xen::deserialize_project(json.dump()), std::invalid_argument);
}

TEST_CASE("Action pitch arithmetic rejects overflow and empty modulo inputs",
          "[numeric][actions]")
{
    auto const all = sequence::Pattern{0, {1}};
    auto engine = xen::ProjectState{};
    selected_sequence(engine, xen::CompositionCursor{}) =
        one_note_cell(std::numeric_limits<int>::max());
    CHECK_THROWS_AS(xen::action::shift_octave(engine, xen::CompositionCursor{},
                                              xen::SelectionPath{}, all, 1),
                    std::overflow_error);
    CHECK_THROWS_AS(xen::action::arp(one_note_cell(0), all, {}), std::invalid_argument);
    CHECK_THROWS_AS(
        xen::action::chord(one_note_cell(std::numeric_limits<int>::max()), {1}, 12),
        std::overflow_error);

    auto scale = valid_scale();
    CHECK_NOTHROW(
        xen::action::shift_scale_mode(scale, std::numeric_limits<int>::min()));
    CHECK_THROWS_AS(xen::action::shift_scale_index(
                        std::nullopt, 1, std::numeric_limits<std::size_t>::max()),
                    std::overflow_error);

    engine = xen::ProjectState{};
    selected_sequence(engine, xen::CompositionCursor{}) = one_note_cell(0);
    CHECK_THROWS_AS(xen::action::set_note_octave(engine, xen::CompositionCursor{},
                                                 xen::SelectionPath{}, all,
                                                 std::numeric_limits<int>::max()),
                    std::overflow_error);
}

TEST_CASE("Modulation rejects invalid definitions and output ranges",
          "[numeric][modulation]")
{
    auto const nan = std::numeric_limits<float>::quiet_NaN();
    auto modulation = xen::ModulationDefinition{
        .waveforms = {{.frequency = nan}},
    };
    CHECK_THROWS_AS(xen::evaluate(modulation, 16), std::invalid_argument);

    modulation = {
        .operation = xen::ModulationOperation::FrequencyModulation,
        .waveforms = {{}, {}, {}},
    };
    CHECK_THROWS_AS(xen::evaluate(modulation, 16), std::invalid_argument);

    CHECK_THROWS_AS(
        xen::validate(xen::ModulationDestination::Weight,
                      xen::ModulationOutputRange{.minimum = 0.0, .maximum = 1.0}),
        std::invalid_argument);
    CHECK_THROWS_AS(
        xen::validate(xen::ModulationDestination::Pitch,
                      xen::ModulationOutputRange{.minimum = 0.5, .maximum = 12.0}),
        std::invalid_argument);
}

TEST_CASE("Modulation evaluates normalized reducers and sampled FM",
          "[numeric][modulation]")
{
    auto const sine = xen::ModulationDefinition{
        .waveforms = {{.frequency = 1.f}},
    };
    auto const sine_output = xen::evaluate(sine, 4);
    REQUIRE(sine_output.size() == 4);
    CHECK(sine_output[0] == Catch::Approx(0.5f));
    CHECK(sine_output[1] == Catch::Approx(1.f));
    CHECK(sine_output[2] == Catch::Approx(0.5f));
    CHECK(sine_output[3] == Catch::Approx(0.f));

    auto const fm = xen::ModulationDefinition{
        .operation = xen::ModulationOperation::FrequencyModulation,
        .waveforms = {{.frequency = 0.f},
                      {.frequency = 0.f, .amplitude = 0.f, .amplitude_offset = 1.f}},
    };
    auto const fm_output = xen::evaluate(fm, 4);
    REQUIRE(fm_output.size() == 4);
    CHECK(fm_output[0] == Catch::Approx(0.5f));
    CHECK(fm_output[1] == Catch::Approx(1.f));
    CHECK(fm_output[2] == Catch::Approx(0.5f));
    CHECK(fm_output[3] == Catch::Approx(0.f));
}

TEST_CASE("MIDI compiler rejects unrepresentable composition timing", "[numeric][midi]")
{
    auto project = xen::ProjectState{};
    project.composition.default_column.duration.denominator = 0;
    CHECK_THROWS_AS(xen::MidiCompiler::compile(project, xen::DEFAULT_CHANNEL_ID, 1),
                    std::invalid_argument);
}
