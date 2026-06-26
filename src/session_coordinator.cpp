#include <xen/session_coordinator.hpp>

#include <algorithm>
#include <set>
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
    if (request.channel_id.empty())
    {
        throw std::invalid_argument{"Instance channel ID must not be empty."};
    }

    found->second.channel_id = std::move(request.channel_id);

    if (session_.instance_binding().instance_id == found->second.instance_id)
    {
        session_.replace_instance_binding(found->second);
    }
    else
    {
        auto project = session_.project_snapshot().project;
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
    auto auto_assigned = false;
    if (binding.channel_id.empty())
    {
        auto active_channels = std::set<ChannelId>{};
        for (auto const &[_, active_binding] : bindings_)
        {
            active_channels.insert(active_binding.channel_id);
        }

        do
        {
            binding.channel_id = "channel-" + std::to_string(next_channel_index_++);
        } while (active_channels.contains(binding.channel_id));
        auto_assigned = true;
    }

    session_.replace_instance_binding(binding);
    if (auto_assigned)
    {
        ensure_channel_row(binding.channel_id);
    }
    return binding;
}

void SessionCoordinator::ensure_channel_row(ChannelId const &channel_id)
{
    auto project = session_.project_snapshot().project;
    auto &rows = project.composition.rows;
    auto const found = std::ranges::find(rows, channel_id, &CompositionRow::channel_id);
    if (found == rows.end())
    {
        auto const row_index = rows.size();
        auto const measure_id = create_measure(project.measure_bank, Measure{});
        insert_row(project.composition, rows.size(), channel_id);
        assign_measure_reference(project.composition, row_index, 0, measure_id);
        session_.replace_project_history_and_binding(std::move(project),
                                                     session_.instance_binding());
    }
}

} // namespace xen::ipc
