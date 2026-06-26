#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <xen/bridge_serialize.hpp>
#include <xen/sequencer_session.hpp>
#include <xen/webview_bridge.hpp>
#include <xen/webview_bridge_services.hpp>

using namespace xen;

namespace
{

auto temporary_keymap_file() -> std::filesystem::path
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("xen-keymap", ".json", false)
        .getFullPathName()
        .toStdString();
}

auto temporary_workspace_file() -> juce::File
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("xen-workspace", ".json", false);
}

auto make_session() -> SequencerSession
{
    return SequencerSession{SubmissionEffects::FailurePoint::None,
                            temporary_workspace_file().getFullPathName().toStdString()};
}

auto make_bridge(SequencerSession &session) -> WebviewBridge
{
    return WebviewBridge{session, temporary_keymap_file()};
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

class FakeApplicationService final : public bridge::ApplicationBridgeService
{
  public:
    ProjectSnapshot project{
        .project = ProjectState{},
        .history_entry_id = HistoryEntryId{3},
        .project_revision = ProjectRevision{7},
    };
    LibrarySnapshot library{
        .library = ContentLibrary{},
        .workspace =
            WorkspaceSettings{
                .sequence_directory =
                    std::filesystem::path{
                        juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getFullPathName()
                            .toStdString()},
                .tuning_directory =
                    std::filesystem::path{
                        juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getFullPathName()
                            .toStdString()},
            },
        .library_revision = LibraryRevision{11},
    };
    InstanceBinding binding{
        .session_id = "session-test",
        .instance_id = "instance-test",
        .channel_id = DEFAULT_CHANNEL_ID,
    };
    std::string executed_command{};
    CommandContext executed_context{};

    [[nodiscard]] auto project_snapshot() const -> ProjectSnapshot override
    {
        return project;
    }

    [[nodiscard]] auto instance_binding() const -> InstanceBinding override
    {
        return binding;
    }

    [[nodiscard]] auto library_snapshot() const -> LibrarySnapshot override
    {
        return library;
    }

    [[nodiscard]] auto command_catalog_metadata() const
        -> std::vector<CatalogCommandMetadata> override
    {
        return {};
    }

    [[nodiscard]] auto execute_command_string(std::string const &command,
                                              CommandContext const &context)
        -> CommandApplicationResult override
    {
        executed_command = command;
        executed_context = context;
        return {
            .status = {MessageLevel::Info, "fake command"},
            .suggested_selection = std::nullopt,
        };
    }

    void set_channel_id(ChannelId channel_id) override
    {
        binding.channel_id = std::move(channel_id);
    }
};

class FakeKeymapService final : public bridge::KeymapBridgeService
{
  public:
    KeymapSnapshot current{.revision = 5, .bindings = default_keymap()};

    [[nodiscard]] auto snapshot() const -> KeymapSnapshot override
    {
        return current;
    }

    [[nodiscard]] auto revision() const noexcept -> std::uint64_t override
    {
        return current.revision;
    }

    auto set_override(std::uint64_t expected_revision, std::string context,
                      KeymapTrigger trigger, std::optional<KeymapTarget> target)
        -> KeymapSnapshot override
    {
        if (expected_revision != current.revision)
        {
            throw std::invalid_argument{"stale fake keymap revision"};
        }
        ++current.revision;
        current.overrides.push_back({
            .context = std::move(context),
            .trigger = std::move(trigger),
            .target = std::move(target),
        });
        return current;
    }

    auto remove_override(std::uint64_t expected_revision, std::string const &,
                         KeymapTrigger const &) -> KeymapSnapshot override
    {
        if (expected_revision != current.revision)
        {
            throw std::invalid_argument{"stale fake keymap revision"};
        }
        ++current.revision;
        current.overrides.clear();
        return current;
    }

    auto reset(std::uint64_t expected_revision) -> KeymapSnapshot override
    {
        if (expected_revision != current.revision)
        {
            throw std::invalid_argument{"stale fake keymap revision"};
        }
        ++current.revision;
        current.overrides.clear();
        return current;
    }
};

class FakeLibraryService final : public bridge::LibraryBridgeService
{
  public:
    [[nodiscard]] auto make_payload(LibrarySnapshot const &snapshot) const
        -> nlohmann::json override
    {
        return {
            {"schema_version", bridge::library_schema_version},
            {"library_revision", snapshot.library_revision.value()},
            {"fake_library", true},
        };
    }
};

class FakeLibraryFilePort final : public bridge::LibraryFilePort
{
  public:
    [[nodiscard]] auto library_root() const -> std::filesystem::path override
    {
        return "/fake/library";
    }

    [[nodiscard]] auto measure_files(std::filesystem::path const &) const
        -> std::vector<bridge::LibraryFileEntry> override
    {
        return {{
            .name = "measure.xss",
            .relative_path = "folder/measure.xss",
            .stem = "folder/measure",
            .path = "/fake/sequences/folder/measure.xss",
        }};
    }

    [[nodiscard]] auto tuning_files(std::filesystem::path const &) const
        -> std::vector<bridge::LibraryTuningEntry> override
    {
        return {{
            .file =
                {
                    .name = "tuning.scl",
                    .relative_path = "tuning.scl",
                    .stem = "tuning",
                    .path = "/fake/tunings/tuning.scl",
                },
            .description = "fake tuning",
            .intervals = {0.f, 100.f},
            .octave = 1200.f,
        }};
    }
};

auto fake_response(bridge::BridgeRequestDispatcher &dispatcher, std::string name,
                   nlohmann::json payload = nlohmann::json::object()) -> nlohmann::json
{
    return nlohmann::json::parse(
        dispatcher.handle_request_json(request(std::move(name), std::move(payload))));
}

auto read_contract_doc() -> std::string
{
    auto const candidates = std::vector<std::filesystem::path>{
        "docs/development/frontend_backend_contract.md",
        "../docs/development/frontend_backend_contract.md",
    };
    for (auto const &path : candidates)
    {
        auto input = std::ifstream{path};
        if (!input)
        {
            continue;
        }
        auto buffer = std::ostringstream{};
        buffer << input.rdbuf();
        return buffer.str();
    }
    return {};
}

} // namespace

