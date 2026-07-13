#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sequence/sequence.hpp>

#include <xen/composition.hpp>
#include <xen/midi_compiler.hpp>
#include <xen/state.hpp>

namespace
{

auto make_note_project(int pitch = 0) -> xen::ProjectState
{
    auto project = xen::ProjectState{};
    xen::selected_sequence(project, xen::CompositionCursor{}) = {
        .elements = {sequence::Note{.pitch = pitch, .velocity = 0.75F}},
        .weight = 1.0F,
    };
    return project;
}

} // namespace

TEST_CASE("MidiCompiler emits beat-domain notes independent of tempo",
          "[midi][compiler]")
{
    auto const schedule =
        xen::MidiCompiler::compile(make_note_project(), xen::DEFAULT_CHANNEL_ID, 7);

    CHECK(schedule.generation == 7);
    CHECK(schedule.loop_beats == Catch::Approx(4.0));
    REQUIRE(schedule.notes.size() == 1);
    CHECK(schedule.notes[0].begin_beat == Catch::Approx(0.0));
    CHECK(schedule.notes[0].end_beat == Catch::Approx(4.0));
    REQUIRE(schedule.boundaries.size() == 2);
    CHECK(schedule.boundaries[0].beat == 0.0);
    CHECK(schedule.boundaries[0].kind == xen::MidiBoundaryKind::End);
    CHECK(schedule.boundaries[1].kind == xen::MidiBoundaryKind::Start);
}

TEST_CASE("MidiCompiler flattens weighted nested cells in musical time",
          "[midi][compiler]")
{
    auto project = xen::ProjectState{};
    xen::selected_sequence(project, xen::CompositionCursor{}) = {
        .elements = {sequence::Sequence{{
            {.elements = {sequence::Note{.pitch = 0}}, .weight = 1.0F},
            {.elements = {sequence::Note{.pitch = 1}}, .weight = 3.0F},
        }}},
        .weight = 1.0F,
    };

    auto const schedule =
        xen::MidiCompiler::compile(project, xen::DEFAULT_CHANNEL_ID, 1);
    REQUIRE(schedule.notes.size() == 2);
    CHECK(schedule.notes[0].begin_beat == Catch::Approx(0.0));
    CHECK(schedule.notes[0].end_beat == Catch::Approx(1.0));
    CHECK(schedule.notes[1].begin_beat == Catch::Approx(1.0));
    CHECK(schedule.notes[1].end_beat == Catch::Approx(4.0));
}

TEST_CASE("MidiCompiler structural identities survive pitch edits", "[midi][compiler]")
{
    auto original = make_note_project(0);
    auto changed = make_note_project(4);
    auto const first = xen::MidiCompiler::compile(original, xen::DEFAULT_CHANNEL_ID, 1);
    auto const second = xen::MidiCompiler::compile(changed, xen::DEFAULT_CHANNEL_ID, 2);

    REQUIRE(first.notes.size() == 1);
    REQUIRE(second.notes.size() == 1);
    CHECK(first.notes[0].key == second.notes[0].key);
    CHECK(first.notes[0].note != second.notes[0].note);
}

TEST_CASE("MidiCompiler filters rows by bound output channel",
          "[midi][compiler][composition]")
{
    auto project = make_note_project();
    (void)xen::ensure_composition_row(project.composition, 1, "peer");
    auto peer =
        sequence::Cell{.elements = {sequence::Note{.pitch = 5}}, .weight = 1.0F};
    auto const peer_id = xen::create_sequence(project.sequence_bank, std::move(peer));
    xen::assign_sequence_reference(project.composition, 1, 0, peer_id);

    auto const main = xen::MidiCompiler::compile(project, xen::DEFAULT_CHANNEL_ID, 1);
    auto const peer_schedule = xen::MidiCompiler::compile(project, "peer", 2);
    REQUIRE(main.notes.size() == 1);
    REQUIRE(peer_schedule.notes.size() == 1);
    CHECK(main.notes[0].note != peer_schedule.notes[0].note);
}

TEST_CASE("MidiCompiler includes sparse implicit columns in loop timing",
          "[midi][compiler][composition]")
{
    auto project = make_note_project();
    project.composition.default_column.duration = {1, 4};
    xen::set_column_duration(project.composition, 0, {1, 4});
    xen::assign_sequence_reference(project.composition, 0, -2,
                                   xen::DEFAULT_SEQUENCE_ID);
    xen::set_loop_start(project.composition, -2);
    xen::set_loop_end(project.composition, 0);

    auto const schedule =
        xen::MidiCompiler::compile(project, xen::DEFAULT_CHANNEL_ID, 1);
    CHECK(schedule.loop_beats == Catch::Approx(3.0));
    REQUIRE(schedule.notes.size() == 2);
    CHECK(schedule.notes[0].begin_beat == Catch::Approx(0.0));
    CHECK(schedule.notes[1].begin_beat == Catch::Approx(2.0));
}

TEST_CASE("MidiCompiler suppresses zero-duration notes", "[midi][compiler]")
{
    auto project = xen::ProjectState{};
    xen::selected_sequence(project, xen::CompositionCursor{}) = {
        .elements = {sequence::Note{.gate = 0.0F}}, .weight = 1.0F};

    auto const schedule =
        xen::MidiCompiler::compile(project, xen::DEFAULT_CHANNEL_ID, 1);
    CHECK(schedule.notes.empty());
    CHECK(schedule.boundaries.empty());
}
