#include <xen/sequencer_session.hpp>

#include <algorithm>
#include <exception>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include <xen/command_transaction.hpp>
#include <xen/project_validation.hpp>
#include <xen/selection.hpp>
#include <xen/string_manip.hpp>

namespace
{

auto error_result(std::string message) -> xen::CommandApplicationResult
{
    return {
        .status = {xen::MessageLevel::Error, std::move(message)},
        .suggested_selection = std::nullopt,
    };
}

auto validate_selection_target(xen::TargetRequirement requirement,
                               std::optional<xen::SelectionPath> const &selection,
                               xen::Measure const &measure)
    -> std::optional<xen::CommandApplicationResult>
{
    using enum xen::TargetRequirement;

    if (requirement == None)
    {
        return std::nullopt;
    }
    if (!selection.has_value())
    {
        return error_result("selection is required");
    }

    try
    {
        switch (requirement)
        {
        case Cell:
            (void)xen::get_selected_cell_const(measure, *selection);
            return std::nullopt;
        case Element:
            (void)xen::get_selected_element_const(measure, *selection);
            return std::nullopt;
        case CellOrElement:
            if (xen::selection_kind(*selection) == xen::SelectionKind::Element)
            {
                (void)xen::get_selected_element_const(measure, *selection);
            }
            else
            {
                (void)xen::get_selected_cell_const(measure, *selection);
            }
            return std::nullopt;
        case None:
            return std::nullopt;
        }
    }
    catch (std::exception const &)
    {
        switch (requirement)
        {
        case Cell:
            return error_result("selection must resolve to a cell");
        case Element:
            return error_result("selection must resolve to an element");
        case CellOrElement:
            return error_result("selection path does not resolve");
        case None:
            break;
        }
    }

    return std::nullopt;
}

} // namespace

