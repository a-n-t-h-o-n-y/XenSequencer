#include <cstdint>
#include <cstdlib>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <juce_core/juce_core.h>

#include <xen/message_level.hpp>
#include <xen/selection.hpp>
#include <xen/serialize.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

struct DisableActiveSessionDiscovery
{
    DisableActiveSessionDiscovery()
    {
        setenv("XEN_SEQUENCER_DISABLE_ACTIVE_SESSION_DISCOVERY", "1", 1);
    }
};

auto const disable_active_session_discovery = DisableActiveSessionDiscovery{};

} // namespace

TEST_CASE("Processor state round-trip preserves engine state", "[processor][state]")
{
    auto source = XenProcessor{};
    REQUIRE(source.session()
                .execute_command_string(
                    "set key 31",
                    {.expected_project_revision =
                         source.session().project_snapshot().project_revision})
                .status.first == MessageLevel::Info);
    REQUIRE(source.session()
                .execute_command_string(
                    "set baseFrequency 333.3",
                    {.expected_project_revision =
                         source.session().project_snapshot().project_revision})
                .status.first == MessageLevel::Info);
    REQUIRE(source.session()
                .execute_command_string(
                    "set duration 7/8",
                    {.expected_project_revision =
                         source.session().project_snapshot().project_revision})
                .status.first == MessageLevel::Info);
    REQUIRE(
        source.session()
            .execute_command_string(
                "note 2", {.selection = SelectionPath{},
                           .expected_project_revision =
                               source.session().project_snapshot().project_revision})
            .status.first == MessageLevel::Info);
    REQUIRE(source.session()
                .execute_command_string(
                    "set midiCC 74 0.25",
                    {.selection = select_element_in_cell({}, 0),
                     .expected_project_revision =
                         source.session().project_snapshot().project_revision})
                .status.first == MessageLevel::Info);
    REQUIRE(source.session()
                .execute_command_string(
                    "set midiCCLabel 74 Cutoff",
                    {.expected_project_revision =
                         source.session().project_snapshot().project_revision})
                .status.first == MessageLevel::Info);

    auto const expected = source.session().project_snapshot().project;

    auto blob = juce::MemoryBlock{};
    source.getStateInformation(blob);
    REQUIRE(blob.getSize() > 0);
    auto const serialized =
        std::string{static_cast<char const *>(blob.getData()), blob.getSize()};
    auto const encoded = nlohmann::json::parse(serialized);
    CHECK(encoded.at("kind") == "xen_processor_state");
    CHECK(encoded.at("binding").at("channel_id") ==
          source.session().instance_binding().channel_id);
    CHECK_FALSE(encoded.at("shared_snapshot").contains("history_entry_id"));
    CHECK(encoded.at("shared_snapshot").contains("project_revision"));
    CHECK(encoded.at("shared_snapshot").contains("state_revision"));
    CHECK(encoded.at("shared_snapshot").contains("document"));

    auto target = XenProcessor{};
    REQUIRE_NOTHROW(target.setStateInformation(blob.getData(), (int)blob.getSize()));

    auto const actual = target.session().project_snapshot().project;
    CHECK(actual == expected);
    auto const target_binding = target.session().instance_binding();
    auto const source_binding = source.session().instance_binding();
    CHECK(target_binding.session_id == source_binding.session_id);
    CHECK(target_binding.channel_id == source_binding.channel_id);
    CHECK(target_binding.instance_id != source_binding.instance_id);
}

TEST_CASE("Processor setStateInformation rejects raw project payloads",
          "[processor][state]")
{
    auto processor = XenProcessor{};
    auto const before_snapshot = processor.session().project_snapshot();
    auto const before_binding = processor.session().instance_binding();
    auto const raw_project = serialize_project(before_snapshot.project);

    REQUIRE_NOTHROW(
        processor.setStateInformation(raw_project.data(), (int)raw_project.size()));

    auto const after_snapshot = processor.session().project_snapshot();
    CHECK(after_snapshot.project == before_snapshot.project);
    CHECK(after_snapshot.history_entry_id == before_snapshot.history_entry_id);
    CHECK(after_snapshot.project_revision == before_snapshot.project_revision);
    CHECK(processor.session().instance_binding() == before_binding);
}

TEST_CASE("Processor setStateInformation ignores invalid payload safely",
          "[processor][state]")
{
    auto processor = XenProcessor{};
    auto const before_snapshot = processor.session().project_snapshot();
    auto const before_compilation = processor.midi_compilation_status();

    auto const invalid = "not json";
    REQUIRE_NOTHROW(processor.setStateInformation(
        invalid, (int)std::char_traits<char>::length(invalid)));

    auto const after_snapshot = processor.session().project_snapshot();
    CHECK(after_snapshot.project == before_snapshot.project);
    CHECK(after_snapshot.history_entry_id == before_snapshot.history_entry_id);
    CHECK(after_snapshot.project_revision == before_snapshot.project_revision);
    CHECK(processor.midi_compilation_status().requested_generation ==
          before_compilation.requested_generation);
}

TEST_CASE("Processor setStateInformation publishes and compiles snapshot on success",
          "[processor][state]")
{
    auto source = XenProcessor{};
    REQUIRE(
        source.session()
            .execute_command_string(
                "set key 5", {.expected_project_revision =
                                  source.session().project_snapshot().project_revision})
            .status.first == MessageLevel::Info);

    auto blob = juce::MemoryBlock{};
    source.getStateInformation(blob);
    REQUIRE(blob.getSize() > 0);

    auto target = XenProcessor{};
    auto const before = target.session().project_snapshot();

    REQUIRE_NOTHROW(target.setStateInformation(blob.getData(), (int)blob.getSize()));

    auto const after = target.session().project_snapshot();
    CHECK(after.history_entry_id != before.history_entry_id);
    CHECK(after.project_revision != before.project_revision);
    CHECK(target.midi_compilation_status().requested_generation > 0);
}

TEST_CASE("Processor equal-data restoration replaces project history",
          "[processor][state]")
{
    auto processor = XenProcessor{};
    auto blob = juce::MemoryBlock{};
    processor.getStateInformation(blob);
    REQUIRE(blob.getSize() > 0);

    auto const before = processor.session().project_snapshot();
    REQUIRE_NOTHROW(processor.setStateInformation(blob.getData(), (int)blob.getSize()));

    auto const after = processor.session().project_snapshot();
    CHECK(after.project == before.project);
    CHECK(after.history_entry_id != before.history_entry_id);
    CHECK(after.project_revision == before.project_revision);
    CHECK(after.state_revision == before.state_revision);
    CHECK(processor.session()
              .execute_command_string(
                  "undo", {.expected_project_revision =
                               processor.session().project_snapshot().project_revision})
              .status.second == "Nothing to undo.");
}
