#include <filesystem>
#include <fstream>
#include <limits>
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

inline constexpr auto HIGH_KEYMAP_REVISION = 18'446'744'073'709'551'600ULL;
inline constexpr auto HIGH_PREFERENCES_REVISION = 18'446'744'073'709'551'500ULL;

auto temporary_keymap_file() -> std::filesystem::path
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("xen-keymap", ".json", false)
        .getFullPathName()
        .toStdString();
}

auto temporary_preferences_file() -> std::filesystem::path
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("xen-preferences", ".json", false)
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
    return WebviewBridge{session, temporary_keymap_file(),
                         temporary_preferences_file()};
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
        .state_revision = StateRevision{9},
    };
    LibrarySnapshot library{
        .library = ContentLibrary{},
        .workspace =
            WorkspaceSettings{
                .content_directory =
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

    [[nodiscard]] auto begin_preview(ProjectRevision) -> PreviewControlResult override
    {
        project.preview_active = true;
        return {
            .status = {MessageLevel::Info, "Preview started."},
            .preview_id = "preview-test",
        };
    }

    [[nodiscard]] auto begin_modulation_preview(ProjectRevision, ModulationTarget)
        -> PreviewControlResult override
    {
        project.preview_active = true;
        return {
            .status = {MessageLevel::Info, "Modulation preview started."},
            .preview_id = "modulation-preview-test",
        };
    }

    [[nodiscard]] auto update_modulation_preview(ModulationPreviewUpdate const &update)
        -> ModulationPreviewUpdateResult override
    {
        project.project_revision = ProjectRevision{8};
        project.state_revision = StateRevision{10};
        return {
            .status = {MessageLevel::Info, "Modulation preview updated."},
            .preview_id = update.preview_id,
            .accepted_update_sequence = update.update_sequence,
            .accepted = true,
            .project_changed = true,
            .project_revision = project.project_revision,
            .state_revision = project.state_revision,
        };
    }

    [[nodiscard]] auto commit_preview(PreviewId const &, ProjectRevision)
        -> PreviewControlResult override
    {
        project.preview_active = false;
        return {.status = {MessageLevel::Info, "Preview committed."}};
    }

    [[nodiscard]] auto cancel_preview(PreviewId const &, ProjectRevision)
        -> PreviewControlResult override
    {
        project.preview_active = false;
        return {.status = {MessageLevel::Info, "Preview cancelled."}};
    }

    [[nodiscard]] auto create_project(ProjectRevision, bool)
        -> DocumentOperationResult override
    {
        return {.snapshot = project};
    }

    [[nodiscard]] auto open_project(std::string relative_path, ProjectRevision, bool)
        -> DocumentOperationResult override
    {
        project.document.relative_path = relative_path;
        return {.snapshot = project};
    }

    [[nodiscard]] auto save_project(ProjectRevision) -> DocumentOperationResult override
    {
        return {.snapshot = project};
    }

    [[nodiscard]] auto save_project_as(std::string relative_path, ProjectRevision,
                                       std::optional<std::string>)
        -> DocumentOperationResult override
    {
        project.document.relative_path = relative_path;
        return {.snapshot = project,
                .file = ContentFileInfo{.name = "project.xenproj",
                                        .relative_path = relative_path,
                                        .stem = "project",
                                        .file_revision = "sha256:project"}};
    }

    [[nodiscard]] auto restore_recovery(std::string, ProjectRevision, bool)
        -> DocumentOperationResult override
    {
        return {.snapshot = project};
    }

    [[nodiscard]] auto discard_recovery(std::string) -> DocumentOperationResult override
    {
        return {.snapshot = project};
    }

    [[nodiscard]] auto import_cell(std::string, ProjectRevision, CompositionCursor)
        -> DocumentOperationResult override
    {
        return {.snapshot = project, .suggested_selection = SelectionPath{}};
    }

    [[nodiscard]] auto save_cell(std::string relative_path, ProjectRevision,
                                 CompositionCursor, SelectionPath,
                                 std::optional<std::string>)
        -> DocumentOperationResult override
    {
        return {.snapshot = project,
                .file = ContentFileInfo{.name = "cell.xencell",
                                        .relative_path = relative_path,
                                        .stem = "cell",
                                        .file_revision = "sha256:cell"}};
    }

    void set_channel_id(ChannelId channel_id) override
    {
        binding.channel_id = std::move(channel_id);
    }
};

class FakeKeymapService final : public bridge::KeymapBridgeService
{
  public:
    KeymapResource current{
        .revision = HIGH_KEYMAP_REVISION,
        .document = nlohmann::json{{"future", true}},
    };

    auto read() -> KeymapResource override
    {
        return current;
    }

    [[nodiscard]] auto revision() const noexcept -> std::uint64_t override
    {
        return current.revision;
    }

    auto write(std::uint64_t expected_revision, nlohmann::json document)
        -> KeymapResource override
    {
        if (expected_revision != current.revision)
        {
            throw KeymapStorageError{KeymapStorageErrorCode::Conflict,
                                     "stale fake keymap revision"};
        }
        ++current.revision;
        current.document = std::move(document);
        return current;
    }

    auto erase(std::uint64_t expected_revision) -> KeymapResource override
    {
        if (expected_revision != current.revision)
        {
            throw KeymapStorageError{KeymapStorageErrorCode::Conflict,
                                     "stale fake keymap revision"};
        }
        ++current.revision;
        current.document.reset();
        return current;
    }

    auto refresh() -> bool override
    {
        return false;
    }
};

class FakePreferencesService final : public bridge::PreferencesBridgeService
{
  public:
    PreferencesResource current{
        .revision = HIGH_PREFERENCES_REVISION,
        .document = nlohmann::json{{"future", true}},
    };

    auto read() -> PreferencesResource override
    {
        return current;
    }

    [[nodiscard]] auto revision() const noexcept -> std::uint64_t override
    {
        return current.revision;
    }

    auto write(std::uint64_t expected_revision, nlohmann::json document)
        -> PreferencesResource override
    {
        if (expected_revision != current.revision)
        {
            throw PreferencesStorageError{PreferencesStorageErrorCode::Conflict,
                                          "stale fake preferences revision"};
        }
        ++current.revision;
        current.document = std::move(document);
        return current;
    }

    auto erase(std::uint64_t expected_revision) -> PreferencesResource override
    {
        if (expected_revision != current.revision)
        {
            throw PreferencesStorageError{PreferencesStorageErrorCode::Conflict,
                                          "stale fake preferences revision"};
        }
        ++current.revision;
        current.document.reset();
        return current;
    }

    auto refresh() -> bool override
    {
        return false;
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
            {"library_revision", std::to_string(snapshot.library_revision.value())},
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

    [[nodiscard]] auto cell_files(std::filesystem::path const &) const
        -> std::vector<bridge::LibraryFileEntry> override
    {
        return {{
            .name = "cell.xencell",
            .relative_path = "folder/cell.xencell",
            .stem = "folder/cell",
            .file_revision = "sha256:cell",
        }};
    }

    [[nodiscard]] auto project_files(std::filesystem::path const &) const
        -> std::vector<bridge::LibraryFileEntry> override
    {
        return {{
            .name = "project.xenproj",
            .relative_path = "folder/project.xenproj",
            .stem = "folder/project",
            .file_revision = "sha256:project",
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
                    .file_revision = "sha256:tuning",
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
    CHECK(payload.at("catalog").at("schema_version") == bridge::catalog_schema_version);
    CHECK(payload.at("modulation").at("schema_version") == MODULATION_SCHEMA_VERSION);
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
    CHECK(payload.at("keymap").contains("revision"));
    REQUIRE(payload.at("keymap").at("revision").is_string());
    auto const keymap_revision =
        std::stoull(payload.at("keymap").at("revision").get<std::string>());
    CHECK(keymap_revision >
          static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
    CHECK(keymap_revision > 9'007'199'254'740'991ULL);
    CHECK(payload.at("keymap").at("document").is_null());
    CHECK(payload.contains("preferences"));
    REQUIRE(payload.at("preferences").at("revision").is_string());
    CHECK(payload.at("preferences").at("document").is_null());
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
    CHECK(library.contains("cells"));
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
    auto const message = response(
        host_bridge, "command.execute",
        {
            {"command", "set key 7"},
            {"context",
             {{"expected_project_revision", std::to_string(revision)},
              {"cursor",
               {{"row_coordinate", 0}, {"column_coordinate", 0}, {"sequence_id", 1}}}}},
        });

    auto const &payload = message.at("payload");
    CHECK(payload.at("status").at("level") == "info");
    CHECK(payload.contains("suggested_selection"));
    CHECK(payload.at("snapshot").contains("project"));
    CHECK_FALSE(payload.at("snapshot").contains("library"));
}

TEST_CASE("Bridge exposes generic project preview lifecycle", "[core][bridge][preview]")
{
    auto session = make_session();
    auto host_bridge = make_bridge(session);
    auto const initial = session.project_snapshot();
    auto const begin = response(host_bridge, "preview.begin",
                                {{"expected_project_revision",
                                  std::to_string(initial.project_revision.value())}})
                           .at("payload");
    REQUIRE(begin.at("preview_id").is_string());
    auto const preview_id = begin.at("preview_id").get<std::string>();
    CHECK(begin.at("snapshot").at("preview_active") == true);

    auto const updated =
        response(
            host_bridge, "command.execute",
            {{"command", "set key 7"},
             {"context",
              {{"expected_project_revision",
                std::to_string(session.project_snapshot().project_revision.value())},
               {"preview_id", preview_id},
               {"cursor",
                {{"row_coordinate", 0},
                 {"column_coordinate", 0},
                 {"sequence_id", 1}}}}}})
            .at("payload");
    CHECK(updated.at("snapshot").at("preview_active") == true);
    CHECK(session.project_snapshot().history_entry_id == initial.history_entry_id);

    auto const cancelled =
        response(
            host_bridge, "preview.cancel",
            {{"preview_id", preview_id},
             {"expected_project_revision",
              std::to_string(session.project_snapshot().project_revision.value())}})
            .at("payload");
    CHECK(cancelled.at("snapshot").at("preview_active") == false);
    CHECK(session.project_snapshot().project == initial.project);
}

TEST_CASE("Bridge exposes snapshot-free modulation preview updates",
          "[core][bridge][modulation]")
{
    auto application = FakeApplicationService{};
    auto library = FakeLibraryService{};
    auto keymap = FakeKeymapService{};
    auto preferences = FakePreferencesService{};
    auto dispatcher =
        bridge::BridgeRequestDispatcher{application, library, keymap, preferences};

    auto const target = nlohmann::json{
        {"cursor",
         {{"row_coordinate", 0}, {"column_coordinate", 0}, {"sequence_id", 1}}},
        {"selection",
         {{"path", nlohmann::json::array({{{"kind", "element"}, {"index", 0}}})}}},
        {"pattern", {{"offset", 0}, {"intervals", {1}}}},
    };
    auto const begin =
        fake_response(dispatcher, "modulation.preview.begin",
                      {{"expected_project_revision", "7"}, {"target", target}})
            .at("payload");
    CHECK(begin.at("preview_id") == "modulation-preview-test");
    CHECK(begin.at("snapshot").at("preview_active") == true);

    auto const update =
        fake_response(
            dispatcher, "modulation.preview.update",
            {{"preview_id", "modulation-preview-test"},
             {"update_sequence", "1"},
             {"expected_project_revision", "7"},
             {"destination", "velocity"},
             {"output_range", {{"minimum", 0.0}, {"maximum", 1.0}}},
             {"modulation",
              {{"operation", "average"},
               {"waveforms", nlohmann::json::array({{{"enabled", true},
                                                     {"shape", "sine"},
                                                     {"frequency", 1.0},
                                                     {"phase", 0.0},
                                                     {"amplitude", 1.0},
                                                     {"amplitude_offset", 0.0}}})}}}})
            .at("payload");
    CHECK(update.at("accepted") == true);
    CHECK(update.at("accepted_update_sequence") == "1");
    CHECK_FALSE(update.contains("snapshot"));

    auto const cancelled = fake_response(dispatcher, "modulation.preview.cancel",
                                         {{"preview_id", "modulation-preview-test"},
                                          {"expected_project_revision", "8"}})
                               .at("payload");
    CHECK(cancelled.at("snapshot").at("preview_active") == false);
}

TEST_CASE("Bridge writes and deletes opaque keymap documents", "[core][bridge]")
{
    auto session = make_session();
    auto host_bridge = make_bridge(session);
    auto const initial = response(host_bridge, "keymap.read").at("payload");
    auto const revision = initial.at("revision").get<std::string>();
    auto const document = nlohmann::json{{"unknown_context", {{"future", true}}}};
    auto const updated = response(host_bridge, "keymap.write",
                                  {
                                      {"expected_revision", revision},
                                      {"document", document},
                                  })
                             .at("payload");
    CHECK(updated.at("revision") != revision);
    CHECK(updated.at("document") == document);

    auto const stale = response(host_bridge, "keymap.write",
                                {
                                    {"expected_revision", revision},
                                    {"document", {{"replacement", true}}},
                                })
                           .at("payload");
    CHECK(stale.at("error").at("code") == "conflict");

    auto const erased = response(host_bridge, "keymap.delete",
                                 {{"expected_revision", updated.at("revision")}})
                            .at("payload");
    CHECK(erased.at("document").is_null());
    CHECK(erased.at("revision") == revision);
}

TEST_CASE("Bridge writes and deletes opaque preferences objects", "[core][bridge]")
{
    auto session = make_session();
    auto host_bridge = make_bridge(session);
    auto const initial = response(host_bridge, "preferences.read").at("payload");
    auto const revision = initial.at("revision").get<std::string>();
    auto const document = nlohmann::json{
        {"schema_version", 1},
        {"unknown_future_preference", {{"enabled", true}}},
    };
    auto const updated = response(host_bridge, "preferences.write",
                                  {
                                      {"expected_revision", revision},
                                      {"document", document},
                                  })
                             .at("payload");
    CHECK(updated.at("revision") != revision);
    CHECK(updated.at("document") == document);

    auto const stale = response(host_bridge, "preferences.write",
                                {
                                    {"expected_revision", revision},
                                    {"document", {{"replacement", true}}},
                                })
                           .at("payload");
    CHECK(stale.at("error").at("code") == "conflict");

    auto const erased = response(host_bridge, "preferences.delete",
                                 {{"expected_revision", updated.at("revision")}})
                            .at("payload");
    CHECK(erased.at("document").is_null());
    CHECK(erased.at("revision") == revision);
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
    CHECK(keymap.at("payload").contains("document"));

    auto const preferences =
        nlohmann::json::parse(host_bridge.make_preferences_changed_event_json());
    CHECK(preferences.at("name") == "preferences.changed");
    CHECK(preferences.at("payload").contains("revision"));
    CHECK(preferences.at("payload").contains("document"));
}

TEST_CASE("Bridge publishes externally changed opaque keymaps", "[core][bridge]")
{
    auto session = make_session();
    auto const file = temporary_keymap_file();
    auto host_bridge = WebviewBridge{session, file, temporary_preferences_file()};
    auto const initial_revision = host_bridge.keymap_revision();
    auto const document = nlohmann::json{{"external", {{"future_action", 7}}}};
    auto output = std::ofstream{file, std::ios::binary | std::ios::trunc};
    REQUIRE(output.good());
    output << document.dump();
    output.close();

    CHECK(host_bridge.refresh_keymap());
    CHECK(host_bridge.keymap_revision() != initial_revision);
    auto const event =
        nlohmann::json::parse(host_bridge.make_keymap_changed_event_json());
    CHECK(event.at("name") == "keymap.changed");
    CHECK(event.at("payload").at("document") == document);
}

TEST_CASE("Bridge publishes externally changed opaque preferences", "[core][bridge]")
{
    auto session = make_session();
    auto const file = temporary_preferences_file();
    auto host_bridge = WebviewBridge{session, temporary_keymap_file(), file};
    auto const initial_revision = host_bridge.preferences_revision();
    auto const document = nlohmann::json{
        {"schema_version", 1},
        {"reduced_motion", true},
    };
    auto output = std::ofstream{file, std::ios::binary | std::ios::trunc};
    REQUIRE(output.good());
    output << document.dump();
    output.close();

    CHECK(host_bridge.refresh_preferences());
    CHECK(host_bridge.preferences_revision() != initial_revision);
    auto const event =
        nlohmann::json::parse(host_bridge.make_preferences_changed_event_json());
    CHECK(event.at("name") == "preferences.changed");
    CHECK(event.at("payload").at("document") == document);
}

TEST_CASE("Bridge dispatcher handles service requests with fake services",
          "[core][bridge]")
{
    auto application = FakeApplicationService{};
    auto library = FakeLibraryService{};
    auto keymap = FakeKeymapService{};
    auto preferences = FakePreferencesService{};
    auto dispatcher =
        bridge::BridgeRequestDispatcher{application, library, keymap, preferences};

    auto const hello = fake_response(dispatcher, "session.hello",
                                     {
                                         {"protocol", bridge::protocol},
                                         {"frontend_app", "test"},
                                         {"frontend_version", "1"},
                                     });
    CHECK(hello.at("payload").at("catalog").at("schema_version") ==
          bridge::catalog_schema_version);
    CHECK(hello.at("payload").at("keymap").at("revision") ==
          std::to_string(HIGH_KEYMAP_REVISION));
    CHECK(hello.at("payload").at("preferences").at("revision") ==
          std::to_string(HIGH_PREFERENCES_REVISION));
    CHECK(hello.at("payload").at("binding").at("channel_id") == DEFAULT_CHANNEL_ID);

    auto const state = fake_response(dispatcher, "state.get").at("payload");
    CHECK(state.at("project_revision") == "7");
    CHECK(state.at("state_revision") == "9");

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
                               {"expected_project_revision", "7"},
                               {"selection", {{"path", nlohmann::json::array()}}},
                               {"cursor",
                                {
                                    {"row_coordinate", -2},
                                    {"column_coordinate", -7},
                                    {"sequence_id", 2},
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
    CHECK(application.executed_context.cursor.row_coordinate == -2);
    CHECK(application.executed_context.cursor.column_coordinate == -7);
    CHECK(application.executed_context.cursor.sequence_id == 2);

    auto const library_payload = fake_response(dispatcher, "library.get").at("payload");
    CHECK(library_payload.at("fake_library") == true);
    CHECK(library_payload.at("library_revision") == "11");
}

TEST_CASE("Bridge library payload uses file port entries", "[core][bridge]")
{
    auto files = FakeLibraryFilePort{};
    auto service = bridge::JuceLibraryBridgeService{files};
    auto snapshot = LibrarySnapshot{
        .library = ContentLibrary{},
        .workspace =
            WorkspaceSettings{
                .content_directory = "/fake/sequences",
                .tuning_directory = "/fake/tunings",
            },
        .library_revision = LibraryRevision{12},
    };

    auto const payload = service.make_payload(snapshot);
    CHECK(payload.at("paths").at("library") == "/fake/library");
    REQUIRE(payload.at("cells").size() == 1);
    CHECK(payload.at("cells").front().at("command") ==
          "load cell \"folder/cell.xencell\"");
    REQUIRE(payload.at("projects").size() == 1);
    CHECK(payload.at("projects").front().at("command") ==
          "project open \"folder/project.xenproj\"");
    REQUIRE(payload.at("tunings").size() == 1);
    CHECK(payload.at("tunings").front().at("description") == "fake tuning");
}

TEST_CASE("Bridge exposes structured project and cell document operations",
          "[core][bridge][document]")
{
    auto application = FakeApplicationService{};
    auto library = FakeLibraryService{};
    auto keymap = FakeKeymapService{};
    auto preferences = FakePreferencesService{};
    auto dispatcher =
        bridge::BridgeRequestDispatcher{application, library, keymap, preferences};

    auto const saved = fake_response(dispatcher, "project.save_as",
                                     {{"relative_path", "folder/project.xenproj"},
                                      {"expected_project_revision", "7"},
                                      {"expected_file_revision", nullptr}})
                           .at("payload");
    CHECK(saved.at("file").at("relative_path") == "folder/project.xenproj");
    CHECK(saved.at("file").at("file_revision") == "sha256:project");
    CHECK(saved.at("snapshot").at("document").at("display_name") == "project");

    auto const cell =
        fake_response(
            dispatcher, "cell.save",
            {{"relative_path", "folder/cell.xencell"},
             {"expected_project_revision", "7"},
             {"expected_file_revision", nullptr},
             {"cursor",
              {{"row_coordinate", 0}, {"column_coordinate", 0}, {"sequence_id", 1}}},
             {"selection", {{"path", nlohmann::json::array()}}}})
            .at("payload");
    CHECK(cell.at("file").at("file_revision") == "sha256:cell");
}

TEST_CASE("Bridge dispatcher handles keymap requests with fake services",
          "[core][bridge]")
{
    auto application = FakeApplicationService{};
    auto library = FakeLibraryService{};
    auto keymap = FakeKeymapService{};
    auto preferences = FakePreferencesService{};
    auto dispatcher =
        bridge::BridgeRequestDispatcher{application, library, keymap, preferences};
    auto const set =
        fake_response(dispatcher, "keymap.write",
                      {
                          {"expected_revision", std::to_string(HIGH_KEYMAP_REVISION)},
                          {"document", nlohmann::json::array({1, "future", true})},
                      })
            .at("payload");
    CHECK(set.at("revision") == std::to_string(HIGH_KEYMAP_REVISION + 1));
    CHECK(set.at("document").is_array());

    auto const remove =
        fake_response(dispatcher, "keymap.delete",
                      {{"expected_revision", std::to_string(HIGH_KEYMAP_REVISION + 1)}})
            .at("payload");
    CHECK(remove.at("revision") == std::to_string(HIGH_KEYMAP_REVISION + 2));
    CHECK(remove.at("document").is_null());

    auto const stale =
        fake_response(dispatcher, "keymap.delete",
                      {{"expected_revision", std::to_string(HIGH_KEYMAP_REVISION + 1)}})
            .at("payload");
    CHECK(stale.at("error").at("code") == "conflict");
}

TEST_CASE("Bridge dispatcher handles preferences requests with fake services",
          "[core][bridge]")
{
    auto application = FakeApplicationService{};
    auto library = FakeLibraryService{};
    auto keymap = FakeKeymapService{};
    auto preferences = FakePreferencesService{};
    auto dispatcher =
        bridge::BridgeRequestDispatcher{application, library, keymap, preferences};
    auto const set = fake_response(dispatcher, "preferences.write",
                                   {
                                       {"expected_revision",
                                        std::to_string(HIGH_PREFERENCES_REVISION)},
                                       {"document", {{"future", true}}},
                                   })
                         .at("payload");
    CHECK(set.at("revision") == std::to_string(HIGH_PREFERENCES_REVISION + 1));
    CHECK(set.at("document").is_object());

    auto const remove = fake_response(dispatcher, "preferences.delete",
                                      {{"expected_revision",
                                        std::to_string(HIGH_PREFERENCES_REVISION + 1)}})
                            .at("payload");
    CHECK(remove.at("revision") == std::to_string(HIGH_PREFERENCES_REVISION + 2));
    CHECK(remove.at("document").is_null());

    auto const stale = fake_response(dispatcher, "preferences.delete",
                                     {{"expected_revision",
                                       std::to_string(HIGH_PREFERENCES_REVISION + 1)}})
                           .at("payload");
    CHECK(stale.at("error").at("code") == "conflict");
}

TEST_CASE("Bridge protocol errors are deterministic", "[core][bridge]")
{
    auto application = FakeApplicationService{};
    auto library = FakeLibraryService{};
    auto keymap = FakeKeymapService{};
    auto preferences = FakePreferencesService{};
    auto dispatcher =
        bridge::BridgeRequestDispatcher{application, library, keymap, preferences};

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

    auto const missing_document =
        fake_response(dispatcher, "keymap.write",
                      {{"expected_revision", std::to_string(HIGH_KEYMAP_REVISION)}})
            .at("payload");
    CHECK(missing_document.at("error").at("code") == "invalid_request");

    auto const invalid_preferences_document =
        fake_response(
            dispatcher, "preferences.write",
            {
                {"expected_revision", std::to_string(HIGH_PREFERENCES_REVISION)},
                {"document", nlohmann::json::array()},
            })
            .at("payload");
    CHECK(invalid_preferences_document.at("error").at("code") == "invalid_request");

    auto const numeric_revision =
        fake_response(dispatcher, "keymap.delete", {{"expected_revision", 5}})
            .at("payload");
    CHECK(numeric_revision.at("error").at("code") == "invalid_request");

    auto const overflowing_revision =
        fake_response(dispatcher, "keymap.delete",
                      {{"expected_revision", "18446744073709551616"}})
            .at("payload");
    CHECK(overflowing_revision.at("error").at("code") == "invalid_request");

    auto const coordinate = nlohmann::json{{"value", -12}};
    CHECK(bridge::require_composition_coordinate(coordinate, "value") == -12);
    auto const overflowing_coordinate = nlohmann::json{
        {"value", std::int64_t{std::numeric_limits<CompositionCoordinate>::max()} + 1}};
    CHECK_THROWS_AS(
        bridge::require_composition_coordinate(overflowing_coordinate, "value"),
        bridge::BridgeError);
}

TEST_CASE("Bridge reports malformed persisted keymaps", "[core][bridge]")
{
    auto session = make_session();
    auto const file = temporary_keymap_file();
    auto output = std::ofstream{file, std::ios::binary | std::ios::trunc};
    REQUIRE(output.good());
    output << "{not-json";
    output.close();
    auto host_bridge = WebviewBridge{session, file, temporary_preferences_file()};

    auto const malformed = response(host_bridge, "keymap.read").at("payload");
    CHECK(malformed.at("error").at("code") == "malformed_document");
}

TEST_CASE("Bridge reports malformed persisted preferences", "[core][bridge]")
{
    auto session = make_session();
    auto const file = temporary_preferences_file();
    auto output = std::ofstream{file, std::ios::binary | std::ios::trunc};
    REQUIRE(output.good());
    output << "[]";
    output.close();
    auto host_bridge = WebviewBridge{session, temporary_keymap_file(), file};

    auto const malformed = response(host_bridge, "preferences.read").at("payload");
    CHECK(malformed.at("error").at("code") == "malformed_document");
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
