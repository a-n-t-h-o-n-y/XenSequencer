#include <string>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <xen/bridge_serialize.hpp>
#include <xen/webview_bridge.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

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
    auto processor = XenProcessor{};
    auto host_bridge = WebviewBridge{processor};
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
    CHECK_FALSE(payload.contains("project"));
    CHECK_FALSE(payload.contains("library"));
}

TEST_CASE("Bridge project and library resources are separated", "[core][bridge]")
{
    auto processor = XenProcessor{};
    auto host_bridge = WebviewBridge{processor};

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
    auto processor = XenProcessor{};
    auto host_bridge = WebviewBridge{processor};
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

TEST_CASE("Removed keymap endpoint is rejected", "[core][bridge]")
{
    auto processor = XenProcessor{};
    auto host_bridge = WebviewBridge{processor};
    auto const message = response(host_bridge, "keymap.get");
    CHECK(message.at("payload").at("error").at("code") == "invalid_request");
}

TEST_CASE("Bridge changed events use independent resource payloads", "[core][bridge]")
{
    auto processor = XenProcessor{};
    auto host_bridge = WebviewBridge{processor};

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
}
