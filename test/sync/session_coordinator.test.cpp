#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <juce_core/juce_core.h>

#include <xen/coordinator_registry.hpp>
#include <xen/ipc_protocol.hpp>
#include <xen/message_level.hpp>
#include <xen/session_coordinator.hpp>

using namespace xen;

namespace
{

auto binding(std::string instance_id, std::string channel_id = {}) -> InstanceBinding
{
    return {
        .session_id = "session",
        .instance_id = std::move(instance_id),
        .channel_id = std::move(channel_id),
    };
}

} // namespace

TEST_CASE("IPC protocol round-trips command requests and responses", "[sync][ipc]")
{
    auto request = ipc::CommandRequest{
        .request_id = "request-1",
        .source_instance_id = "instance-a",
        .command = "set key 5",
        .context =
            CommandContext{
                .selection = SelectionPath{},
                .expected_project_revision = ProjectRevision{12},
                .preview_id = "preview-1",
                .cursor =
                    xen::CompositionCursor{
                        .row_index = 0,
                        .column_index = 1,
                        .sequence_id = 2,
                    },
            },
    };

    auto const decoded_request =
        ipc::decode_command_request(ipc::encode_command_request(request));
    CHECK(decoded_request.request_id == request.request_id);
    CHECK(decoded_request.source_instance_id == request.source_instance_id);
    CHECK(decoded_request.command == request.command);
    REQUIRE(decoded_request.context.expected_project_revision.has_value());
    CHECK(decoded_request.context.expected_project_revision->value() == 12);
    CHECK(decoded_request.context.preview_id == "preview-1");
    REQUIRE(decoded_request.context.selection.has_value());
    CHECK(decoded_request.context.selection->path.empty());
    CHECK(decoded_request.context.cursor.row_index == 0);
    CHECK(decoded_request.context.cursor.column_index == 1);
    CHECK(decoded_request.context.cursor.sequence_id == 2);

    auto response = ipc::CommandResponse{
        .request_id = "request-1",
        .result =
            CommandApplicationResult{
                .status = {MessageLevel::Info, "ok"},
                .suggested_selection = SelectionPath{},
            },
        .snapshot =
            ProjectSnapshot{
                .project = ProjectState{},
                .history_entry_id = HistoryEntryId{3},
                .project_revision = ProjectRevision{4},
            },
    };

    auto const decoded_response =
        ipc::decode_command_response(ipc::encode_command_response(response));
    CHECK(decoded_response.request_id == response.request_id);
    CHECK(decoded_response.result.status.first == MessageLevel::Info);
    CHECK(decoded_response.result.status.second == "ok");
    REQUIRE(decoded_response.result.suggested_selection.has_value());
    CHECK(decoded_response.snapshot.project == response.snapshot.project);
    CHECK(decoded_response.snapshot.history_entry_id.value() == 3);
    CHECK(decoded_response.snapshot.project_revision.value() == 4);
}

TEST_CASE("IPC protocol encodes absent command context fields as null", "[sync][ipc]")
{
    auto const encoded_request = ipc::encode_command_request({
        .request_id = "request-1",
        .source_instance_id = "instance-a",
        .command = "note 0",
        .context = CommandContext{},
    });

    auto const &context = encoded_request.at("payload").at("context");

    CHECK(context.at("expected_project_revision").is_null());
    CHECK(context.at("selection").is_null());
    CHECK(context.at("preview_id").is_null());
    CHECK(context.at("cursor").is_object());
    CHECK(context.at("cursor").at("row_index") == 0);
    CHECK(context.at("cursor").at("column_index") == 0);
}

TEST_CASE("IPC protocol rejects malformed composition cursors", "[sync][ipc]")
{
    auto message = ipc::encode_command_request({
        .request_id = "request-1",
        .source_instance_id = "instance-a",
        .command = "note 0",
        .context = CommandContext{},
    });
    message["payload"]["context"]["cursor"] = nlohmann::json::array({nullptr});

    try
    {
        (void)ipc::decode_command_request(message);
        FAIL("Expected malformed active measure target to throw.");
    }
    catch (std::invalid_argument const &error)
    {
        CHECK(std::string{error.what()} == "Field must be an object: context.cursor.");
    }
}

