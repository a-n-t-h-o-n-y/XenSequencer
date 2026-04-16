#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/sequence.hpp>

#include <xen/midi.hpp>
#include <xen/midi_engine.hpp>
#include <xen/state.hpp>

namespace
{

[[nodiscard]] auto first_timed_note(xen::EngineState const &engine,
                                    xen::DAWState const &daw)
    -> sequence::midi::TimedMidiNote
{
    auto const timeline = xen::state_to_timeline(
        engine.measure, engine.tuning, engine.base_frequency, daw, engine.scale,
        engine.key, engine.scale_translate_direction);
    if (timeline.empty())
    {
        return {};
    }
    return timeline.front();
}

[[nodiscard]] auto make_tuning_with_offset(float cents) -> sequence::Tuning
{
    return {
        .intervals = {cents, 100.f + cents, 200.f + cents, 300.f + cents,
                      400.f + cents, 500.f + cents, 600.f + cents, 700.f + cents,
                      800.f + cents, 900.f + cents, 1000.f + cents, 1100.f + cents},
        .octave = 1200.f,
        .description = "offset",
    };
}

[[nodiscard]] auto make_tuning_shifted_engine(int pitch, float cents,
                                              float velocity = 0.75f)
    -> xen::EngineState
{
    auto engine = xen::EngineState{};
    engine.measure.cell = {
        .elements = {sequence::Note{.pitch = pitch, .velocity = velocity}},
        .weight = 1.f,
    };
    engine.tuning = make_tuning_with_offset(cents);
    return engine;
}

[[nodiscard]] auto make_two_note_tuning_engine(float cents) -> xen::EngineState
{
    auto engine = xen::EngineState{};
    engine.measure.cell = {
        .elements = {
            sequence::Note{.pitch = 0, .velocity = 0.75f},
            sequence::Note{.pitch = 1, .velocity = 0.75f},
        },
        .weight = 1.f,
    };
    engine.tuning = make_tuning_with_offset(cents);
    return engine;
}

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

[[nodiscard]] auto playing_daw_state() -> xen::DAWState
{
    return {
        .bpm = 120.f,
        .sample_rate = 44'100,
        .is_playing = true,
    };
}

[[nodiscard]] auto stopped_daw_state() -> xen::DAWState
{
    auto daw = playing_daw_state();
    daw.is_playing = false;
    return daw;
}

[[nodiscard]] auto make_sustained_note_engine(int pitch) -> xen::EngineState
{
    auto engine = xen::EngineState{};
    engine.measure.cell = {
        .elements = {sequence::Note{.pitch = pitch, .velocity = 0.75f}},
        .weight = 1.f,
    };
    return engine;
}

[[nodiscard]] auto make_empty_engine() -> xen::EngineState
{
    auto engine = xen::EngineState{};
    engine.measure.cell = {
        .elements = {},
        .weight = 1.f,
    };
    return engine;
}

[[nodiscard]] auto make_second_half_note_engine(int pitch) -> xen::EngineState
{
    auto engine = xen::EngineState{};
    engine.measure.cell = {
        .elements = {sequence::Sequence{{
            {.elements = {}, .weight = 1.f},
            {.elements = {sequence::Note{.pitch = pitch, .velocity = 0.75f}},
             .weight = 1.f},
        }}},
        .weight = 1.f,
    };
    return engine;
}

[[nodiscard]] auto first_note_number(xen::EngineState const &engine,
                                     xen::DAWState const &daw) -> int
{
    auto const timeline = xen::state_to_timeline(
        engine.measure, engine.tuning, engine.base_frequency, daw, engine.scale,
        engine.key, engine.scale_translate_direction);
    if (timeline.size() != 1)
    {
        return -1;
    }
    return timeline.front().note;
}

} // namespace

TEST_CASE("MidiEngine starts in-flight notes immediately when playback begins mid-note",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const sequencer = make_sustained_note_engine(0);
    auto const daw = playing_daw_state();

    engine.update(sequencer, daw);

    auto const events = capture_events(engine.step({}, 100, 10, daw));
    REQUIRE(events.size() == 2);

    CHECK(events[0].sample_position == 0);
    CHECK(events[0].message.isPitchWheel());
    CHECK(events[0].message.getChannel() == 2);

    CHECK(events[1].sample_position == 0);
    CHECK(events[1].message.isNoteOn());
    CHECK(events[1].message.getChannel() == 2);
    CHECK(events[1].message.getNoteNumber() == first_note_number(sequencer, daw));
}

