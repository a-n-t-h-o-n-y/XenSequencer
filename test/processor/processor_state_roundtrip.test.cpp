#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include <juce_core/juce_core.h>

#include <xen/message_level.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

TEST_CASE("Processor state round-trip preserves engine state", "[processor][state]")
{
    auto source = XenProcessor{};
    REQUIRE(source.execute_command_string("set key 31").first == MessageLevel::Info);
    REQUIRE(source.execute_command_string("set baseFrequency 333.3").first ==
            MessageLevel::Info);
    REQUIRE(source.execute_command_string("set measure timeSignature 7/8").first ==
            MessageLevel::Info);

    auto const expected = source.get_engine_snapshot().engine;

    auto blob = juce::MemoryBlock{};
    source.getStateInformation(blob);
    REQUIRE(blob.getSize() > 0);
    auto const serialized =
        std::string{static_cast<char const *>(blob.getData()), blob.getSize()};
    CHECK(serialized.find("history_entry_id") == std::string::npos);
    CHECK(serialized.find("project_revision") == std::string::npos);

    auto target = XenProcessor{};
    REQUIRE_NOTHROW(target.setStateInformation(blob.getData(), (int)blob.getSize()));

    auto const actual = target.get_engine_snapshot().engine;
    CHECK(actual == expected);
}

TEST_CASE("Processor setStateInformation ignores invalid payload safely",
          "[processor][state]")
{
    auto processor = XenProcessor{};
    auto const before_snapshot = processor.get_engine_snapshot();
    auto const before_ui_version = processor.get_ui_snapshot_version();
    auto const before_mailbox_version = processor.pending_engine_state_update.version();

    auto const invalid = "not json";
    REQUIRE_NOTHROW(processor.setStateInformation(
        invalid, (int)std::char_traits<char>::length(invalid)));

    auto const after_snapshot = processor.get_engine_snapshot();
    CHECK(after_snapshot.engine == before_snapshot.engine);
    CHECK(after_snapshot.history_entry_id == before_snapshot.history_entry_id);
    CHECK(after_snapshot.project_revision == before_snapshot.project_revision);
    CHECK(processor.get_ui_snapshot_version() == before_ui_version);
    CHECK(processor.pending_engine_state_update.version() == before_mailbox_version);
}

TEST_CASE("Processor setStateInformation publishes and advances snapshot on success",
          "[processor][state]")
{
    auto source = XenProcessor{};
    REQUIRE(source.execute_command_string("set key 5").first == MessageLevel::Info);

    auto blob = juce::MemoryBlock{};
    source.getStateInformation(blob);
    REQUIRE(blob.getSize() > 0);

    auto target = XenProcessor{};
    auto const before = target.get_engine_snapshot();
    auto const before_ui_version = target.get_ui_snapshot_version();
    auto const before_mailbox_version = target.pending_engine_state_update.version();

    REQUIRE_NOTHROW(target.setStateInformation(blob.getData(), (int)blob.getSize()));

    auto const after = target.get_engine_snapshot();
    CHECK(after.history_entry_id != before.history_entry_id);
    CHECK(after.project_revision != before.project_revision);
    CHECK(target.get_ui_snapshot_version() > before_ui_version);
    CHECK(target.pending_engine_state_update.version() == before_mailbox_version + 1);
}

TEST_CASE("Processor equal-data restoration replaces project history",
          "[processor][state]")
{
    auto processor = XenProcessor{};
    auto blob = juce::MemoryBlock{};
    processor.getStateInformation(blob);
    REQUIRE(blob.getSize() > 0);

    auto const before = processor.get_engine_snapshot();
    REQUIRE_NOTHROW(processor.setStateInformation(blob.getData(), (int)blob.getSize()));

    auto const after = processor.get_engine_snapshot();
    CHECK(after.engine == before.engine);
    CHECK(after.history_entry_id != before.history_entry_id);
    CHECK(after.project_revision != before.project_revision);
    CHECK(processor.execute_command_string("undo").second == "Nothing to undo.");
}
