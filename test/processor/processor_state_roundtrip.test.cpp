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
    REQUIRE(source.execute_command_string("set sequence name \"alpha\" 0").first ==
            MessageLevel::Info);

    auto const expected = source.get_engine_snapshot().engine;

    auto blob = juce::MemoryBlock{};
    source.getStateInformation(blob);
    REQUIRE(blob.getSize() > 0);

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
    REQUIRE_NOTHROW(
        processor.setStateInformation(invalid, (int)std::char_traits<char>::length(invalid)));

    auto const after_snapshot = processor.get_engine_snapshot();
    CHECK(after_snapshot.engine == before_snapshot.engine);
    CHECK(after_snapshot.commit_id == before_snapshot.commit_id);
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
    CHECK(after.commit_id > before.commit_id);
    CHECK(target.get_ui_snapshot_version() > before_ui_version);
    CHECK(target.pending_engine_state_update.version() == before_mailbox_version + 1);
}
