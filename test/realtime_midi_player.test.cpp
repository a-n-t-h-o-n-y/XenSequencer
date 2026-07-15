#include <cstddef>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <juce_audio_basics/juce_audio_basics.h>

#include <sequence/sequence.hpp>

#include <xen/midi_compiler.hpp>
#include <xen/realtime_midi_player.hpp>

namespace
{

struct Event
{
    int sample{};
    juce::MidiMessage message{};
};

auto events(juce::MidiBuffer const &buffer) -> std::vector<Event>
{
    auto result = std::vector<Event>{};
    for (auto const metadata : buffer)
    {
        result.push_back({metadata.samplePosition, metadata.getMessage()});
    }
    return result;
}

auto make_project(int pitch = 0, float velocity = 0.75F) -> xen::ProjectState
{
    auto project = xen::ProjectState{};
    xen::selected_sequence(project, xen::CompositionCursor{}) = {
        .elements = {sequence::Note{.pitch = pitch, .velocity = velocity}},
        .weight = 1.0F,
    };
    return project;
}

auto make_update(xen::ProjectState const &project, std::uint64_t generation = 1)
    -> xen::CompiledMidiUpdate
{
    auto update = xen::CompiledMidiUpdate{.generation = generation};
    update.schedule =
        xen::MidiCompiler::compile(project, xen::DEFAULT_CHANNEL_ID, generation);
    return update;
}

auto playing(double ppq, int samples = 512) -> xen::TransportBlock
{
    return {
        .playing = true,
        .has_ppq = true,
        .ppq = ppq,
        .bpm = 120.0,
        .sample_rate = 44'100,
        .sample_count = samples,
    };
}

auto prepared_player() -> xen::RealtimeMidiPlayer
{
    auto player = xen::RealtimeMidiPlayer{};
    player.prepare(44'100, 512);
    return player;
}

} // namespace

TEST_CASE("RealtimeMidiPlayer schedules continuous PPQ blocks without retriggering",
          "[midi][player]")
{
    auto player = prepared_player();
    auto const update = make_update(make_project());
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};

    player.process(playing(0.0), buffer);
    auto const started = events(buffer);
    REQUIRE(started.size() == 2);
    CHECK(started[0].message.isPitchWheel());
    CHECK(started[1].message.isNoteOn());

    auto const next_ppq = 512.0 * 120.0 / (60.0 * 44'100.0);
    buffer.clear();
    player.process(playing(next_ppq), buffer);
    CHECK(buffer.isEmpty());
}

TEST_CASE("RealtimeMidiPlayer reconciles starts stops seeks and schedule changes",
          "[midi][player]")
{
    auto player = prepared_player();
    auto original = make_update(make_project(0), 1);
    player.adopt(original);
    auto buffer = juce::MidiBuffer{};

    player.process(playing(1.0), buffer);
    REQUIRE(events(buffer).size() == 2);

    auto changed = make_update(make_project(4), 2);
    player.adopt(changed);
    buffer.clear();
    player.process(playing(1.01), buffer);
    auto const replacement = events(buffer);
    REQUIRE(replacement.size() == 3);
    CHECK(replacement[0].message.isNoteOff());
    CHECK(replacement[1].message.isPitchWheel());
    CHECK(replacement[2].message.isNoteOn());

    buffer.clear();
    auto stopped = playing(1.02);
    stopped.playing = false;
    player.process(stopped, buffer);
    auto const stopped_events = events(buffer);
    REQUIRE(stopped_events.size() == 1);
    CHECK(stopped_events[0].message.isNoteOff());
}

TEST_CASE("RealtimeMidiPlayer updates only pitch bend for a held logical note",
          "[midi][player]")
{
    auto original_project = make_project();
    auto changed_project = original_project;
    changed_project.composition.columns.at(0).pitch.base_frequency = 441.0F;
    auto original = make_update(original_project, 1);
    auto changed = make_update(changed_project, 2);
    REQUIRE(original.schedule.notes[0].note == changed.schedule.notes[0].note);
    REQUIRE(original.schedule.notes[0].pitch_bend !=
            changed.schedule.notes[0].pitch_bend);

    auto player = prepared_player();
    player.adopt(original);
    auto buffer = juce::MidiBuffer{};
    player.process(playing(1.0), buffer);

    player.adopt(changed);
    buffer.clear();
    player.process(playing(1.01), buffer);
    auto const output = events(buffer);
    REQUIRE(output.size() == 1);
    CHECK(output[0].message.isPitchWheel());
}

TEST_CASE("RealtimeMidiPlayer loops with note-off before the next note-on",
          "[midi][player]")
{
    auto player = prepared_player();
    auto const update = make_update(make_project());
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};