TEST_CASE("IPC protocol round-trips coordinator broadcasts and errors", "[sync][ipc]")
{
    auto const project_changed =
        ipc::decode_project_changed(ipc::encode_project_changed(
            {.snapshot = ProjectSnapshot{.project = ProjectState{},
                                         .history_entry_id = HistoryEntryId{7},
                                         .project_revision = ProjectRevision{8}}}));
    CHECK(project_changed.snapshot.project_revision.value() == 8);

    auto const binding_response =
        ipc::decode_binding_set_response(ipc::encode_binding_set_response(
            {.request_id = "binding-1",
             .binding = binding("instance-a", "channel-1"),
             .snapshot = ProjectSnapshot{.project = ProjectState{},
                                         .history_entry_id = HistoryEntryId{9},
                                         .project_revision = ProjectRevision{10}}}));
    CHECK(binding_response.request_id == "binding-1");
    CHECK(binding_response.binding.channel_id == "channel-1");

    auto const instances = ipc::decode_instances_changed(ipc::encode_instances_changed(
        {.instances = {binding("instance-a", "channel-1"),
                       binding("instance-b", "channel-2")}}));
    REQUIRE(instances.instances.size() == 2);
    CHECK(instances.instances[1].channel_id == "channel-2");

    auto const heartbeat =
        ipc::decode_heartbeat(ipc::encode_heartbeat({.sequence = 42}));
    CHECK(heartbeat.sequence == 42);

    auto const shutdown_request = ipc::decode_shutdown_if_idle_request(
        ipc::encode_shutdown_if_idle_request({.request_id = "shutdown-1"}));
    CHECK(shutdown_request.request_id == "shutdown-1");

    auto const shutdown_response =
        ipc::decode_shutdown_if_idle_response(ipc::encode_shutdown_if_idle_response(
            {.request_id = "shutdown-1", .will_exit = true}));
    CHECK(shutdown_response.request_id == "shutdown-1");
    CHECK(shutdown_response.will_exit);

    auto const error = ipc::decode_error(ipc::encode_error(
        {.request_id = "request-1", .code = "bad", .message = "failed"}));
    CHECK(error.request_id == "request-1");
    CHECK(error.code == "bad");
    CHECK(error.message == "failed");
}

TEST_CASE("CoordinatorRegistry round-trips valid entries and clears invalid files",
          "[sync][ipc][registry]")
{
    auto const file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("xen-registry-test", ".json");
    auto registry = ipc::CoordinatorRegistry{file.getFullPathName().toStdString()};
    auto const entry = ipc::CoordinatorRegistryEntry{
        .session_id = "session",
        .port = 49001,
        .pid = 123,
        .nonce = "nonce",
    };

    registry.write(entry);
    auto const restored = registry.read();
    REQUIRE(restored.has_value());
    CHECK(restored->session_id == entry.session_id);
    CHECK(restored->port == entry.port);
    CHECK(restored->nonce == entry.nonce);

    file.replaceWithText("{not json");
    CHECK_FALSE(registry.read().has_value());
    CHECK_FALSE(file.existsAsFile());
}

TEST_CASE("CoordinatorRegistry never exposes partial publications",
          "[sync][ipc][registry]")
{
    auto const file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getNonexistentChildFile("xen-registry-race-test", ".json");
    auto registry = ipc::CoordinatorRegistry{file.getFullPathName().toStdString()};
    auto const nonce_prefix = std::string(256 * 1024, 'x');
    auto writer_done = std::atomic<bool>{false};
    auto invalid_publication_seen = std::atomic<bool>{false};

    registry.write({
        .session_id = "session",
        .port = 49'001,
        .pid = 123,
        .nonce = nonce_prefix + "initial",
    });

    auto writer = std::thread{[&] {
        for (auto publication = 0; publication < 25; ++publication)
        {
            registry.write({
                .session_id = "session",
                .port = 49'001,
                .pid = 123,
                .nonce = nonce_prefix + std::to_string(publication),
            });
        }
        writer_done.store(true, std::memory_order_release);
    }};

    while (!writer_done.load(std::memory_order_acquire))
    {
        auto const published = registry.read();
        if (!published.has_value() || published->nonce.size() < nonce_prefix.size() ||
            !published->nonce.starts_with(nonce_prefix))
        {
            invalid_publication_seen.store(true, std::memory_order_relaxed);
        }
    }

    writer.join();
    CHECK_FALSE(invalid_publication_seen.load(std::memory_order_relaxed));
    REQUIRE(registry.read().has_value());
    registry.clear();
}

