#include <string>
#include <vector>

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

    auto const hello =
        request("session.hello",
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
    REQUIRE(response["payload"].contains("reference"));
    REQUIRE(response["payload"]["reference"].contains("commands"));
    REQUIRE(response["payload"]["reference"].contains("keybindings"));
    CHECK_FALSE(response["payload"]["reference"]["commands"].empty());
    CHECK_FALSE(response["payload"]["reference"]["keybindings"].empty());
}

TEST_CASE("WebviewBridge state.get returns snapshot payload", "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const response = parse_response(bridge.handle_request_json(
        request("state.get", nlohmann::json::object()).dump()));

    auto const &payload = response.at("payload");
    CHECK(payload.at("schema_version") == bridge::snapshot_schema_version);
    CHECK(payload.contains("snapshot_version"));
    CHECK(payload.contains("history_entry_id"));
    CHECK(payload.contains("project_revision"));
    CHECK_FALSE(payload.contains("commit_id"));
    CHECK(payload.at("history_entry_id").is_number_unsigned());
    CHECK(payload.at("project_revision").is_number_unsigned());
    CHECK(payload.contains("engine"));
    CHECK(payload.contains("editor"));
    CHECK(payload.contains("library"));
    CHECK(payload.at("editor").at("selected").contains("path"));
    CHECK(payload.at("editor").at("selected").at("path").is_array());
    CHECK_FALSE(payload.at("editor").at("selected").contains("cell"));
    CHECK_FALSE(payload.at("editor").at("selected").contains("element_index"));
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

    auto const structured = parse_response(bridge.handle_request_json(
        request("command.complete", nlohmann::json{{"partial", "set "}}).dump()));
    REQUIRE(structured.at("payload").contains("candidates"));
    CHECK_FALSE(structured.at("payload").at("candidates").empty());

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

    auto const response = parse_response(bridge.handle_request_json(
        request("keymap.get", nlohmann::json::object()).dump()));

    auto const &keymap = response.at("payload").at("keymap");
    REQUIRE(keymap.contains("SequenceView"));
    CHECK(keymap.at("SequenceView").is_object());
    CHECK_FALSE(keymap.at("SequenceView").empty());
}

TEST_CASE("WebviewBridge library.get returns filesystem-backed library status",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const response = parse_response(bridge.handle_request_json(
        request("library.get", nlohmann::json::object()).dump()));

    auto const &payload = response.at("payload");
    REQUIRE(payload.contains("paths"));
    REQUIRE(payload.at("paths").contains("library"));
    REQUIRE(payload.at("paths").contains("sequences"));
    REQUIRE(payload.at("paths").contains("tunings"));

    REQUIRE(payload.contains("measures"));
    REQUIRE(payload.at("measures").is_array());
    CHECK_FALSE(payload.at("measures").empty());
    REQUIRE(payload.at("measures").at(0).contains("relative_path"));
    REQUIRE(payload.at("measures").at(0).contains("command"));
    REQUIRE(payload.contains("tunings"));
    REQUIRE(payload.at("tunings").is_array());
    for (auto const &tuning : payload.at("tunings"))
    {
        REQUIRE(tuning.contains("description"));
        REQUIRE(tuning.contains("intervals"));
        REQUIRE(tuning.contains("octave"));
        REQUIRE(tuning.contains("note_count"));
    }

    REQUIRE(payload.contains("scales"));
    REQUIRE(payload.at("scales").is_array());
    CHECK_FALSE(payload.at("scales").empty());
    REQUIRE(payload.at("scales").at(0).contains("intervals"));
    CHECK(payload.at("scales").at(0).at("command") == "set scale \"chromatic\"");

    REQUIRE(payload.contains("chords"));
    REQUIRE(payload.at("chords").is_array());
    CHECK_FALSE(payload.at("chords").empty());
    REQUIRE(payload.at("chords").at(0).contains("intervals"));
    REQUIRE(payload.at("chords").at(0).contains("command"));

    REQUIRE(payload.contains("commands"));
    CHECK(payload.at("commands").at("reload_scales") == "load scales");
    CHECK(payload.at("commands").at("reload_chords") == "load chords");

    REQUIRE(payload.contains("active"));
    REQUIRE(payload.at("active").contains("tuning_name"));
    REQUIRE(payload.at("active").contains("scale_name"));

    auto has_directory_segment = false;
    for (auto const &entry : payload.at("measures"))
    {
        auto const rel = entry.at("relative_path").get<std::string>();
        if (rel.find('/') != std::string::npos)
        {
            has_directory_segment = true;
            break;
        }
    }
    CHECK(has_directory_segment);
}

TEST_CASE("WebviewBridge transport event helpers produce bridge envelopes",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const phase_sync = nlohmann::json::parse(bridge.make_phase_sync_event_json(
        WebviewBridge::MeasurePhase{.phase = 0.25}, 120.f));
    CHECK(phase_sync.at("type") == "event");
    CHECK(phase_sync.at("name") == "transport.phase.sync");
    CHECK(phase_sync.at("payload").at("bpm") == 120.f);
    CHECK(phase_sync.at("payload").at("phase") == 0.25);

    auto const transport_stopped =
        nlohmann::json::parse(bridge.make_transport_stopped_event_json());
    CHECK(transport_stopped.at("type") == "event");
    CHECK(transport_stopped.at("name") == "transport.stopped");
    CHECK(transport_stopped.at("payload").empty());
}