    constexpr auto first_samples = 100;
    player.process(playing(3.99, first_samples), buffer);
    buffer.clear();
    auto const next_ppq = 3.99 + first_samples * 120.0 / (60.0 * 44'100.0);
    auto const samples_to_wrap =
        static_cast<int>((4.0 - next_ppq) * 60.0 * 44'100.0 / 120.0) + 4;
    player.process(playing(next_ppq, samples_to_wrap), buffer);
    auto const looped = events(buffer);
    REQUIRE(looped.size() == 3);
    CHECK(looped[0].message.isNoteOff());
    CHECK(looped[1].message.isPitchWheel());
    CHECK(looped[2].message.isNoteOn());
    CHECK(looped[0].sample == looped[1].sample);
}

TEST_CASE("RealtimeMidiPlayer emits sorted CC before velocity-zero notes",
          "[midi][player][midi-cc]")
{
    auto project = make_project(0, 0.f);
    auto &note =
        std::get<sequence::Note>(xen::selected_sequence(project, {}).elements.front());
    note.midi_cc = {{1, 0.f}, {74, 0.5f}, {127, 1.f}};
    auto const update = make_update(project);
    auto player = prepared_player();
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};

    player.process(playing(0.0), buffer);
    auto const output = events(buffer);
    REQUIRE(output.size() == 5);
    CHECK(output[0].message.isPitchWheel());
    for (auto index = std::size_t{1}; index < 4; ++index)
    {
        CHECK(output[index].message.isController());
        CHECK(output[index].message.getChannel() == output[0].message.getChannel());
    }
    CHECK(output[1].message.getControllerNumber() == 1);
    CHECK(output[1].message.getControllerValue() == 0);
    CHECK(output[2].message.getControllerNumber() == 74);
    CHECK(output[2].message.getControllerValue() == 64);
    CHECK(output[3].message.getControllerNumber() == 127);
    CHECK(output[3].message.getControllerValue() == 127);
    CHECK((output[4].message.getRawData()[0] & 0xf0U) == 0x90U);
    CHECK(output[4].message.getVelocity() == 0);
    CHECK(output[4].message.getChannel() == output[0].message.getChannel());
}

TEST_CASE("RealtimeMidiPlayer re-emits controllers when adopting a held note",
          "[midi][player][midi-cc]")
{
    auto project = make_project();
    auto &note =
        std::get<sequence::Note>(xen::selected_sequence(project, {}).elements.front());
    note.midi_cc = {{74, 0.25f}};
    auto original = make_update(project, 1);
    auto player = prepared_player();
    player.adopt(original);
    auto buffer = juce::MidiBuffer{};
    player.process(playing(1.0), buffer);

    note.midi_cc.at(74) = 0.75f;
    auto changed = make_update(project, 2);
    player.adopt(changed);
    buffer.clear();
    player.process(playing(1.01), buffer);
    auto const output = events(buffer);
    REQUIRE(output.size() == 1);
    CHECK(output.front().message.isController());
    CHECK(output.front().message.getControllerNumber() == 74);
    CHECK(output.front().message.getControllerValue() == 95);
}

TEST_CASE("RealtimeMidiPlayer steals the oldest MPE voice deterministically",
          "[midi][player][mpe]")
{
    auto project = xen::ProjectState{};
    auto &cell = xen::selected_sequence(project, xen::CompositionCursor{});
    cell.elements.clear();
    for (auto pitch = 0; pitch < 16; ++pitch)
    {
        cell.elements.push_back(sequence::Note{.pitch = pitch});
    }
    auto update = make_update(project);
    auto player = prepared_player();
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};

    player.process(playing(0.0), buffer);
    auto note_ons = std::size_t{};
    auto note_offs = std::size_t{};
    for (auto const &event : events(buffer))
    {
        note_ons += event.message.isNoteOn() ? 1U : 0U;
        note_offs += event.message.isNoteOff() ? 1U : 0U;
    }
    CHECK(note_ons == 16);
    CHECK(note_offs == 1);
}

