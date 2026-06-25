#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <utility>

#include <juce_core/juce_core.h>

#include <xen/coordinator_registry.hpp>
#include <xen/ipc_protocol.hpp>
#include <xen/message_level.hpp>
#include <xen/session_coordinator.hpp>

using namespace xen;

namespace
{

auto binding(std::string instance_id,
             std::string output_id = CURRENT_INSTANCE_OUTPUT_ID) -> InstanceBinding
{
    return {
        .session_id = "session",
        .instance_id = std::move(instance_id),
        .output_id = std::move(output_id),
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
            },
    };

    auto const decoded_request =
        ipc::decode_command_request(ipc::encode_command_request(request));
    CHECK(decoded_request.request_id == request.request_id);
    CHECK(decoded_request.source_instance_id == request.source_instance_id);
    CHECK(decoded_request.command == request.command);
    REQUIRE(decoded_request.context.expected_project_revision.has_value());
    CHECK(decoded_request.context.expected_project_revision->value() == 12);
    REQUIRE(decoded_request.context.selection.has_value());
    CHECK(decoded_request.context.selection->path.empty());

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
             .binding = binding("instance-a", "track-1"),
             .snapshot = ProjectSnapshot{.project = ProjectState{},
                                         .history_entry_id = HistoryEntryId{9},
                                         .project_revision = ProjectRevision{10}}}));
    CHECK(binding_response.request_id == "binding-1");
    CHECK(binding_response.binding.output_id == "track-1");

    auto const instances = ipc::decode_instances_changed(ipc::encode_instances_changed(
        {.instances = {binding("instance-a", "track-1"),
                       binding("instance-b", "track-2")}}));
    REQUIRE(instances.instances.size() == 2);
    CHECK(instances.instances[1].output_id == "track-2");

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

TEST_CASE("SessionCoordinator broadcasts authoritative command results",
          "[sync][ipc][coordinator]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto const hello_a = coordinator.connect({.binding = binding("instance-a")});
    auto const hello_b =
        coordinator.connect({.binding = binding("instance-b", "peer")});

    CHECK(hello_a.binding.output_id == "track-1");
    CHECK(hello_b.binding.output_id == "peer");
    CHECK(hello_a.snapshot.project.composition.rows.front().output_id == "track-1");
    CHECK(hello_b.snapshot.project.composition.rows.front().output_id == "track-1");
    REQUIRE(coordinator.binding_for("instance-b") != nullptr);
    CHECK(coordinator.binding_for("instance-b")->output_id == "peer");
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
    CHECK(result.snapshot.project.pitch.transposition == 5);
    CHECK(coordinator.snapshot().project == result.snapshot.project);
    CHECK(coordinator.live_edit_started());
}

TEST_CASE("SessionCoordinator binding changes republish shared output rows",
          "[sync][ipc][coordinator]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto const hello = coordinator.connect({.binding = binding("instance-a")});

    auto const response = coordinator.set_binding({
        .request_id = "binding-1",
        .instance_id = hello.binding.instance_id,
        .output_id = "lead",
    });

    CHECK(response.request_id == "binding-1");
    CHECK(response.binding.output_id == "lead");
    CHECK(coordinator.binding_for("instance-a")->output_id == "lead");
    CHECK(response.snapshot.project.composition.rows.front().output_id == "lead");
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
    newer.project.pitch.transposition = 8;
    newer.saved_project_revision = ProjectRevision{2};

    (void)coordinator.connect({.binding = older.binding, .restore_state = older});
    auto const hello =
        coordinator.connect({.binding = newer.binding, .restore_state = newer});

    CHECK(hello.snapshot.project.pitch.transposition == 8);
    CHECK(coordinator.snapshot().project.pitch.transposition == 8);
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
    conflicting.project.pitch.transposition = 9;

    (void)coordinator.connect({.binding = first.binding, .restore_state = first});
    CHECK_THROWS_AS(coordinator.connect(
                        {.binding = conflicting.binding, .restore_state = conflicting}),
                    std::runtime_error);
}
