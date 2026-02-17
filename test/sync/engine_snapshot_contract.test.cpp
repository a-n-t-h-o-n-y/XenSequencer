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

    auto const before_commit_id = before.commit_id;
    auto const [level, _message] = processor.execute_command_string("set key 11");
    CHECK(level == MessageLevel::Info);

    auto const after = processor.get_engine_snapshot();
    auto const after_version = processor.get_ui_snapshot_version();
    CHECK(after.snapshot_version == after_version);
    CHECK(after.snapshot_version > before.snapshot_version);
    CHECK(after.engine.key == 11);
    CHECK(after.commit_id != before_commit_id);
}

TEST_CASE("Deprecated UI commands do not mutate engine state", "[sync][snapshot]")
{
    auto processor = XenProcessor{};

    auto const before = processor.get_engine_snapshot();

    auto const [focus_level, _focus_message] =
        processor.execute_command_string("focus SequenceView");
    CHECK(focus_level == MessageLevel::Warning);

    auto const [show_level, _show_message] =
        processor.execute_command_string("show LibraryView");
    CHECK(show_level == MessageLevel::Warning);

    auto const [theme_level, _theme_message] =
        processor.execute_command_string("set theme apollo");
    CHECK(theme_level == MessageLevel::Warning);

    auto const after = processor.get_engine_snapshot();
    CHECK(after.engine == before.engine);
    CHECK(after.commit_id == before.commit_id);
    CHECK(after.snapshot_version > before.snapshot_version);
}

TEST_CASE("Mailbox version advances only for committed engine changes",
          "[sync][snapshot]")
{
    auto processor = XenProcessor{};

    auto const mailbox_before = processor.pending_engine_state_update.version();

    auto const [non_mutating_level, _version_message] =
        processor.execute_command_string("version");
    CHECK(non_mutating_level == MessageLevel::Info);
    CHECK(processor.pending_engine_state_update.version() == mailbox_before);

    auto const [mutating_level, _set_key_message] =
        processor.execute_command_string("set key 9");
    CHECK(mutating_level == MessageLevel::Info);
    CHECK(processor.pending_engine_state_update.version() == mailbox_before + 1);
}

TEST_CASE("Empty command strings do not advance snapshot version", "[sync][snapshot]")
{
    auto processor = XenProcessor{};

    auto const before = processor.get_ui_snapshot_version();
    auto const [level, message] = processor.execute_command_string("   ;    ; ");

    CHECK(level == MessageLevel::Debug);
    CHECK(message.empty());
    CHECK(processor.get_ui_snapshot_version() == before);
}
