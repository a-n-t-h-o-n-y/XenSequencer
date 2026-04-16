#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/midi.hpp>
#include <sequence/sequence.hpp>
#include <sequence/timing.hpp>
#include <sequence/tuning.hpp>

#include <xen/midi.hpp>
#include <xen/state.hpp>

namespace
{

struct CapturedEvent
{
    int sample_position;
    juce::MidiMessage message;
};

[[nodiscard]] auto capture_events(juce::MidiBuffer const &buffer)
    -> std::vector<CapturedEvent>
{
    auto events = std::vector<CapturedEvent>{};
    for (auto const metadata : buffer)
    {
        events.push_back({
            .sample_position = metadata.samplePosition,
            .message = metadata.getMessage(),
        });
    }
    return events;
}

} // namespace

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

    auto const events = capture_events(xen::render_to_midi(timeline));
    REQUIRE(events.size() == 3);

    CHECK(events[0].sample_position == 12);
    CHECK(events[0].message.isPitchWheel());
    CHECK(events[0].message.getChannel() == 2);
    CHECK(events[0].message.getPitchWheelValue() == 9'000);

    CHECK(events[1].sample_position == 12);
    CHECK(events[1].message.isNoteOn());
    CHECK(events[1].message.getChannel() == 2);
    CHECK(events[1].message.getNoteNumber() == 69);
    CHECK(events[1].message.getFloatVelocity() ==
          Catch::Approx(101.0f / 127.0f));

    CHECK(events[2].sample_position == 34);
    CHECK(events[2].message.isNoteOff());
    CHECK(events[2].message.getChannel() == 2);
    CHECK(events[2].message.getNoteNumber() == 69);
}

TEST_CASE("render_to_midi assigns overlapping notes to different member channels",
          "[midi]")
{
    auto const timeline = std::vector<sequence::midi::TimedMidiNote>{
        {.begin = 10, .end = 30, .note = 60, .velocity = 100, .pitch_bend = 8'100},
        {.begin = 15, .end = 40, .note = 64, .velocity = 110, .pitch_bend = 8'300},
    };

    auto const events = capture_events(xen::render_to_midi(timeline));
    REQUIRE(events.size() == 6);

    CHECK(events[0].sample_position == 10);
    CHECK(events[0].message.isPitchWheel());
    CHECK(events[0].message.getChannel() == 2);

    CHECK(events[1].sample_position == 10);
    CHECK(events[1].message.isNoteOn());
    CHECK(events[1].message.getChannel() == 2);
    CHECK(events[1].message.getNoteNumber() == 60);

    CHECK(events[2].sample_position == 15);
    CHECK(events[2].message.isPitchWheel());
    CHECK(events[2].message.getChannel() == 3);

    CHECK(events[3].sample_position == 15);
    CHECK(events[3].message.isNoteOn());
    CHECK(events[3].message.getChannel() == 3);
    CHECK(events[3].message.getNoteNumber() == 64);

    CHECK(events[4].sample_position == 30);
    CHECK(events[4].message.isNoteOff());
    CHECK(events[4].message.getChannel() == 2);

    CHECK(events[5].sample_position == 40);
    CHECK(events[5].message.isNoteOff());
    CHECK(events[5].message.getChannel() == 3);
}

TEST_CASE("render_to_midi reuses a released channel after note-off at the same sample",
          "[midi]")
{
    auto const timeline = std::vector<sequence::midi::TimedMidiNote>{
        {.begin = 10, .end = 20, .note = 60, .velocity = 100, .pitch_bend = 8'100},
        {.begin = 20, .end = 30, .note = 64, .velocity = 110, .pitch_bend = 8'300},
    };

    auto const events = capture_events(xen::render_to_midi(timeline));
    REQUIRE(events.size() == 6);

    CHECK(events[2].sample_position == 20);
    CHECK(events[2].message.isNoteOff());
    CHECK(events[2].message.getChannel() == 2);
    CHECK(events[2].message.getNoteNumber() == 60);

    CHECK(events[3].sample_position == 20);
    CHECK(events[3].message.isPitchWheel());
    CHECK(events[3].message.getChannel() == 2);
    CHECK(events[3].message.getPitchWheelValue() == 8'300);

    CHECK(events[4].sample_position == 20);
    CHECK(events[4].message.isNoteOn());
    CHECK(events[4].message.getChannel() == 2);
    CHECK(events[4].message.getNoteNumber() == 64);
}

TEST_CASE("render_to_midi drops notes when all 15 member channels are occupied",
          "[midi]")
{
    auto timeline = std::vector<sequence::midi::TimedMidiNote>{};
    timeline.reserve(16);
    for (auto i = 0; i < 16; ++i)
    {
        timeline.push_back({
            .begin = 0,
            .end = 100,
            .note = (std::uint8_t)(60 + i),
            .velocity = 100,
            .pitch_bend = (std::uint16_t)(8'000 + i),
        });
    }

    auto const events = capture_events(xen::render_to_midi(timeline));
    REQUIRE(events.size() == 45);

    auto used_channels = std::array<bool, 17>{};
    auto note_on_count = 0;
    auto dropped_found = false;
    for (auto const &event : events)
    {
        if (event.message.isNoteOn())
        {
            ++note_on_count;
            used_channels[(std::size_t)event.message.getChannel()] = true;
            if (event.message.getNoteNumber() == 75)
            {
                dropped_found = true;
            }
        }
    }

    CHECK(note_on_count == 15);
    CHECK_FALSE(dropped_found);
    for (auto channel = 2; channel <= 16; ++channel)
    {
        CHECK(used_channels[(std::size_t)channel]);
    }
}
