#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include <xen/message_level.hpp>
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