TEST_CASE("MidiEngine does not duplicate a note-on across continuous mid-note blocks",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const sequencer = make_sustained_note_engine(0);
    auto const daw = playing_daw_state();

    engine.update(sequencer, daw);
    auto const first_events = capture_events(engine.step({}, 100, 10, daw));
    REQUIRE(first_events.size() == 2);

    auto const second_events = capture_events(engine.step({}, 110, 10, daw));
    CHECK(second_events.empty());
}

TEST_CASE("MidiEngine reconciles a changed currently sounding note immediately",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const daw = playing_daw_state();
    auto const original = make_sustained_note_engine(0);
    auto const changed = make_sustained_note_engine(4);

    engine.update(original, daw);
    (void)engine.step({}, 100, 10, daw);

    engine.update(changed, daw);
    auto const events = capture_events(engine.step({}, 110, 10, daw));
    REQUIRE(events.size() == 3);

    CHECK(events[0].sample_position == 0);
    CHECK(events[0].message.isNoteOff());
    CHECK(events[0].message.getChannel() == 2);
    CHECK(events[0].message.getNoteNumber() == first_note_number(original, daw));

    CHECK(events[1].sample_position == 0);
    CHECK(events[1].message.isPitchWheel());
    CHECK(events[1].message.getChannel() == 2);

    CHECK(events[2].sample_position == 0);
    CHECK(events[2].message.isNoteOn());
    CHECK(events[2].message.getChannel() == 2);
    CHECK(events[2].message.getNoteNumber() == first_note_number(changed, daw));
}

TEST_CASE("MidiEngine turns off a deleted currently sounding note immediately",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const daw = playing_daw_state();
    auto const original = make_sustained_note_engine(0);

    engine.update(original, daw);
    (void)engine.step({}, 100, 10, daw);

    engine.update(make_empty_engine(), daw);
    auto const events = capture_events(engine.step({}, 110, 10, daw));
    REQUIRE(events.size() == 1);

    CHECK(events[0].sample_position == 0);
    CHECK(events[0].message.isNoteOff());
    CHECK(events[0].message.getChannel() == 2);
    CHECK(events[0].message.getNoteNumber() == first_note_number(original, daw));
}

TEST_CASE("MidiEngine turns on an added note immediately when its start is in the past",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const daw = playing_daw_state();
    auto const added = make_sustained_note_engine(0);

    engine.update(make_empty_engine(), daw);
    auto const before_events = capture_events(engine.step({}, 100, 10, daw));
    CHECK(before_events.empty());

    engine.update(added, daw);
    auto const events = capture_events(engine.step({}, 110, 10, daw));
    REQUIRE(events.size() == 2);

    CHECK(events[0].sample_position == 0);
    CHECK(events[0].message.isPitchWheel());
    CHECK(events[0].message.getChannel() == 2);

    CHECK(events[1].sample_position == 0);
    CHECK(events[1].message.isNoteOn());
    CHECK(events[1].message.getChannel() == 2);
    CHECK(events[1].message.getNoteNumber() == first_note_number(added, daw));
}

TEST_CASE("MidiEngine does not retrigger when only future notes change",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const daw = playing_daw_state();

    engine.update(make_second_half_note_engine(0), daw);
    auto const before_events = capture_events(engine.step({}, 100, 10, daw));
    CHECK(before_events.empty());

    engine.update(make_second_half_note_engine(4), daw);
    auto const after_events = capture_events(engine.step({}, 110, 10, daw));
    CHECK(after_events.empty());
}

TEST_CASE("MidiEngine turns off active notes when transport stops and only does it once",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const playing = playing_daw_state();
    auto const stopped = stopped_daw_state();
    auto const sequencer = make_sustained_note_engine(0);

    engine.update(sequencer, playing);
    (void)engine.step({}, 100, 10, playing);

    auto const stop_events = capture_events(engine.step({}, 110, 10, stopped));
    REQUIRE(stop_events.size() == 1);
    CHECK(stop_events[0].sample_position == 0);
    CHECK(stop_events[0].message.isNoteOff());
    CHECK(stop_events[0].message.getChannel() == 2);
    CHECK(stop_events[0].message.getNoteNumber() == first_note_number(sequencer, playing));

    auto const stopped_again = capture_events(engine.step({}, 120, 10, stopped));
    CHECK(stopped_again.empty());
}