TEST_CASE("SessionCoordinator broadcasts authoritative command results",
          "[sync][ipc][coordinator]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto const hello_a = coordinator.connect({.binding = binding("instance-a")});
    auto const hello_b = coordinator.connect({.binding = binding("instance-b")});

    CHECK(hello_a.binding.channel_id == "channel-1");
    CHECK(hello_b.binding.channel_id == "channel-2");
    CHECK(hello_a.snapshot.project.composition.rows.front().channel_id == "channel-1");
    CHECK(hello_b.snapshot.project.composition.rows[1].channel_id == "channel-2");
    CHECK(hello_b.snapshot.project.composition.rows[1].cells.front().has_value());
    REQUIRE(coordinator.binding_for("instance-b") != nullptr);
    CHECK(coordinator.binding_for("instance-b")->channel_id == "channel-2");
    CHECK(coordinator.snapshot().project.composition.rows.size() == 2);

    auto const result = coordinator.execute({
        .request_id = "request-1",
        .source_instance_id = "instance-a",
        .command = "set key 5",
        .context =
            CommandContext{
                .expected_project_revision = coordinator.snapshot().project_revision,
            },
    });

    CHECK(result.request_id == "request-1");
    CHECK(result.result.status.first == MessageLevel::Info);
    CHECK(result.snapshot.project.composition.columns.front().pitch.transposition == 5);
    CHECK(coordinator.snapshot().project == result.snapshot.project);
    CHECK(coordinator.live_edit_started());
}

TEST_CASE("SessionCoordinator owns and cancels shared previews",
          "[sync][ipc][coordinator][preview]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto const hello_a = coordinator.connect({.binding = binding("instance-a")});
    auto const hello_b = coordinator.connect({.binding = binding("instance-b")});
    auto const baseline = coordinator.snapshot();

    auto const started = coordinator.begin_preview({
        .request_id = "preview-1",
        .source_instance_id = hello_a.binding.instance_id,
        .expected_project_revision = baseline.project_revision,
    });
    REQUIRE(started.result.preview_id.has_value());
    CHECK(started.snapshot.preview_active);

    auto const foreign = coordinator.execute({
        .request_id = "request-foreign",
        .source_instance_id = hello_b.binding.instance_id,
        .command = "set key 3",
        .context =
            CommandContext{
                .expected_project_revision = coordinator.snapshot().project_revision,
                .preview_id = started.result.preview_id,
            },
    });
    CHECK(foreign.result.status.first == MessageLevel::Error);

    auto const updated = coordinator.execute({
        .request_id = "request-owner",
        .source_instance_id = hello_a.binding.instance_id,
        .command = "set key 6",
        .context =
            CommandContext{
                .expected_project_revision = coordinator.snapshot().project_revision,
                .preview_id = started.result.preview_id,
            },
    });
    REQUIRE(updated.result.status.first == MessageLevel::Info);
    CHECK(updated.snapshot.history_entry_id == baseline.history_entry_id);
    CHECK(updated.snapshot.project.composition.columns.front().pitch.transposition ==
          6);

    auto const restored = coordinator.disconnect(hello_a.binding.instance_id);
    REQUIRE(restored.has_value());
    CHECK_FALSE(restored->preview_active);
    CHECK(restored->project == baseline.project);
    CHECK_FALSE(coordinator.disconnect(hello_a.binding.instance_id).has_value());
}

TEST_CASE("IPC protocol round-trips preview lifecycle messages", "[sync][ipc][preview]")
{
    auto const begin =
        ipc::decode_preview_begin_request(ipc::encode_preview_begin_request({
            .request_id = "preview-1",
            .source_instance_id = "instance-a",
            .expected_project_revision = ProjectRevision{12},
        }));
    CHECK(begin.request_id == "preview-1");
    CHECK(begin.expected_project_revision.value() == 12);

    auto const end = ipc::PreviewEndRequest{
        .request_id = "preview-2",
        .source_instance_id = "instance-a",
        .preview_id = "token",
        .expected_project_revision = ProjectRevision{13},
    };
    auto const commit =
        ipc::decode_preview_commit_request(ipc::encode_preview_commit_request(end));
    CHECK(commit.preview_id == "token");
    auto const cancel =
        ipc::decode_preview_cancel_request(ipc::encode_preview_cancel_request(end));
    CHECK(cancel.expected_project_revision.value() == 13);

    auto const response = ipc::decode_preview_response(ipc::encode_preview_response({
        .request_id = "preview-1",
        .result =
            PreviewControlResult{
                .status = {MessageLevel::Info, "started"},
                .preview_id = "token",
            },
        .snapshot =
            ProjectSnapshot{
                .project = ProjectState{},
                .history_entry_id = HistoryEntryId{2},
                .project_revision = ProjectRevision{3},
                .preview_active = true,
            },
    }));
    CHECK(response.result.preview_id == "token");
    CHECK(response.snapshot.preview_active);
}

