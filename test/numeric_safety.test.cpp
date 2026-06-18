#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <nlohmann/json.hpp>

#include <sequence/sequence.hpp>

#include <xen/actions.hpp>
#include <xen/midi.hpp>
#include <xen/midi_engine.hpp>
#include <xen/midi_internal.hpp>
#include <xen/modulator.hpp>
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
    auto state = xen::EngineState{};
    state.scale = valid_scale();
    auto json = nlohmann::json::parse(xen::serialize_plugin(state));

    json["scale"]["intervals"][0] = 256;
    CHECK_THROWS_AS(xen::deserialize_plugin(json.dump()), std::invalid_argument);

    json = nlohmann::json::parse(xen::serialize_plugin(state));
    json["scale"]["mode"] = 257;
    CHECK_THROWS_AS(xen::deserialize_plugin(json.dump()), std::invalid_argument);
}

TEST_CASE("Action pitch arithmetic rejects overflow and empty modulo inputs",
          "[numeric][actions]")
{
    auto const all = sequence::Pattern{0, {1}};
    auto engine = xen::EngineState{};
    engine.measure.cell = one_note_cell(std::numeric_limits<int>::max());
    CHECK_THROWS_AS(
        xen::action::shift_octave(engine, xen::EditorSessionState{}, all, 1),
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

    engine = xen::EngineState{};
    engine.measure.cell = one_note_cell(0);
    CHECK_THROWS_AS(xen::action::set_note_octave(engine, xen::EditorSessionState{}, all,
                                                 std::numeric_limits<int>::max()),
                    std::overflow_error);
}

TEST_CASE("Modulators reject invalid wavetable and action outputs",
          "[numeric][modulator]")
{
    auto const nan = std::numeric_limits<float>::quiet_NaN();
    auto const infinity = std::numeric_limits<float>::infinity();

    CHECK_THROWS_AS(xen::evaluate(xen::modulator::Sine{.frequency = nan}, 0.f),
                    std::invalid_argument);
    CHECK_THROWS_AS(xen::evaluate(xen::modulator::Sine{.frequency = infinity},
                                  std::numeric_limits<float>::max()),
                    std::invalid_argument);
    CHECK_THROWS_AS(xen::evaluate(
                        xen::modulator::Sine{
                            .frequency = std::numeric_limits<float>::max(),
                            .amplitude = 1.f,
                            .phase = 0.f,
                        },
                        std::numeric_limits<float>::max()),
                    std::overflow_error);

    auto const sequence_cell = sequence::Cell{
        .elements = {sequence::Sequence{{one_note_cell(0)}}},
        .weight = 1.f,
    };
    auto const all = sequence::Pattern{0, {1}};
    CHECK_THROWS_AS(
        xen::action::set_pitches(sequence_cell, all,
                                 xen::modulator::Constant{.value = infinity}),
        std::invalid_argument);
    CHECK_THROWS_AS(xen::action::set_weights(sequence_cell, all,
                                             xen::modulator::Constant{.value = nan}),
                    std::invalid_argument);
    CHECK_THROWS_AS(xen::action::set_weights(sequence_cell, all,
                                             xen::modulator::Constant{.value = 0.f}),
                    std::invalid_argument);
}

TEST_CASE("MIDI timing rejects unsupported signed sample positions", "[numeric][midi]")
{
    auto const too_large =
        static_cast<std::uint32_t>(std::numeric_limits<int>::max()) + 1U;
    auto const timeline = std::vector<sequence::midi::TimedMidiNote>{
        {.begin = too_large,
         .end = too_large + 1U,
         .note = 60,
         .velocity = 100,
         .pitch_bend = 8'192},
    };
    CHECK_THROWS_AS(xen::render_to_midi(timeline), std::overflow_error);

    auto const buffer = juce::MidiBuffer{};
    CHECK_THROWS_AS(xen::extract_window(buffer, 0, 0, 1), std::invalid_argument);
    CHECK_THROWS_AS(
        xen::extract_window(
            buffer, static_cast<xen::SampleCount>(std::numeric_limits<int>::max()) + 1,
            0, 1),
        std::invalid_argument);
    CHECK_THROWS_AS(
        xen::extract_window(
            buffer, 1, 0,
            static_cast<xen::SampleIndex>(std::numeric_limits<int>::max()) + 1),
        std::overflow_error);
    CHECK_NOTHROW(xen::extract_window(
        buffer, 64, std::numeric_limits<xen::SampleIndex>::max() - 100,
        std::numeric_limits<xen::SampleIndex>::max() - 50));

    CHECK_THROWS_AS(xen::midi_internal::checked_measure_sample_count(
                        sequence::TimeSignature{4, 4},
                        std::numeric_limits<std::uint32_t>::max(), 1.f),
                    std::overflow_error);
}

TEST_CASE("MIDI engine rejects overflowing absolute processing windows",
          "[numeric][midi]")
{
    auto engine = xen::MidiEngine{};
    auto sequencer = xen::EngineState{};
    auto const daw = xen::DAWState{
        .bpm = 120.f,
        .sample_rate = 44'100,
        .is_playing = true,
    };
    engine.update(sequencer, daw);

    CHECK_THROWS_AS(
        engine.step({}, std::numeric_limits<xen::SampleIndex>::max(), 1, daw),
        std::overflow_error);
}
