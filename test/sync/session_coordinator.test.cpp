#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>
#include <utility>

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

TEST_CASE("SessionCoordinator broadcasts authoritative command results",
          "[sync][ipc][coordinator]")
{
    auto coordinator = ipc::SessionCoordinator{};
    auto const hello_a = coordinator.connect({.binding = binding("instance-a")});
    auto const hello_b =
        coordinator.connect({.binding = binding("instance-b", "peer")});

    CHECK(hello_a.snapshot.project == hello_b.snapshot.project);
    REQUIRE(coordinator.binding_for("instance-b") != nullptr);
    CHECK(coordinator.binding_for("instance-b")->output_id == "peer");

    auto const result = coordinator.execute({
        .request_id = "request-1",
        .source_instance_id = "instance-a",
        .command = "set key 5",
        .context =
            CommandContext{
                .expected_project_revision = hello_a.snapshot.project_revision,
            },
    });

    CHECK(result.request_id == "request-1");
    CHECK(result.result.status.first == MessageLevel::Info);
    CHECK(result.snapshot.project.pitch.transposition == 5);
    CHECK(coordinator.snapshot().project == result.snapshot.project);
    CHECK(coordinator.live_edit_started());
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