TEST_CASE("SessionCoordinator accepts inactive composition row channels",
          "[sync][ipc][coordinator]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto const hello_a = coordinator.connect({.binding = binding("instance-a")});

    auto const valid = coordinator.execute({
        .request_id = "request-1",
        .source_instance_id = hello_a.binding.instance_id,
        .command = "composition row channel 0 inactive",
        .context =
            CommandContext{
                .expected_project_revision = coordinator.snapshot().project_revision,
            },
    });

    CHECK(valid.result.status.first == MessageLevel::Info);
    CHECK(valid.snapshot.project.composition.rows.front().channel_id == "inactive");

    auto const invalid = coordinator.execute({
        .request_id = "request-2",
        .source_instance_id = hello_a.binding.instance_id,
        .command = "composition row channel 0 \"\"",
        .context =
            CommandContext{
                .expected_project_revision = coordinator.snapshot().project_revision,
            },
    });

    CHECK(invalid.result.status.first == MessageLevel::Error);
    CHECK(invalid.result.status.second == "Channel ID must not be empty.");
}

TEST_CASE("SessionCoordinator binding changes do not rewrite channel rows",
          "[sync][ipc][coordinator]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto const hello = coordinator.connect({.binding = binding("instance-a")});

    auto const response = coordinator.set_binding({
        .request_id = "binding-1",
        .instance_id = hello.binding.instance_id,
        .channel_id = "lead",
    });

    CHECK(response.request_id == "binding-1");
    CHECK(response.binding.channel_id == "lead");
    CHECK(coordinator.binding_for("instance-a")->channel_id == "lead");
    CHECK(response.snapshot.project.composition.rows.front().channel_id == "channel-1");
}

TEST_CASE("SessionCoordinator allows duplicate listener channels",
          "[sync][ipc][coordinator]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto const hello_a =
        coordinator.connect({.binding = binding("instance-a", "lead")});
    auto const hello_b =
        coordinator.connect({.binding = binding("instance-b", "lead")});

    CHECK(hello_a.binding.channel_id == "lead");
    CHECK(hello_b.binding.channel_id == "lead");
    REQUIRE(coordinator.binding_for("instance-a") != nullptr);
    REQUIRE(coordinator.binding_for("instance-b") != nullptr);
    CHECK(coordinator.binding_for("instance-a")->channel_id == "lead");
    CHECK(coordinator.binding_for("instance-b")->channel_id == "lead");
    CHECK(coordinator.snapshot().project.composition.rows.front().channel_id ==
          DEFAULT_CHANNEL_ID);
}

TEST_CASE("SessionCoordinator seeds from the newest restore snapshot before edits",
          "[sync][ipc][coordinator]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto older = PersistedProcessorState{
        .binding = binding("instance-a"),
        .project = ProjectState{},
        .saved_history_entry_id = HistoryEntryId{1},
        .saved_project_revision = ProjectRevision{1},
    };
    auto newer = older;
    newer.binding = binding("instance-b");
    newer.project.composition.columns.front().pitch.transposition = 8;
    newer.saved_project_revision = ProjectRevision{2};

    (void)coordinator.connect({.binding = older.binding, .restore_state = older});
    auto const hello =
        coordinator.connect({.binding = newer.binding, .restore_state = newer});

    CHECK(hello.snapshot.project.composition.columns.front().pitch.transposition == 8);
    CHECK(coordinator.snapshot()
              .project.composition.columns.front()
              .pitch.transposition == 8);
}

TEST_CASE("SessionCoordinator rejects equal-revision restore conflicts",
          "[sync][ipc][coordinator]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto first = PersistedProcessorState{
        .binding = binding("instance-a"),
        .project = ProjectState{},
        .saved_history_entry_id = HistoryEntryId{1},
        .saved_project_revision = ProjectRevision{2},
    };
    auto conflicting = first;
    conflicting.binding = binding("instance-b");
    conflicting.project.composition.columns.front().pitch.transposition = 9;

    (void)coordinator.connect({.binding = first.binding, .restore_state = first});
    CHECK_THROWS_AS(coordinator.connect(
                        {.binding = conflicting.binding, .restore_state = conflicting}),
                    std::runtime_error);
}
