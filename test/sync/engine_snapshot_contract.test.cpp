#include <chrono>
#include <cstdint>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include <xen/message_level.hpp>
#include <xen/midi_compilation_service.hpp>
#include <xen/sequencer_session.hpp>

using namespace xen;

TEST_CASE("Project snapshots advance project identity", "[sync][snapshot]")
{
    auto session = SequencerSession{};

    auto const before = session.project_snapshot();

    auto const result = session.execute_command_string(
        "set key 11",
        {.expected_project_revision = session.project_snapshot().project_revision});
    CHECK(result.status.first == MessageLevel::Info);

    auto const after = session.project_snapshot();
    CHECK(after.history_entry_id != before.history_entry_id);
    CHECK(after.project_revision != before.project_revision);
}

TEST_CASE("Unknown commands do not mutate engine state", "[sync][snapshot]")
{
    auto session = SequencerSession{};
    auto const before = session.project_snapshot();

    auto const missing =
        session.execute_command_string("notACommand", CommandContext{});
    CHECK(missing.status.first == MessageLevel::Error);

    auto const after = session.project_snapshot();
    CHECK(after.project == before.project);
    CHECK(after.history_entry_id == before.history_entry_id);
    CHECK(after.project_revision == before.project_revision);
}

TEST_CASE("Instance output binding updates session state", "[sync][snapshot]")
{
    auto session = SequencerSession{};

    session.set_channel_id("peer");

    CHECK(session.instance_binding().channel_id == "peer");
}

TEST_CASE("MIDI compilation service publishes only the latest requested generation",
          "[sync][midi-compilation]")
{
    auto service = MidiCompilationService{};
    auto project = ProjectState{};
    auto latest = std::uint64_t{};
    for (auto pitch = 0; pitch < 20; ++pitch)
    {
        selected_sequence(project, CompositionCursor{}).elements = {
            sequence::Note{.pitch = pitch}};
        latest = service.submit(project, DEFAULT_CHANNEL_ID);
    }

    auto view = std::optional<CompiledMidiMailbox::ReadView>{};
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (auto candidate = service.try_consume_latest();
            candidate.has_value() && candidate->generation() == latest)
        {
            view = candidate;
            break;
        }
        std::this_thread::yield();
    }
    REQUIRE(view.has_value());
    CHECK(view->update().ready());
    CHECK(view->update().schedule.generation == latest);
    REQUIRE(view->update().schedule.notes.size() == 1);
}

TEST_CASE("MIDI compilation service starts and stops while idle",
          "[sync][midi-compilation]")
{
    for (auto iteration = 0; iteration < 100; ++iteration)
    {
        auto service = MidiCompilationService{};
    }
}

TEST_CASE("MIDI compilation service publishes explicit failures",
          "[sync][midi-compilation]")
{
    auto service = MidiCompilationService{};
    auto invalid = ProjectState{};
    invalid.composition.default_column.duration.denominator = 0;
    auto const generation = service.submit(std::move(invalid), DEFAULT_CHANNEL_ID);

    auto view = std::optional<CompiledMidiMailbox::ReadView>{};
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (auto candidate = service.try_consume_latest();
            candidate.has_value() && candidate->generation() == generation)
        {
            view = candidate;
            break;
        }
        std::this_thread::yield();
    }
    REQUIRE(view.has_value());
    CHECK_FALSE(view->update().ready());
    CHECK(view->update().error == MidiCompilationError::InvalidProject);
}