TEST_CASE("RealtimeMidiPlayer preserves controls and suppresses input notes",
          "[midi][player][input]")
{
    auto player = prepared_player();
    auto buffer = juce::MidiBuffer{};
    buffer.addEvent(juce::MidiMessage::controllerEvent(1, 7, 100), 3);
    buffer.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 4);
    auto transport = playing(0.0);
    transport.playing = false;

    player.process(transport, buffer);
    auto const output = events(buffer);
    REQUIRE(output.size() == 1);
    CHECK(output[0].message.isController());
    CHECK(output[0].sample == 3);
}

TEST_CASE("RealtimeMidiPlayer fails closed without host PPQ", "[midi][player]")
{
    auto player = prepared_player();
    auto const update = make_update(make_project());
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};
    player.process(playing(1.0), buffer);
    buffer.clear();
    auto transport = playing(1.01);
    transport.has_ppq = false;

    player.process(transport, buffer);
    auto const output = events(buffer);
    REQUIRE(output.size() == 1);
    CHECK(output[0].message.isNoteOff());
    CHECK(player.status().current_fault == xen::RealtimeMidiFault::MissingPpq);
    CHECK(player.status().fault_count == 1);

    buffer.clear();
    player.process(transport, buffer);
    CHECK(player.status().fault_count == 1);
}

TEST_CASE("RealtimeMidiPlayer wraps negative host PPQ", "[midi][player]")
{
    auto player = prepared_player();
    auto const update = make_update(make_project());
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};

    player.process(playing(-1.0), buffer);

    CHECK(player.status().current_fault == xen::RealtimeMidiFault::None);
    CHECK(player.loop_phase() > 0.749);
    CHECK(player.loop_phase() < 0.751);
}

TEST_CASE("RealtimeMidiPlayer fails closed when the host exceeds the prepared block",
          "[midi][player]")
{
    auto player = prepared_player();
    auto const update = make_update(make_project());
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};
    player.process(playing(0.0), buffer);

    buffer.clear();
    player.process(playing(0.01, 513), buffer);

    auto const output = events(buffer);
    REQUIRE(output.size() == 1);
    CHECK(output[0].message.isNoteOff());
    CHECK(player.status().current_fault == xen::RealtimeMidiFault::BlockTooLarge);
}

TEST_CASE("RealtimeMidiPlayer silences active notes after compilation failure",
          "[midi][player]")
{
    auto player = prepared_player();
    auto const update = make_update(make_project());
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};
    player.process(playing(0.0), buffer);

    auto const failed = xen::CompiledMidiUpdate{
        .generation = 2,
        .error = xen::MidiCompilationError::InvalidProject,
    };
    player.adopt(failed);
    buffer.clear();
    player.process(playing(0.01), buffer);

    auto const output = events(buffer);
    REQUIRE(output.size() == 1);
    CHECK(output[0].message.isNoteOff());
    CHECK(player.status().current_fault == xen::RealtimeMidiFault::CompilationFailed);
}

TEST_CASE("RealtimeMidiPlayer bounds retained MIDI storage", "[midi][player]")
{
    auto player = prepared_player();
    auto payload = std::vector<std::uint8_t>(60 * 1024, 0x01U);
    auto const message = juce::MidiMessage::createSysExMessage(
        payload.data(), static_cast<int>(payload.size()));
    auto buffer = juce::MidiBuffer{};
    for (auto index = 0; index < 5; ++index)
    {
        buffer.addEvent(message, index);
    }
    REQUIRE(buffer.getNumEvents() == 5);
    auto transport = playing(0.0);
    transport.playing = false;

    player.process(transport, buffer);

    CHECK(buffer.isEmpty());
    CHECK(player.status().current_fault ==
          xen::RealtimeMidiFault::MidiByteCapacityExceeded);
}

TEST_CASE("RealtimeMidiPlayer bounds pathological event density", "[midi][player]")
{
    auto project = xen::ProjectState{};
    auto &cell = xen::selected_sequence(project, xen::CompositionCursor{});
    cell.elements.clear();
    for (auto pitch = 0; pitch < 2'100; ++pitch)
    {
        cell.elements.push_back(sequence::Note{.pitch = pitch});
    }
    auto update = make_update(project);
    auto player = prepared_player();
    player.adopt(update);
    auto buffer = juce::MidiBuffer{};

    player.process(playing(0.0), buffer);
    CHECK(player.status().current_fault ==
          xen::RealtimeMidiFault::EventCapacityExceeded);
    CHECK(buffer.isEmpty());
}
