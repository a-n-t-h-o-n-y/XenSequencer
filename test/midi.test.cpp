#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/midi.hpp>
#include <sequence/sequence.hpp>
#include <sequence/timing.hpp>
#include <sequence/tuning.hpp>

#include <xen/midi.hpp>
#include <xen/state.hpp>

TEST_CASE("state_to_timeline returns timed midi notes", "[midi]")
{
    auto const measure = xen::Measure{
        .cell =
            {
                .elements = {sequence::Sequence{{
                    {.elements = {sequence::Note{.pitch = 0, .velocity = 0.5f}},
                     .weight = 1.f},
                    {.elements = {sequence::Note{.pitch = 4, .velocity = 0.75f}},
                     .weight = 1.f},
                }}},
                .weight = 1.f,
            },
        .time_signature = sequence::TimeSignature{4, 4},
    };
    auto const tuning = sequence::Tuning{
        .intervals = {0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100},
        .octave = 1200,
        .description = "12-TET",
    };
    auto const daw_state = xen::DAWState{.bpm = 120.f, .sample_rate = 44'100};

    auto const timeline =
        xen::state_to_timeline(measure, tuning, 440.f, daw_state, std::nullopt, 0,
                               xen::TranslateDirection::Up);

    auto const expected = sequence::midi::flatten_to_midi(
        measure.cell.elements, 0,
        sequence::samples_count(measure.time_signature, daw_state.sample_rate,
                                daw_state.bpm),
        tuning, 440.f, 48.f);

    REQUIRE(timeline == expected);
}

TEST_CASE("render_to_midi emits pitch bend before note on", "[midi]")
{
    auto const timeline = std::vector<sequence::midi::TimedMidiNote>{
        {.begin = 12, .end = 34, .note = 69, .velocity = 101, .pitch_bend = 9'000},
    };

    auto const midi = xen::render_to_midi(timeline);

    auto it = midi.begin();
    REQUIRE(it != midi.end());
    auto const first = it->getMessage();
    CHECK(it->samplePosition == 12);
    CHECK(first.isPitchWheel());
    CHECK(first.getChannel() == 1);
    CHECK(first.getPitchWheelValue() == 9'000);

    ++it;
    REQUIRE(it != midi.end());
    auto const second = it->getMessage();
    CHECK(it->samplePosition == 12);
    CHECK(second.isNoteOn());
    CHECK(second.getChannel() == 1);
    CHECK(second.getNoteNumber() == 69);
    CHECK(second.getVelocity() == Catch::Approx(101.0f / 127.0f));

    ++it;
    REQUIRE(it != midi.end());
    auto const third = it->getMessage();
    CHECK(it->samplePosition == 34);
    CHECK(third.isNoteOff());
    CHECK(third.getChannel() == 1);
    CHECK(third.getNoteNumber() == 69);

    ++it;
    CHECK(it == midi.end());
}
