#include <xen/session_coordinator.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <xen/composition.hpp>

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
    auto binding = assign_binding(std::move(hello.binding));
    auto const instance_id = binding.instance_id;
    bindings_[instance_id] = binding;
    return {
        .binding = bindings_.at(instance_id),
        .snapshot = session_.project_snapshot(),
        .library = session_.library_snapshot(),
        .instances = instances(),
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

auto SessionCoordinator::set_binding(BindingSetRequest request) -> BindingSetResponse
{
    auto const found = bindings_.find(request.instance_id);
    if (found == bindings_.end())
    {
        throw std::invalid_argument{"Unknown binding instance ID."};
    }
    if (request.output_id.empty())
    {
        throw std::invalid_argument{"Instance output ID must not be empty."};
    }

    auto const old_output_id = found->second.output_id;
    found->second.output_id = std::move(request.output_id);
    auto project = session_.project_snapshot().project;
    if (auto const row = std::ranges::find(project.composition.rows, old_output_id,
                                           &CompositionRow::output_id);
        row != project.composition.rows.end())
    {
        row->output_id = found->second.output_id;
        session_.replace_project_history_and_binding(std::move(project),
                                                     session_.instance_binding());
    }
    else
    {
        ensure_output_row(found->second.output_id);
    }

    if (session_.instance_binding().instance_id == found->second.instance_id)
    {
        session_.replace_instance_binding(found->second);
    }
    else
    {
        project = session_.project_snapshot().project;
        session_.replace_project_history(std::move(project));
    }

    return {
        .request_id = std::move(request.request_id),
        .binding = found->second,
        .snapshot = session_.project_snapshot(),
    };
}

auto SessionCoordinator::snapshot() const -> ProjectSnapshot
{
    return session_.project_snapshot();
}

auto SessionCoordinator::library_snapshot() const -> LibrarySnapshot
{
    return session_.library_snapshot();
}

auto SessionCoordinator::instances() const -> std::vector<InstanceBinding>
{
    auto result = std::vector<InstanceBinding>{};
    result.reserve(bindings_.size());
    for (auto const &[_, binding] : bindings_)
    {
        result.push_back(binding);
    }
    return result;
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

auto SessionCoordinator::assign_binding(InstanceBinding binding) -> InstanceBinding
{
    if (binding.output_id == CURRENT_INSTANCE_OUTPUT_ID ||
        std::ranges::any_of(bindings_, [&](auto const &entry) {
            return entry.first != binding.instance_id &&
                   entry.second.output_id == binding.output_id;
        }))
    {
        do
        {
            binding.output_id = "track-" + std::to_string(next_output_index_++);
        } while (std::ranges::any_of(bindings_, [&](auto const &entry) {
            return entry.first != binding.instance_id &&
                   entry.second.output_id == binding.output_id;
        }));
    }

    session_.replace_instance_binding(binding);
    ensure_output_row(binding.output_id);
    return binding;
}

void SessionCoordinator::ensure_output_row(OutputId const &output_id)
{
    auto project = session_.project_snapshot().project;
    auto &rows = project.composition.rows;
    if (rows.size() == 1 && rows.front().output_id == CURRENT_INSTANCE_OUTPUT_ID)
    {
        rows.front().output_id = output_id;
        session_.replace_project_history_and_binding(std::move(project),
                                                     session_.instance_binding());
        return;
    }

    auto const found = std::ranges::find(rows, output_id, &CompositionRow::output_id);
    if (found == rows.end())
    {
        insert_row(project.composition, rows.size(), output_id);
        session_.replace_project_history_and_binding(std::move(project),
                                                     session_.instance_binding());
    }
}

} // namespace xen::ipc
