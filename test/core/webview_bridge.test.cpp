#include <algorithm>
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
    return response;
}

auto root_selection_json() -> nlohmann::json
{
    return nlohmann::json{{"path", nlohmann::json::array()}};
}

} // namespace

TEST_CASE("WebviewBridge state.get omits backend editor state",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const response = parse_response(bridge.handle_request_json(
        request("state.get", nlohmann::json::object()).dump()));

    auto const &payload = response.at("payload");
    CHECK(payload.at("schema_version") == bridge::snapshot_schema_version);
    CHECK(payload.contains("history_entry_id"));
    CHECK(payload.contains("project_revision"));
    CHECK(payload.contains("engine"));
    CHECK(payload.contains("library"));
    CHECK_FALSE(payload.contains("editor"));
}

TEST_CASE("WebviewBridge session.hello delivers versioned immutable catalog",
          "[core][webview-bridge][catalog]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    auto const response = parse_response(bridge.handle_request_json(
        request("session.hello",
                {{"protocol", bridge::protocol},
                 {"snapshot_schema_version", bridge::snapshot_schema_version},
                 {"frontend_app", "test"},
                 {"frontend_version", "1"}})
            .dump()));
    auto const &payload = response.at("payload");
    CHECK(payload.at("catalog").at("schema_version") == bridge::catalog_schema_version);
    REQUIRE_FALSE(payload.at("catalog").at("commands").empty());
    auto const &command = payload.at("catalog").at("commands").front();
    CHECK(command.contains("path"));
    CHECK(command.contains("description"));
    CHECK(command.contains("accepts_pattern_prefix"));
    CHECK(command.contains("target_requirement"));
    CHECK(command.contains("arguments"));
    CHECK(payload.contains("keymap"));
    CHECK_FALSE(payload.contains("reference"));

    auto const &commands = payload.at("catalog").at("commands");
    auto const set_key =
        std::find_if(commands.begin(), commands.end(), [](nlohmann::json const &entry) {
            return entry.at("path") == nlohmann::json::array({"set", "key"});
        });
    REQUIRE(set_key != commands.end());
    CHECK(set_key->at("target_requirement") == "none");
    auto const &argument = set_key->at("arguments").front();
    CHECK(argument.at("kind") == "integer");
    CHECK(argument.at("display_name") == "key");
    CHECK_FALSE(argument.at("required").get<bool>());
    CHECK(argument.at("default_value") == "0");
    CHECK(argument.at("constraints").is_array());
}

TEST_CASE("WebviewBridge removed catalog and completion requests are unknown",
          "[core][webview-bridge][catalog]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};

    for (auto const *name : {"catalog.get", "command.complete", "command.completeText",
                             "command.completeId"})
    {
        auto const response = parse_response(
            bridge.handle_request_json(request(name, nlohmann::json::object()).dump()));
        CHECK(response.at("payload").at("error").at("code") == "invalid_request");
        CHECK(response.at("payload").at("error").at("message").get<std::string>().find(
                  "Unknown request name") != std::string::npos);
    }
}

TEST_CASE("WebviewBridge command.execute parses selection and encodes suggestions",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};
    auto const revision = processor.get_engine_snapshot().project_revision.value();

    auto const response = parse_response(bridge.handle_request_json(
        request("command.execute",
                nlohmann::json{
                    {"command", "note 9"},
                    {"context",
                     {{"expected_project_revision", revision},
                      {"selection", root_selection_json()}}},
                })
            .dump()));

    auto const &payload = response.at("payload");
    CHECK(payload.at("status").at("level") == "info");
    REQUIRE(payload.contains("suggested_selection"));
    CHECK(payload.at("suggested_selection").at("path").is_array());
}

TEST_CASE("WebviewBridge validates typed selection request context",
          "[core][webview-bridge]")
{
    auto processor = XenProcessor{};
    auto bridge = WebviewBridge{processor};
    auto invalid_context = nlohmann::json{
        {"expected_project_revision",
         processor.get_engine_snapshot().project_revision.value()},
        {"selection",
         {{"path",
           nlohmann::json::array({nlohmann::json{{"kind", "bad"}, {"index", 0}}})}}},
    };

    auto const invalid = parse_response(
        bridge.handle_request_json(request("command.execute",
                                           nlohmann::json{
                                               {"command", "note 9"},
                                               {"context", invalid_context},
                                           })
                                       .dump()));

    CHECK(invalid.at("payload").at("error").at("code") == "invalid_request");
}
