#include <string>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <xen/bridge_serialize.hpp>
#include <xen/webview_bridge.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto temporary_keymap_file() -> juce::File
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("xen-keymap", ".json", false);
}

auto temporary_workspace_file() -> juce::File
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("xen-workspace", ".json", false);
}

auto make_processor() -> XenProcessor
{
    return XenProcessor{SubmissionEffects::FailurePoint::None,
                        temporary_workspace_file()};
}

auto make_bridge(XenProcessor &processor) -> WebviewBridge
{
    return WebviewBridge{processor, temporary_keymap_file()};
}

auto request(std::string name, nlohmann::json payload = nlohmann::json::object())
    -> std::string
{
    return nlohmann::json{
        {"protocol", bridge::protocol},  {"type", "request"},
        {"name", std::move(name)},       {"request_id", "test"},
        {"payload", std::move(payload)},
    }
        .dump();
}

auto response(WebviewBridge &bridge, std::string name,
              nlohmann::json payload = nlohmann::json::object()) -> nlohmann::json
{
    return nlohmann::json::parse(
        bridge.handle_request_json(request(std::move(name), std::move(payload))));
}

} // namespace

TEST_CASE("Bridge session hello contains session resources only", "[core][bridge]")
{
    auto processor = make_processor();
    auto host_bridge = make_bridge(processor);
    auto const message = response(host_bridge, "session.hello",
                                  {
                                      {"protocol", bridge::protocol},
                                      {"frontend_app", "test"},
                                      {"frontend_version", "1"},
                                  });

    auto const &payload = message.at("payload");
    CHECK(payload.at("protocol") == bridge::protocol);
    CHECK(payload.at("project_schema_version") == bridge::project_schema_version);
    CHECK(payload.at("library_schema_version") == bridge::library_schema_version);
    CHECK(payload.contains("catalog"));
    CHECK(payload.contains("keymap"));
    CHECK(payload.at("keymap").at("schema_version") == 1);
    CHECK(payload.at("keymap").at("key_semantics") == "KeyboardEvent.key");
    CHECK(payload.at("keymap").contains("bindings"));
    CHECK(payload.at("keymap").contains("overrides"));
    CHECK_FALSE(payload.contains("project"));
    CHECK_FALSE(payload.contains("library"));
}

TEST_CASE("Bridge project and library resources are separated", "[core][bridge]")
{
    auto processor = make_processor();
    auto host_bridge = make_bridge(processor);

    auto const state = response(host_bridge, "state.get").at("payload");
    CHECK(state.at("schema_version") == bridge::project_schema_version);
    CHECK(state.contains("project"));
    CHECK(state.contains("project_revision"));
    CHECK(state.contains("history_entry_id"));
    CHECK_FALSE(state.contains("library"));
    CHECK_FALSE(state.contains("paths"));

    auto const library = response(host_bridge, "library.get").at("payload");
    CHECK(library.at("schema_version") == bridge::library_schema_version);
    CHECK(library.contains("library_revision"));
    CHECK(library.contains("scales"));
    CHECK(library.contains("chords"));
    CHECK(library.contains("measures"));
    CHECK(library.contains("tunings"));
    CHECK(library.contains("paths"));
    CHECK_FALSE(library.contains("project"));
    CHECK_FALSE(library.contains("project_revision"));
}

TEST_CASE("Bridge command response contains current project snapshot", "[core][bridge]")
{
    auto processor = make_processor();
    auto host_bridge = make_bridge(processor);
    auto const revision = processor.get_project_snapshot().project_revision.value();
    auto const message =
        response(host_bridge, "command.execute",
                 {
                     {"command", "set key 7"},
                     {"context", {{"expected_project_revision", revision}}},
                 });

    auto const &payload = message.at("payload");
    CHECK(payload.at("status").at("level") == "info");
    CHECK(payload.contains("suggested_selection"));
    CHECK(payload.at("snapshot").contains("project"));
    CHECK_FALSE(payload.at("snapshot").contains("library"));
}

TEST_CASE("Bridge updates and resets individual keymap overrides", "[core][bridge]")
{
    auto processor = make_processor();
    auto host_bridge = make_bridge(processor);
    auto const initial = response(host_bridge, "keymap.get").at("payload");
    auto const revision = initial.at("revision").get<std::uint64_t>();
    auto const trigger = nlohmann::json{
        {"key", "q"},
        {"modifiers", {{"shift", false}, {"command", false}, {"alt", false}}},
    };
    auto const updated =
        response(host_bridge, "keymap.override.set",
                 {
                     {"expected_revision", revision},
                     {"context", "sequence"},
                     {"trigger", trigger},
                     {"target", {{"type", "command"}, {"command", "rest"}}},
                 })
            .at("payload");
    CHECK(updated.at("revision") == revision + 1);
    REQUIRE(updated.at("overrides").size() == 1);
    CHECK(updated.at("overrides").front().at("target").at("command") == "rest");

    auto const stale =
        response(host_bridge, "keymap.override.set",
                 {
                     {"expected_revision", revision},
                     {"context", "sequence"},
                     {"trigger", trigger},
                     {"target", {{"type", "command"}, {"command", "note"}}},
                 })
            .at("payload");
    CHECK(stale.at("error").at("code") == "invalid_request");

    auto const restored = response(host_bridge, "keymap.override.remove",
                                   {
                                       {"expected_revision", updated.at("revision")},
                                       {"context", "sequence"},
                                       {"trigger", trigger},
                                   })
                              .at("payload");
    CHECK(restored.at("overrides").empty());

    auto const reupdated =
        response(host_bridge, "keymap.override.set",
                 {
                     {"expected_revision", restored.at("revision")},
                     {"context", "sequence"},
                     {"trigger", trigger},
                     {"target", {{"type", "command"}, {"command", "rest"}}},
                 })
            .at("payload");
    auto const reset = response(host_bridge, "keymap.reset",
                                {{"expected_revision", reupdated.at("revision")}})
                           .at("payload");
    CHECK(reset.at("overrides").empty());
    CHECK(reset.at("revision") == revision + 4);
}

TEST_CASE("Bridge changed events use independent resource payloads", "[core][bridge]")
{
    auto processor = make_processor();
    auto host_bridge = make_bridge(processor);

    auto const state =
        nlohmann::json::parse(host_bridge.make_state_changed_event_json());
    CHECK(state.at("name") == "state.changed");
    CHECK(state.at("payload").contains("project"));
    CHECK_FALSE(state.at("payload").contains("library_revision"));

    auto const library =
        nlohmann::json::parse(host_bridge.make_library_changed_event_json());
    CHECK(library.at("name") == "library.changed");
    CHECK(library.at("payload").contains("library_revision"));
    CHECK_FALSE(library.at("payload").contains("project"));

    auto const keymap =
        nlohmann::json::parse(host_bridge.make_keymap_changed_event_json());
    CHECK(keymap.at("name") == "keymap.changed");
    CHECK(keymap.at("payload").contains("revision"));
    CHECK(keymap.at("payload").contains("bindings"));
}