namespace xen
{

SequencerSession::SequencerSession(SubmissionEffects::FailurePoint effect_failure,
                                   std::filesystem::path workspace_settings_file)
    : state_{.workspace =
                 WorkspaceSettingsStore{workspace_settings_file}.load_or_initialize(),
             .timeline = XenTimeline{ProjectState{}}},
      workspace_settings_store_{std::move(workspace_settings_file)},
      command_catalog_{create_command_catalog()}, effect_failure_{effect_failure}
{
    publish_project_snapshot();
    (void)execute_command_string("load scales", CommandContext{});
    (void)execute_command_string("load chords", CommandContext{});
}

auto SequencerSession::project_snapshot() const -> ProjectSnapshot
{
    return ProjectSnapshot{
        .project = state_.timeline.get_state(),
        .history_entry_id = state_.timeline.get_current_entry_id(),
        .project_revision = state_.timeline.get_project_revision(),
    };
}

auto SequencerSession::library_snapshot() const -> LibrarySnapshot
{
    return LibrarySnapshot{
        .library = state_.library,
        .workspace = state_.workspace,
        .library_revision = state_.library_revision,
    };
}

auto SequencerSession::command_catalog() const noexcept -> CommandCatalog const &
{
    return command_catalog_;
}

auto SequencerSession::command_session() const noexcept -> CommandSessionState const &
{
    return state_.command_session;
}

auto SequencerSession::execute_command_string(std::string const &command_string,
                                              CommandContext const &context)
    -> CommandApplicationResult
{
    try
    {
        auto const parsed = parse_command_chain(command_string);
        auto expanded = std::vector<CommandInvocation>{};
        for (auto const &invocation : parsed)
        {
            auto const result = command_catalog_.bind_invocation(invocation);
            if (std::holds_alternative<CatalogBindError>(result))
            {
                return error_result(std::get<CatalogBindError>(result).message);
            }
            auto const &step = std::get<BoundStep>(result);
            if (std::holds_alternative<RepeatPrevious>(step))
            {
                if (state_.command_session.repeat_chain.empty())
                {
                    return error_result("No previous command to repeat.");
                }
                expanded.insert(expanded.end(),
                                state_.command_session.repeat_chain.begin(),
                                state_.command_session.repeat_chain.end());
            }
            else
            {
                expanded.push_back(invocation);
            }
        }

        auto const bind_result = command_catalog_.bind_chain(expanded);
        if (std::holds_alternative<CatalogBindError>(bind_result))
        {
            return error_result(std::get<CatalogBindError>(bind_result).message);
        }
        auto const &steps = std::get<std::vector<BoundStep>>(bind_result);

        auto const project_aware =
            std::ranges::any_of(steps, [](BoundStep const &step) {
                return std::visit(
                    [](auto const &typed_step) {
                        using Step = std::decay_t<decltype(typed_step)>;
                        if constexpr (std::is_same_v<Step, RepeatPrevious>)
                        {
                            return false;
                        }
                        else
                        {
                            return typed_step.policy.project != ProjectOperation::None;
                        }
                    },
                    step);
            });
        if (project_aware && !context.expected_project_revision.has_value())
        {
            return error_result("expected project revision is required");
        }
        auto const current_revision = state_.timeline.get_project_revision();
        if (project_aware && *context.expected_project_revision != current_revision)
        {
            return error_result(
                "stale project revision: expected " +
                std::to_string(context.expected_project_revision->value()) +
                ", current " + std::to_string(current_revision.value()));
        }

        auto const history_count =
            std::ranges::count_if(steps, [](BoundStep const &step) {
                return std::holds_alternative<ExecutableHistoryNavigation>(step);
            });
        if (history_count > 0 && steps.size() != 1)
        {
            return error_result("undo and redo must be submitted alone.");
        }

        auto transaction = CommandTransaction{state_, effect_failure_};
        if (history_count == 1)
        {
            transaction.invalidate_transform_sessions();
        }
        auto execution_context =
            CommandExecutionContext{.selection = context.selection};
        auto result = CommandApplicationResult{};
        auto const initial_engine = state_.timeline.get_state();
        auto const initial_revision = state_.timeline.get_project_revision();

        for (auto const &step : steps)
        {
            if (std::holds_alternative<RepeatPrevious>(step))
            {
                return error_result("Recursive 'again' expansion is not allowed.");
            }

            if (std::holds_alternative<ExecutableHistoryNavigation>(step))
            {
                auto const &navigation = std::get<ExecutableHistoryNavigation>(step);
                transaction.plan_history(HistoryPlan{
                    .kind = navigation.direction == HistoryNavigationDirection::Undo
                                ? HistoryPlanKind::NavigateUndo
                                : HistoryPlanKind::NavigateRedo,
                });
                result.status = CommandStatus{MessageLevel::Info,
                                              navigation.direction ==
                                                      HistoryNavigationDirection::Undo
                                                  ? "Undone"
                                                  : "Redone"};
                result.suggested_selection = std::nullopt;
                continue;
            }

            auto const &command = std::get<ExecutableCommand>(step);
            if (auto selection_error = validate_selection_target(
                    command.policy.target, execution_context.selection,
                    default_measure(transaction.project()));
                selection_error.has_value())
            {
                return *selection_error;
            }

            auto const before = transaction.project();
            result = command.execute(transaction, execution_context);
            if (result.status.first == MessageLevel::Error)
            {
                return result;
            }

            if (result.suggested_selection.has_value())
            {
                execution_context.selection = result.suggested_selection;
            }

            auto const changed = transaction.project() != before;
            if ((changed &&
                 command.policy.history != HistoryPolicy::AmendCompatibleTransform) ||
                transaction.library_changed() || transaction.workspace_changed())
            {
                transaction.invalidate_transform_sessions();
            }
            if (changed &&
                command.policy.repeat == RepeatPolicy::OnSuccessfulProjectChange)
            {
                transaction.record_repeat(command.invocation);
            }
        }

        if (steps.size() != 1 && transaction.history_plan_is_amend())
        {
            transaction.plan_history(HistoryPlan{.kind = HistoryPlanKind::Commit});
        }
        if (history_count == 0 && transaction.project_changed() &&
            !transaction.has_history_plan())
        {
            transaction.plan_history(HistoryPlan{.kind = HistoryPlanKind::Commit});
        }

        auto const project_changed = transaction.project_changed();
        transaction.prepare();
        try
        {
            transaction.apply_effects();
            if (transaction.workspace_changed())
            {
                workspace_settings_store_.save(transaction.workspace());
            }
        }
        catch (std::exception const &e)
        {
            auto const rollback_failures = transaction.rollback_effects();
            auto message = std::string{e.what()};
            if (!rollback_failures.empty())
            {
                message += "; rollback failed for: " + rollback_failures;
            }
            return error_result(std::move(message));
        }

        transaction.install();
        transaction.finalize_effects();
        if (transaction.repeat_candidate().has_value() && project_changed)
        {
            state_.command_session.repeat_chain = *transaction.repeat_candidate();
        }
        auto const &final_engine = state_.timeline.get_state();
        auto const final_revision = state_.timeline.get_project_revision();
        if (history_count == 1 && final_revision == initial_revision)
        {
            auto const &navigation =
                std::get<ExecutableHistoryNavigation>(steps.front());
            result.status =
                CommandStatus{MessageLevel::Info,
                              navigation.direction == HistoryNavigationDirection::Undo
                                  ? "Nothing to undo."
                                  : "Nothing to redo."};
        }
        if (final_revision != initial_revision || final_engine != initial_engine)
        {
            publish_project_snapshot();
        }
        if (!expanded.empty() &&
            expanded.front().input.words == std::vector<std::string>{"reset"})
        {
            state_.command_session = CommandSessionState{};
        }
        return result;
    }
    catch (std::exception const &e)
    {
        return error_result(e.what());
    }
    catch (...)
    {
        return error_result("Unknown error");
    }
}

void SequencerSession::replace_project_history(ProjectState state)
{
    state_.timeline.replace_history(std::move(state));
    state_.command_session = CommandSessionState{};
    publish_project_snapshot();
}

void SequencerSession::replace_library(ContentLibrary library)
{
    state_.library = std::move(library);
    state_.library_revision = detail::allocate_library_revision();
}

auto SequencerSession::audio_project_update_version() const noexcept -> std::uint64_t
{
    return pending_engine_state_update_.version();
}

auto SequencerSession::try_consume_audio_project_update() noexcept
    -> std::optional<EngineStateMailbox::ReadView>
{
    return pending_engine_state_update_.try_consume_latest();
}

void SequencerSession::publish_project_snapshot()
{
    validate(state_.timeline.get_state());
    pending_engine_state_update_.publish(
        AudioProjectSnapshot{state_.timeline.get_state()});
}

} // namespace xen
