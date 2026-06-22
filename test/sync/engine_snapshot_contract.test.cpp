#include <cstdint>
#include <string>

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

    auto const before_entry_id = before.history_entry_id;
    auto const before_revision = before.project_revision;
    auto const [level, _message] = processor.execute_command_string(
        "set key 11", {.expected_project_revision =
                           processor.get_engine_snapshot().project_revision});
    CHECK(level == MessageLevel::Info);

    auto const after = processor.get_engine_snapshot();
    auto const after_version = processor.get_ui_snapshot_version();
    CHECK(after.snapshot_version == after_version);
    CHECK(after.snapshot_version > before.snapshot_version);
    CHECK(after.engine.key == 11);
    CHECK(after.history_entry_id != before_entry_id);
    CHECK(after.project_revision != before_revision);
}

TEST_CASE("Unknown commands do not mutate engine state", "[sync][snapshot]")
{
    auto processor = XenProcessor{};

    auto const before = processor.get_engine_snapshot();

    auto const [missing_level, _missing_message] =
        processor.execute_command_string("notACommand", CommandContext{});
    CHECK(missing_level == MessageLevel::Error);

    auto const [invalid_level, _invalid_message] =
        processor.execute_command_string("notACommand 123", CommandContext{});
    CHECK(invalid_level == MessageLevel::Error);

    auto const after = processor.get_engine_snapshot();
    CHECK(after.engine == before.engine);
    CHECK(after.history_entry_id == before.history_entry_id);
    CHECK(after.project_revision == before.project_revision);
    CHECK(after.snapshot_version == before.snapshot_version);
}

TEST_CASE("Mailbox version advances only for committed engine changes",
          "[sync][snapshot]")
{
    auto processor = XenProcessor{};

    auto const mailbox_before = processor.pending_engine_state_update.version();

    auto const [non_mutating_level, _version_message] =
        processor.execute_command_string("version", CommandContext{});
    CHECK(non_mutating_level == MessageLevel::Info);
    CHECK(processor.pending_engine_state_update.version() == mailbox_before);

    auto const [mutating_level, _set_key_message] = processor.execute_command_string(
        "set key 9", {.expected_project_revision =
                          processor.get_engine_snapshot().project_revision});
    CHECK(mutating_level == MessageLevel::Info);
    CHECK(processor.pending_engine_state_update.version() == mailbox_before + 1);
}

TEST_CASE("Empty command strings do not advance snapshot version", "[sync][snapshot]")
{
    auto processor = XenProcessor{};

    auto const before = processor.get_ui_snapshot_version();
    auto const [level, message] =
        processor.execute_command_string("   ;    ; ", CommandContext{});

    CHECK(level == MessageLevel::Debug);
    CHECK(message.empty());
    CHECK(processor.get_ui_snapshot_version() == before);
}