TEST_CASE("Bridge session hello contains session resources only", "[core][bridge]")
{
    auto session = make_session();
    auto host_bridge = make_bridge(session);
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
    CHECK(payload.at("catalog").at("schema_version") == 2);
    for (auto const &command : payload.at("catalog").at("commands"))
    {
        for (auto const &argument : command.at("arguments"))
        {
            for (auto const &constraint : argument.at("constraints"))
            {
                CHECK((constraint.at("minimum").is_null() ||
                       constraint.at("minimum").is_number()));
                CHECK((constraint.at("maximum").is_null() ||
                       constraint.at("maximum").is_number()));
            }
        }
    }
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
    auto session = make_session();
    auto host_bridge = make_bridge(session);

    auto const state = response(host_bridge, "state.get").at("payload");
    CHECK(state.at("schema_version") == bridge::project_schema_version);
    CHECK(state.contains("project"));
    CHECK(state.contains("project_revision"));
    CHECK(state.contains("history_entry_id"));
    CHECK(state.at("project").at("composition").contains("loop_region"));
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
    auto session = make_session();
    auto host_bridge = make_bridge(session);
    auto const revision = session.project_snapshot().project_revision.value();
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
    auto session = make_session();
    auto host_bridge = make_bridge(session);
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
    auto session = make_session();
    auto host_bridge = make_bridge(session);

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

TEST_CASE("Bridge dispatcher handles service requests with fake services",
          "[core][bridge]")
{
    auto application = FakeApplicationService{};
    auto library = FakeLibraryService{};
    auto keymap = FakeKeymapService{};
    auto dispatcher = bridge::BridgeRequestDispatcher{application, library, keymap};

    auto const hello = fake_response(dispatcher, "session.hello",
                                     {
                                         {"protocol", bridge::protocol},
                                         {"frontend_app", "test"},
                                         {"frontend_version", "1"},
                                     });
    CHECK(hello.at("payload").at("catalog").at("schema_version") ==
          bridge::catalog_schema_version);
    CHECK(hello.at("payload").at("keymap").at("revision") == 5);
    CHECK(hello.at("payload").at("binding").at("channel_id") == DEFAULT_CHANNEL_ID);

    auto const state = fake_response(dispatcher, "state.get").at("payload");
    CHECK(state.at("project_revision") == 7);

    auto const binding = fake_response(dispatcher, "session.binding.get").at("payload");
    CHECK(binding.at("instance_id") == "instance-test");

    auto const updated_binding =
        fake_response(dispatcher, "session.binding.set", {{"channel_id", "peer"}})
            .at("payload");
    CHECK(updated_binding.at("channel_id") == "peer");
    CHECK(application.binding.channel_id == "peer");

    auto const command =
        fake_response(dispatcher, "command.execute",
                      {
                          {"command", "fake"},
                          {"context",
                           {
                               {"expected_project_revision", 7},
                               {"selection", {{"path", nlohmann::json::array()}}},
                               {"active_measure_target",
                                {
                                    {"row_index", 0},
                                    {"column_index", 1},
                                    {"measure_id", 2},
                                }},
                           }},
                      })
            .at("payload");
    CHECK(command.at("status").at("message") == "fake command");
    CHECK(application.executed_command == "fake");
    REQUIRE(application.executed_context.expected_project_revision.has_value());
    CHECK(application.executed_context.expected_project_revision->value() == 7);
    REQUIRE(application.executed_context.selection.has_value());
    CHECK(application.executed_context.selection->path.empty());
    REQUIRE(application.executed_context.active_measure_target.has_value());
    CHECK(application.executed_context.active_measure_target->row_index == 0);
    CHECK(application.executed_context.active_measure_target->column_index == 1);
    CHECK(application.executed_context.active_measure_target->measure_id == 2);

    auto const library_payload = fake_response(dispatcher, "library.get").at("payload");
    CHECK(library_payload.at("fake_library") == true);
    CHECK(library_payload.at("library_revision") == 11);
}

TEST_CASE("Bridge library payload uses file port entries", "[core][bridge]")
{
    auto files = FakeLibraryFilePort{};
    auto service = bridge::JuceLibraryBridgeService{files};
    auto snapshot = LibrarySnapshot{
        .library = ContentLibrary{},
        .workspace =
            WorkspaceSettings{
                .sequence_directory = "/fake/sequences",
                .tuning_directory = "/fake/tunings",
            },
        .library_revision = LibraryRevision{12},
    };

    auto const payload = service.make_payload(snapshot);
    CHECK(payload.at("paths").at("library") == "/fake/library");
    REQUIRE(payload.at("measures").size() == 1);
    CHECK(payload.at("measures").front().at("command") ==
          "load measure \"folder/measure\"");
    REQUIRE(payload.at("tunings").size() == 1);
    CHECK(payload.at("tunings").front().at("description") == "fake tuning");
}

TEST_CASE("Bridge dispatcher handles keymap requests with fake services",
          "[core][bridge]")
{
    auto application = FakeApplicationService{};
    auto library = FakeLibraryService{};
    auto keymap = FakeKeymapService{};
    auto dispatcher = bridge::BridgeRequestDispatcher{application, library, keymap};
    auto const trigger = nlohmann::json{
        {"key", "q"},
        {"modifiers", {{"shift", false}, {"command", false}, {"alt", false}}},
    };

    auto const set =
        fake_response(dispatcher, "keymap.override.set",
                      {
                          {"expected_revision", 5},
                          {"context", "sequence"},
                          {"trigger", trigger},
                          {"target", {{"type", "command"}, {"command", "rest"}}},
                      })
            .at("payload");
    CHECK(set.at("revision") == 6);
    REQUIRE(set.at("overrides").size() == 1);

    auto const remove = fake_response(dispatcher, "keymap.override.remove",
                                      {
                                          {"expected_revision", 6},
                                          {"context", "sequence"},
                                          {"trigger", trigger},
                                      })
                            .at("payload");
    CHECK(remove.at("revision") == 7);
    CHECK(remove.at("overrides").empty());

    auto const reset =
        fake_response(dispatcher, "keymap.reset", {{"expected_revision", 7}})
            .at("payload");
    CHECK(reset.at("revision") == 8);
}

TEST_CASE("Bridge protocol errors are deterministic", "[core][bridge]")
{
    auto application = FakeApplicationService{};
    auto library = FakeLibraryService{};
    auto keymap = FakeKeymapService{};
    auto dispatcher = bridge::BridgeRequestDispatcher{application, library, keymap};

    auto const unknown = fake_response(dispatcher, "missing.request").at("payload");
    CHECK(unknown.at("error").at("code") == "invalid_request");
    CHECK(unknown.at("error").at("message") == "Unknown request name: missing.request");

    auto const invalid_schema = fake_response(dispatcher, "session.hello",
                                              {
                                                  {"protocol", "xen.bridge.bad"},
                                                  {"frontend_app", "test"},
                                                  {"frontend_version", "1"},
                                              })
                                    .at("payload");
    CHECK(invalid_schema.at("error").at("code") == "unsupported_protocol");
}

TEST_CASE("Bridge documentation tracks schema constants and catalog fields",
          "[core][bridge][contract]")
{
    auto const doc = read_contract_doc();
    REQUIRE_FALSE(doc.empty());

    CHECK(doc.find("project_schema_version: " +
                   std::to_string(bridge::project_schema_version) + ";") !=
          std::string::npos);
    CHECK(doc.find("library_schema_version: " +
                   std::to_string(bridge::library_schema_version) + ";") !=
          std::string::npos);
    CHECK(doc.find("schema_version: " + std::to_string(bridge::catalog_schema_version) +
                   ";") != std::string::npos);
    CHECK(doc.find("keywords: string[];") != std::string::npos);
}
