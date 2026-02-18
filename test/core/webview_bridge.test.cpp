#include <string>

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <xen/bridge_serialize.hpp>
#include <xen/webview_bridge.hpp>
#include <xen/xen_processor.hpp>

using namespace xen;

namespace
{

auto request(std::string const &name, nlohmann::json payload,
             std::string const &request_id = "req-1") -> nlohmann::json
{
    return nlohmann::json{
        {"protocol", bridge::protocol},
        {"type", "request"},
        {"name", name},
        {"request_id", request_id},
        {"payload", std::move(payload)},
    };
}

auto parse_response(std::string const &response_text) -> nlohmann::json
{
    auto response = nlohmann::json::parse(response_text);
    REQUIRE(response.is_object());
    REQUIRE(response.contains("protocol"));
    REQUIRE(response.contains("type"));
    REQUIRE(response.contains("name"));
    REQUIRE(response.contains("payload"));
    return response;
}

} // namespace

TEST_CASE("WebviewBridge validates request protocol and returns structured errors",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto invalid = request("state.get", nlohmann::json::object());
    invalid["protocol"] = "xen.bridge.v0";

    auto const response = parse_response(bridge.handle_request_json(invalid.dump()));
    REQUIRE(response["payload"].contains("error"));
    CHECK(response["payload"]["error"]["code"] == "unsupported_protocol");
}

TEST_CASE("WebviewBridge handles session hello with fixed contract",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const hello = request(
        "session.hello",
        nlohmann::json{
            {"protocol", bridge::protocol},
            {"snapshot_schema_version", bridge::snapshot_schema_version},
            {"frontend_app", "xen-web-ui"},
            {"frontend_version", "0.1.0"},
        });

    auto const response = parse_response(bridge.handle_request_json(hello.dump()));
    CHECK(response["type"] == "response");
    CHECK(response["name"] == "session.hello");
    CHECK(response["request_id"] == "req-1");
    CHECK(response["payload"]["protocol"] == bridge::protocol);
    CHECK(response["payload"]["snapshot_schema_version"] ==
          bridge::snapshot_schema_version);
}

TEST_CASE("WebviewBridge state.get returns snapshot payload", "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const response = parse_response(
        bridge.handle_request_json(request("state.get", nlohmann::json::object()).dump()));

    auto const &payload = response.at("payload");
    CHECK(payload.at("schema_version") == bridge::snapshot_schema_version);
    CHECK(payload.contains("snapshot_version"));
    CHECK(payload.contains("commit_id"));
    CHECK(payload.contains("engine"));
    CHECK(payload.contains("editor"));
    CHECK(payload.contains("library"));
}

TEST_CASE("WebviewBridge command.execute returns status and snapshot",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const response = parse_response(bridge.handle_request_json(
        request("command.execute", nlohmann::json{{"command", "set key 9"}}).dump()));

    auto const &payload = response.at("payload");
    CHECK(payload.at("status").at("level") == "info");
    CHECK(payload.at("snapshot").at("engine").at("key") == 9);
}

TEST_CASE("WebviewBridge catalog and completion endpoints respond",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const complete_text = parse_response(bridge.handle_request_json(
        request("command.completeText", nlohmann::json{{"partial", "set ba"}}).dump()));
    CHECK(complete_text.at("payload").contains("suffix"));

    auto const complete_id = parse_response(bridge.handle_request_json(
        request("command.completeId", nlohmann::json{{"partial", "set ba"}}).dump()));
    CHECK(complete_id.at("payload").contains("id_suffix"));

    auto const catalog = parse_response(bridge.handle_request_json(
        request("catalog.get", nlohmann::json::object()).dump()));
    REQUIRE(catalog.at("payload").contains("commands"));
    CHECK_FALSE(catalog.at("payload").at("commands").empty());
}

TEST_CASE("WebviewBridge keymap.get returns merged raw keymap",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const response = parse_response(
        bridge.handle_request_json(request("keymap.get", nlohmann::json::object()).dump()));

    auto const &keymap = response.at("payload").at("keymap");
    REQUIRE(keymap.contains("SequenceView"));
    CHECK(keymap.at("SequenceView").contains("w"));
}
