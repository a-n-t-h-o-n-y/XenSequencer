#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include <xen/message_level.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

TEST_CASE("Engine snapshot version mirrors UI snapshot version", "[sync][snapshot]")
{
    auto processor = XenProcessor{};

    auto const before = processor.get_engine_snapshot();
    auto const before_version = processor.get_ui_snapshot_version();
    CHECK(before.snapshot_version == before_version);

    auto const result = processor.execute_command_string(
        "set key 11",
        {.expected_project_revision =
             processor.get_engine_snapshot().project_revision});
    CHECK(result.status.first == MessageLevel::Info);

    auto const after = processor.get_engine_snapshot();
    CHECK(after.snapshot_version == processor.get_ui_snapshot_version());
    CHECK(after.snapshot_version > before.snapshot_version);
    CHECK(after.history_entry_id != before.history_entry_id);
    CHECK(after.project_revision != before.project_revision);
}

TEST_CASE("Unknown commands do not mutate engine state", "[sync][snapshot]")
{
    auto processor = XenProcessor{};
    auto const before = processor.get_engine_snapshot();

    auto const missing =
        processor.execute_command_string("notACommand", CommandContext{});
    CHECK(missing.status.first == MessageLevel::Error);

    auto const after = processor.get_engine_snapshot();
    CHECK(after.engine == before.engine);
    CHECK(after.history_entry_id == before.history_entry_id);
    CHECK(after.project_revision == before.project_revision);
    CHECK(after.snapshot_version == before.snapshot_version);
}
