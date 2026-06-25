#include <xen/session_coordinator.hpp>

#include <stdexcept>
#include <utility>

namespace xen::ipc
{

SessionCoordinator::SessionCoordinator(SubmissionEffects::FailurePoint effect_failure,
                                       std::filesystem::path workspace_settings_file)
    : session_{effect_failure, std::move(workspace_settings_file)}
{
}

auto SessionCoordinator::connect(ClientHello hello) -> CoordinatorHello
{
    if (hello.binding.session_id.empty())
    {
        throw std::invalid_argument{"Session ID must not be empty."};
    }
    if (hello.binding.instance_id.empty())
    {
        throw std::invalid_argument{"Instance ID must not be empty."};
    }
    if (hello.binding.output_id.empty())
    {
        throw std::invalid_argument{"Output ID must not be empty."};
    }

    maybe_seed_from(hello);
    auto binding = std::move(hello.binding);
    auto const instance_id = binding.instance_id;
    bindings_[instance_id] = binding;
    return {
        .binding = bindings_.at(instance_id),
        .snapshot = session_.project_snapshot(),
    };
}

auto SessionCoordinator::execute(CommandRequest request) -> CommandResponse
{
    if (bindings_.find(request.source_instance_id) == bindings_.end())
    {
        throw std::invalid_argument{"Unknown source instance ID."};
    }
    live_edit_started_ = true;
    auto result = session_.execute_command_string(request.command, request.context);
    return {
        .request_id = std::move(request.request_id),
        .result = std::move(result),
        .snapshot = session_.project_snapshot(),
    };
}

auto SessionCoordinator::snapshot() const -> ProjectSnapshot
{
    return session_.project_snapshot();
}

auto SessionCoordinator::binding_for(InstanceId const &instance_id) const
    -> InstanceBinding const *
{
    auto const it = bindings_.find(instance_id);
    return it == bindings_.end() ? nullptr : &it->second;
}

auto SessionCoordinator::live_edit_started() const noexcept -> bool
{
    return live_edit_started_;
}

void SessionCoordinator::maybe_seed_from(ClientHello const &hello)
{
    if (!hello.restore_state.has_value())
    {
        return;
    }
    if (live_edit_started_)
    {
        return;
    }

    auto const revision = hello.restore_state->saved_project_revision;
    if (revision > seed_revision_)
    {
        session_.replace_project_history(hello.restore_state->project);
        seed_revision_ = revision;
        return;
    }

    if (revision == seed_revision_ &&
        hello.restore_state->project != session_.project_snapshot().project)
    {
        throw std::runtime_error{
            "Conflicting restore snapshots share the same revision."};
    }
}

} // namespace xen::ipc
