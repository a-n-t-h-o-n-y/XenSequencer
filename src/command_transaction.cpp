#include <xen/command_transaction.hpp>

#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include <xen/chord.hpp>
#include <xen/selection.hpp>

namespace xen
{
static_assert(std::is_nothrow_swappable_v<XenTimeline>);
static_assert(std::is_nothrow_swappable_v<ContentLibraryState>);
static_assert(std::is_nothrow_swappable_v<AppConfigState>);
static_assert(std::is_nothrow_move_assignable_v<EngineState>);

namespace
{

[[noreturn]] void denied(char const *capability)
{
    throw std::logic_error{std::string{"Command handler lacks "} + capability +
                           " capability."};
}

auto resolve_chord_cycle(std::vector<Chord> const &chords, ChordCycleState &cycle,
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

auto ProjectReadCapability::get() const -> EngineState const &
{
    return transaction_.project();
}

auto ProjectEditCapability::get() -> EngineState &
{
    return transaction_.edit_project();
}

auto LibraryReadCapability::get() const -> ContentLibraryState const &
{
    return transaction_.library();
}

auto LibraryEditCapability::get() -> ContentLibraryState &
{
    return transaction_.edit_library();
}

auto WorkspaceReadCapability::get() const -> AppConfigState const &
{
    return transaction_.workspace();
}

auto WorkspaceEditCapability::get() -> AppConfigState &
{
    return transaction_.edit_workspace();
}

auto FileReadCapability::read_text(juce::File const &source) const
    -> std::optional<std::string>
{
    return transaction_.effects().read_text(source);
}

void FileWriteCapability::write_text(juce::File const &destination, std::string content)
{
    transaction_.effects().write_text(destination, std::move(content));
}

auto CommandHandlerContext::project() const -> EngineState const &
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

auto CommandHandlerContext::edit_project() -> EngineState &
{
    if (project_edit == nullptr)
    {
        denied("project-edit");
    }
    return project_edit->get();
}

auto CommandHandlerContext::library() const -> ContentLibraryState const &
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

auto CommandHandlerContext::edit_library() -> ContentLibraryState &
{
    if (library_edit == nullptr)
    {
        denied("library-edit");
    }
    return library_edit->get();
}

auto CommandHandlerContext::workspace() const -> AppConfigState const &
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

auto CommandHandlerContext::edit_workspace() -> AppConfigState &
{
    if (workspace_edit == nullptr)
    {
        denied("workspace-edit");
    }
    return workspace_edit->get();
}

auto CommandHandlerContext::read_text(juce::File const &source) const
    -> std::optional<std::string>
{
    if (file_read == nullptr)
    {
        denied("file-read");
    }
    return file_read->read_text(source);
}

void CommandHandlerContext::write_text(juce::File const &destination,
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

auto CommandTransaction::project() const -> EngineState const &
{
    return project_.has_value() ? *project_ : state_.timeline.get_state();
}

auto CommandTransaction::edit_project() -> EngineState &
{
    if (!project_.has_value())
    {
        project_ = state_.timeline.get_state();
    }
    return *project_;
}

auto CommandTransaction::library() const -> ContentLibraryState const &
{
    return library_.has_value() ? *library_ : state_.library;
}

auto CommandTransaction::edit_library() -> ContentLibraryState &
{
    if (!library_.has_value())
    {
        library_ = state_.library;
    }
    return *library_;
}

auto CommandTransaction::workspace() const -> AppConfigState const &
{
    return workspace_.has_value() ? *workspace_ : state_.config;
}

auto CommandTransaction::edit_workspace() -> AppConfigState &
{
    if (!workspace_.has_value())
    {
        workspace_ = state_.config;
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
        sessions_ = state_.sessions;
    }
    auto &cycle =
        kind == TransformKind::Arpeggio ? sessions_->arp_state : sessions_->chord_state;
    auto const revision = state_.timeline.get_project_revision();
    if (selection != cycle.selection || cycle.previous_project_revision != revision)
    {
        cycle.sequencer = project();
        cycle.selection = selection;
    }
    std::tie(chord_name, inversion) =
        resolve_chord_cycle(library().chords, cycle, std::move(chord_name), inversion);
    cycle.previous_chord_name = chord_name;
    cycle.previous_inversion = inversion;
    cycle.previous_project_revision = revision;
    return TransformInputs{
        .baseline = cycle.sequencer,
        .selection = cycle.selection,
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
    sessions_ = TransformSessionState{};
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
    return workspace_.has_value();
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

void CommandTransaction::prepare()
{
    if (history_.kind != HistoryPlanKind::None)
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
    }
    if (workspace_.has_value())
    {
        using std::swap;
        swap(state_.config, *workspace_);
    }
    if (sessions_.has_value())
    {
        using std::swap;
        swap(state_.sessions, *sessions_);
    }
}

void CommandTransaction::finalize_effects() noexcept
{
    effects_.finalize();
}

} // namespace xen
