#include <xen/command_transaction.hpp>

#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include <xen/chord.hpp>
#include <xen/project_validation.hpp>
#include <xen/selection.hpp>

namespace xen
{
static_assert(std::is_nothrow_swappable_v<XenTimeline>);
static_assert(std::is_nothrow_swappable_v<ContentLibrary>);
static_assert(std::is_nothrow_swappable_v<WorkspaceSettings>);
static_assert(std::is_nothrow_move_assignable_v<ProjectState>);

namespace
{

[[noreturn]] void denied(char const *capability)
{
    throw std::logic_error{std::string{"Command handler lacks "} + capability +
                           " capability."};
}

auto resolve_chord_cycle(std::vector<Chord> const &chords, TransformCycleSession &cycle,
                         std::string chord_name, int inversion)
    -> std::pair<std::string, int>
{
    if (chord_name == "cycle" && inversion != -1)
    {
        chord_name = find_next_chord(chords, cycle.previous_chord_name).name;
        auto const chord = find_chord(chords, chord_name);
        if (chord.intervals.empty() ||
            chord.intervals.size() >
                static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::invalid_argument{"Invalid chord interval count."};
        }
        inversion = std::min(inversion, static_cast<int>(chord.intervals.size() - 1));
    }
    else if (chord_name != "cycle" && inversion == -1)
    {
        inversion = increment_inversion(find_chord(chords, chord_name),
                                        cycle.previous_inversion);
    }
    else if (chord_name == "cycle" && inversion == -1)
    {
        chord_name = cycle.previous_chord_name;
        inversion = chord_name.empty()
                        ? 0
                        : increment_inversion(find_chord(chords, chord_name),
                                              cycle.previous_inversion);
        if (inversion == 0)
        {
            chord_name = find_next_chord(chords, chord_name).name;
        }
    }
    return {std::move(chord_name), inversion};
}

} // namespace

auto ProjectReadCapability::get() const -> ProjectState const &
{
    return transaction_.project();
}

auto ProjectEditCapability::get() -> ProjectState &
{
    return transaction_.edit_project();
}

auto LibraryReadCapability::get() const -> ContentLibrary const &
{
    return transaction_.library();
}

auto LibraryEditCapability::get() -> ContentLibrary &
{
    return transaction_.edit_library();
}

auto WorkspaceReadCapability::get() const -> WorkspaceSettings const &
{
    return transaction_.workspace();
}

auto WorkspaceEditCapability::get() -> WorkspaceSettings &
{
    return transaction_.edit_workspace();
}

auto FileReadCapability::read_text(std::filesystem::path const &source) const
    -> std::optional<std::string>
{
    return transaction_.effects().read_text(source);
}

void FileWriteCapability::write_text(std::filesystem::path const &destination,
                                     std::string content)
{
    transaction_.effects().write_text(destination, std::move(content));
}

auto CommandHandlerContext::project() const -> ProjectState const &
{
    if (project_read != nullptr)
    {
        return project_read->get();
    }
    if (project_edit != nullptr)
    {
        return project_edit->get();
    }
    denied("project-read");
}

auto CommandHandlerContext::edit_project() -> ProjectState &
{
    if (project_edit == nullptr)
    {
        denied("project-edit");
    }
    return project_edit->get();
}

auto CommandHandlerContext::library() const -> ContentLibrary const &
{
    if (library_read != nullptr)
    {
        return library_read->get();
    }
    if (library_edit != nullptr)
    {
        return library_edit->get();
    }
    denied("library-read");
}

auto CommandHandlerContext::edit_library() -> ContentLibrary &
{
    if (library_edit == nullptr)
    {
        denied("library-edit");
    }
    return library_edit->get();
}

auto CommandHandlerContext::workspace() const -> WorkspaceSettings const &
{
    if (workspace_read != nullptr)
    {
        return workspace_read->get();
    }
    if (workspace_edit != nullptr)
    {
        return workspace_edit->get();
    }
    denied("workspace-read");
}

auto CommandHandlerContext::edit_workspace() -> WorkspaceSettings &
{
    if (workspace_edit == nullptr)
    {
        denied("workspace-edit");
    }
    return workspace_edit->get();
}

auto CommandHandlerContext::read_text(std::filesystem::path const &source) const
    -> std::optional<std::string>
{
    if (file_read == nullptr)
    {
        denied("file-read");
    }
    return file_read->read_text(source);
}

void CommandHandlerContext::write_text(std::filesystem::path const &destination,
                                       std::string content)
{
    if (file_write == nullptr)
    {
        denied("file-write");
    }
    file_write->write_text(destination, std::move(content));
}

auto CommandHandlerContext::prepare_transform(TransformKind kind,
                                              std::string chord_name, int inversion)
    -> TransformInputs
{
    if (!execution.selection.has_value())
    {
        throw std::logic_error{"Resolved target is required."};
    }
    return project_edit->transaction_.prepare_transform(
        kind, *execution.selection, std::move(chord_name), inversion);
}

CommandTransaction::CommandTransaction(PluginState &state,
                                       SubmissionEffects::FailurePoint effect_failure)
    : state_{state}, effects_{effect_failure}
{
}

auto CommandTransaction::make_handler_context(CommandPolicy const &policy,
                                              CommandExecutionContext &execution)
    -> CommandHandlerContext
{
    return CommandHandlerContext{
        .project_read =
            policy.project == ProjectOperation::Read ? &project_read_ : nullptr,
        .project_edit =
            policy.project == ProjectOperation::Edit ? &project_edit_ : nullptr,
        .library_read =
            policy.library == LibraryAccess::Read ? &library_read_ : nullptr,
        .library_edit =
            policy.library == LibraryAccess::Mutate ? &library_edit_ : nullptr,
        .workspace_read =
            policy.workspace == WorkspaceAccess::Read ? &workspace_read_ : nullptr,
        .workspace_edit =
            policy.workspace == WorkspaceAccess::Mutate ? &workspace_edit_ : nullptr,
        .file_read = policy.files == FileAccess::Read ? &file_read_ : nullptr,
        .file_write = policy.files == FileAccess::Write ? &file_write_ : nullptr,
        .execution = execution,
    };
}

auto CommandTransaction::project() const -> ProjectState const &
{
    return project_.has_value() ? *project_ : state_.timeline.get_state();
}

auto CommandTransaction::edit_project() -> ProjectState &
{
    if (!project_.has_value())
    {
        project_ = state_.timeline.get_state();
    }
    return *project_;
}

auto CommandTransaction::library() const -> ContentLibrary const &
{
    return library_.has_value() ? *library_ : state_.library;
}

auto CommandTransaction::edit_library() -> ContentLibrary &
{
    if (!library_.has_value())
    {
        library_ = state_.library;
    }
    return *library_;
}

auto CommandTransaction::workspace() const -> WorkspaceSettings const &
{
    return workspace_.has_value() ? *workspace_ : state_.workspace;
}

auto CommandTransaction::edit_workspace() -> WorkspaceSettings &
{
    if (!workspace_.has_value())
    {
        workspace_ = state_.workspace;
    }
    return *workspace_;
}

auto CommandTransaction::effects() noexcept -> SubmissionEffects &
{
    return effects_;
}

auto CommandTransaction::prepare_transform(TransformKind kind,
                                           SelectionPath const &selection,
                                           std::string chord_name, int inversion)
    -> TransformInputs
{
    if (!sessions_.has_value())
    {
        sessions_ = state_.command_session;
    }
    auto const revision = state_.timeline.get_project_revision();
    auto const entry_id = state_.timeline.get_current_entry_id();
    auto const compatible =
        sessions_->transform_cycle.has_value() &&
        sessions_->transform_cycle->kind == kind &&
        sessions_->transform_cycle->target == selection &&
        sessions_->transform_cycle->project_revision == revision &&
        sessions_->transform_cycle->history_entry_id == entry_id &&
        sessions_->transform_cycle->library_revision == state_.library_revision;
    if (!compatible)
    {
        auto baseline =
            selection_kind(selection) == SelectionKind::Element
                ? TargetSnapshot{get_selected_element_const(project().measure,
                                                            selection)}
                : TargetSnapshot{get_selected_cell_const(project().measure, selection)};
        sessions_->transform_cycle = TransformCycleSession{
            .kind = kind,
            .target = selection,
            .baseline = std::move(baseline),
            .project_revision = revision,
            .history_entry_id = entry_id,
            .library_revision = state_.library_revision,
        };
    }
    auto &cycle = *sessions_->transform_cycle;
    std::tie(chord_name, inversion) =
        resolve_chord_cycle(library().chords, cycle, std::move(chord_name), inversion);
    cycle.previous_chord_name = chord_name;
    cycle.previous_inversion = inversion;
    auto baseline_project = project();
    if (std::holds_alternative<sequence::Cell>(cycle.baseline))
    {
        get_selected_cell(baseline_project.measure, selection) =
            std::get<sequence::Cell>(cycle.baseline);
    }
    else
    {
        get_selected_element(baseline_project.measure, selection) =
            std::get<sequence::MusicElement>(cycle.baseline);
    }
    if (compatible && cycle.committed)
    {
        plan_history(HistoryPlan{HistoryPlanKind::Amend, cycle.history_entry_id});
    }
    return TransformInputs{
        .baseline = std::move(baseline_project),
        .selection = cycle.target,
        .chord_name = std::move(chord_name),
        .inversion = inversion,
    };
}

void CommandTransaction::plan_history(HistoryPlan plan) noexcept
{
    history_ = plan;
}

void CommandTransaction::record_repeat(CommandInvocation invocation)
{
    if (!repeat_candidate_.has_value())
    {
        repeat_candidate_.emplace();
    }
    repeat_candidate_->push_back(std::move(invocation));
}

void CommandTransaction::clear_repeat() noexcept
{
    repeat_candidate_.reset();
}

void CommandTransaction::invalidate_transform_sessions()
{
    if (!sessions_.has_value())
    {
        sessions_ = state_.command_session;
    }
    sessions_->transform_cycle.reset();
}

auto CommandTransaction::repeat_candidate() const
    -> std::optional<std::vector<CommandInvocation>> const &
{
    return repeat_candidate_;
}

auto CommandTransaction::project_changed() const -> bool
{
    return project_.has_value() && *project_ != state_.timeline.get_state();
}

auto CommandTransaction::library_changed() const -> bool
{
    return library_.has_value();
}

auto CommandTransaction::workspace_changed() const -> bool
{
    return workspace_.has_value() && *workspace_ != state_.workspace;
}

auto CommandTransaction::has_domain_candidates() const noexcept -> bool
{
    return project_.has_value() || library_.has_value() || workspace_.has_value();
}

auto CommandTransaction::has_project_candidate() const noexcept -> bool
{
    return project_.has_value();
}

auto CommandTransaction::has_library_candidate() const noexcept -> bool
{
    return library_.has_value();
}

auto CommandTransaction::has_workspace_candidate() const noexcept -> bool
{
    return workspace_.has_value();
}

auto CommandTransaction::has_history_plan() const noexcept -> bool
{
    return history_.kind != HistoryPlanKind::None;
}

auto CommandTransaction::history_plan_is_amend() const noexcept -> bool
{
    return history_.kind == HistoryPlanKind::Amend;
}

void CommandTransaction::prepare()
{
    if (project_.has_value())
    {
        validate(*project_);
    }
    if (library_.has_value())
    {
        validate(*library_);
    }
    if (workspace_changed())
    {
        validate(*workspace_);
    }
    auto const changed_history_candidate = (history_.kind != HistoryPlanKind::Commit &&
                                            history_.kind != HistoryPlanKind::Amend) ||
                                           project_changed();
    if (history_.kind != HistoryPlanKind::None && changed_history_candidate)
    {
        prepared_timeline_ = state_.timeline;
        switch (history_.kind)
        {
        case HistoryPlanKind::Commit:
            (void)prepared_timeline_->commit(project());
            break;
        case HistoryPlanKind::Amend:
            if (!history_.expected_entry_id.has_value() ||
                !prepared_timeline_->amend_current(*history_.expected_entry_id,
                                                   project()))
            {
                throw std::runtime_error{"History amendment guard failed."};
            }
            break;
        case HistoryPlanKind::Replace:
            prepared_timeline_->replace_history(project());
            break;
        case HistoryPlanKind::NavigateUndo:
            (void)prepared_timeline_->undo();
            break;
        case HistoryPlanKind::NavigateRedo:
            (void)prepared_timeline_->redo();
            break;
        case HistoryPlanKind::None:
            break;
        }
        if (sessions_.has_value() && sessions_->transform_cycle.has_value() &&
            prepared_timeline_->get_project_revision() !=
                state_.timeline.get_project_revision())
        {
            auto &cycle = *sessions_->transform_cycle;
            cycle.project_revision = prepared_timeline_->get_project_revision();
            cycle.history_entry_id = prepared_timeline_->get_current_entry_id();
            cycle.committed = true;
        }
    }
    effects_.prepare();
}

void CommandTransaction::apply_effects()
{
    effects_.apply();
}

auto CommandTransaction::rollback_effects() noexcept -> std::string
{
    return effects_.rollback();
}

void CommandTransaction::install() noexcept
{
    if (prepared_timeline_.has_value())
    {
        using std::swap;
        swap(state_.timeline, *prepared_timeline_);
    }
    else if (project_.has_value())
    {
        state_.timeline.stage(std::move(*project_));
    }
    if (library_.has_value())
    {
        using std::swap;
        swap(state_.library, *library_);
        state_.library_revision = detail::allocate_library_revision();
    }
    if (workspace_.has_value())
    {
        using std::swap;
        swap(state_.workspace, *workspace_);
        state_.library_revision = detail::allocate_library_revision();
    }
    if (sessions_.has_value())
    {
        using std::swap;
        swap(state_.command_session, *sessions_);
    }
}

void CommandTransaction::finalize_effects() noexcept
{
    effects_.finalize();
}

} // namespace xen
