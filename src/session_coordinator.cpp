#include <xen/session_coordinator.hpp>

#include <algorithm>
#include <limits>
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
    if (preview_owner_.has_value())
    {
        auto const revision = session_.project_snapshot().project_revision;
        (void)session_.cancel_preview(preview_owner_->preview_id, revision);
        preview_owner_.reset();
    }
    maybe_seed_from(hello);
    auto binding = assign_binding(std::move(hello.binding));
    auto const instance_id = binding.instance_id;
    bindings_[instance_id] = binding;
    return {
        .binding = bindings_.at(instance_id),
        .snapshot = session_.project_snapshot(),
        .persistent_snapshot = session_.persistent_project_snapshot(),
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
    if (request.context.preview_id.has_value() &&
        (!preview_owner_.has_value() ||
         preview_owner_->instance_id != request.source_instance_id ||
         preview_owner_->preview_id != *request.context.preview_id))
    {
        return {
            .request_id = std::move(request.request_id),
            .result = {.status = {MessageLevel::Error,
                                  "Project preview is owned by another instance."}},
            .snapshot = session_.project_snapshot(),
        };
    }
    live_edit_started_ = true;
    auto result = session_.execute_command_string(request.command, request.context);
    return {
        .request_id = std::move(request.request_id),
        .result = std::move(result),
        .snapshot = session_.project_snapshot(),
    };
}

auto SessionCoordinator::begin_preview(PreviewBeginRequest request) -> PreviewResponse
{
    if (bindings_.find(request.source_instance_id) == bindings_.end())
    {
        throw std::invalid_argument{"Unknown source instance ID."};
    }
    auto result = session_.begin_preview(request.expected_project_revision);
    if (result.preview_id.has_value())
    {
        live_edit_started_ = true;
        preview_owner_ = PreviewOwner{
            .preview_id = *result.preview_id,
            .instance_id = request.source_instance_id,
        };
    }
    return {
        .request_id = std::move(request.request_id),
        .result = std::move(result),
        .snapshot = session_.project_snapshot(),
    };
}

auto SessionCoordinator::commit_preview(PreviewEndRequest request) -> PreviewResponse
{
    if (!preview_owner_.has_value() ||
        preview_owner_->instance_id != request.source_instance_id ||
        preview_owner_->preview_id != request.preview_id)
    {
        return {
            .request_id = std::move(request.request_id),
            .result = {.status = {MessageLevel::Error,
                                  "Project preview is owned by another instance."}},
            .snapshot = session_.project_snapshot(),
        };
    }
    auto result =
        session_.commit_preview(request.preview_id, request.expected_project_revision);
    if (result.status.first != MessageLevel::Error)
    {
        preview_owner_.reset();
    }
    return {
        .request_id = std::move(request.request_id),
        .result = std::move(result),
        .snapshot = session_.project_snapshot(),
    };
}

auto SessionCoordinator::cancel_preview(PreviewEndRequest request) -> PreviewResponse
{
    if (!preview_owner_.has_value() ||
        preview_owner_->instance_id != request.source_instance_id ||
        preview_owner_->preview_id != request.preview_id)
    {
        return {
            .request_id = std::move(request.request_id),
            .result = {.status = {MessageLevel::Error,
                                  "Project preview is owned by another instance."}},
            .snapshot = session_.project_snapshot(),
        };
    }
    auto result =
        session_.cancel_preview(request.preview_id, request.expected_project_revision);
    if (result.status.first != MessageLevel::Error)
    {
        preview_owner_.reset();
    }
    return {
        .request_id = std::move(request.request_id),
        .result = std::move(result),
        .snapshot = session_.project_snapshot(),
    };
}

auto SessionCoordinator::disconnect(InstanceId const &instance_id)
    -> std::optional<ProjectSnapshot>
{
    if (!preview_owner_.has_value() || preview_owner_->instance_id != instance_id)
    {
        return std::nullopt;
    }
    auto const preview_id = preview_owner_->preview_id;
    auto const revision = session_.project_snapshot().project_revision;
    auto const result = session_.cancel_preview(preview_id, revision);
    preview_owner_.reset();
    if (result.status.first == MessageLevel::Error)
    {
        return std::nullopt;
    }
    return session_.project_snapshot();
}

auto SessionCoordinator::set_binding(BindingSetRequest request) -> BindingSetResponse
{
    if (preview_owner_.has_value())
    {
        throw std::runtime_error{"A project preview is active; commit or cancel it "
                                 "before changing bindings."};
    }
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
    auto const found =
        std::ranges::any_of(project.composition.rows, [&channel_id](auto const &entry) {
            return entry.second.channel_id == channel_id;
        });
    if (!found)
    {
        auto row_coordinate = CompositionCoordinate{};
        if (!project.composition.rows.empty())
        {
            auto const greatest = project.composition.rows.rbegin()->first;
            if (greatest >= 0)
            {
                if (greatest == std::numeric_limits<CompositionCoordinate>::max())
                    throw std::overflow_error{"Composition row coordinate overflow."};
                row_coordinate = greatest + 1;
            }
        }
        auto const sequence_id =
            create_sequence(project.sequence_bank, sequence::Cell{});
        (void)ensure_composition_row(project.composition, row_coordinate, channel_id);
        assign_sequence_reference(project.composition, row_coordinate, 0, sequence_id);
        session_.replace_project_history_and_binding(std::move(project),
                                                     session_.instance_binding());
    }
}

} // namespace xen::ipc