TEST_CASE("MidiEngine reconciles active notes correctly after a transport seek",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const daw = playing_daw_state();

    engine.update(make_second_half_note_engine(0), daw);
    auto const started = capture_events(engine.step({}, 45'000, 10, daw));
    REQUIRE(started.size() == 2);
    CHECK(started[1].message.isNoteOn());

    auto const sought = capture_events(engine.step({}, 100, 10, daw));
    REQUIRE(sought.size() == 1);
    CHECK(sought[0].sample_position == 0);
    CHECK(sought[0].message.isNoteOff());
    CHECK(sought[0].message.getChannel() == 2);
}

TEST_CASE("MidiEngine emits only pitch bend when tuning changes bend on a held note",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const daw = playing_daw_state();
    auto const original = make_tuning_shifted_engine(0, 0.f);
    auto const changed = make_tuning_shifted_engine(0, 10.f);

    auto const original_note = first_timed_note(original, daw);
    auto const changed_note = first_timed_note(changed, daw);
    REQUIRE(original_note.note == changed_note.note);
    REQUIRE(original_note.pitch_bend != changed_note.pitch_bend);

    engine.update(original, daw);
    (void)engine.step({}, 100, 10, daw);

    engine.update(changed, daw);
    auto const events = capture_events(engine.step({}, 110, 10, daw));
    REQUIRE(events.size() == 1);
    CHECK(events[0].sample_position == 0);
    CHECK(events[0].message.isPitchWheel());
    CHECK(events[0].message.getChannel() == 2);
    CHECK(events[0].message.getPitchWheelValue() == changed_note.pitch_bend);
}

TEST_CASE("MidiEngine retriggers when held note velocity changes",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const daw = playing_daw_state();
    auto const original = make_tuning_shifted_engine(0, 0.f, 0.5f);
    auto const changed = make_tuning_shifted_engine(0, 0.f, 0.8f);

    auto const original_note = first_timed_note(original, daw);
    auto const changed_note = first_timed_note(changed, daw);
    REQUIRE(original_note.note == changed_note.note);
    REQUIRE(original_note.velocity != changed_note.velocity);

    engine.update(original, daw);
    (void)engine.step({}, 100, 10, daw);

    engine.update(changed, daw);
    auto const events = capture_events(engine.step({}, 110, 10, daw));
    REQUIRE(events.size() == 3);
    CHECK(events[0].message.isNoteOff());
    CHECK(events[0].message.getChannel() == 2);
    CHECK(events[1].message.isPitchWheel());
    CHECK(events[1].message.getChannel() == 2);
    CHECK(events[2].message.isNoteOn());
    CHECK(events[2].message.getChannel() == 2);
}

TEST_CASE("MidiEngine preserves live channels across tuning bend reshuffles",
          "[midi][midi-engine]")
{
    auto engine = xen::MidiEngine{};
    auto const daw = playing_daw_state();
    auto const original = make_two_note_tuning_engine(0.f);
    auto const changed = make_two_note_tuning_engine(10.f);

    engine.update(original, daw);
    auto const started = capture_events(engine.step({}, 100, 10, daw));
    REQUIRE(started.size() == 4);
    CHECK(started[0].message.isPitchWheel());
    CHECK(started[0].message.getChannel() == 2);
    CHECK(started[1].message.isNoteOn());
    CHECK(started[1].message.getChannel() == 2);
    CHECK(started[2].message.isPitchWheel());
    CHECK(started[2].message.getChannel() == 3);
    CHECK(started[3].message.isNoteOn());
    CHECK(started[3].message.getChannel() == 3);

    engine.update(changed, daw);
    auto const events = capture_events(engine.step({}, 110, 10, daw));
    REQUIRE(events.size() == 2);
    CHECK(events[0].message.isPitchWheel());
    CHECK(events[0].message.getChannel() == 2);
    CHECK(events[1].message.isPitchWheel());
    CHECK(events[1].message.getChannel() == 3);
}
