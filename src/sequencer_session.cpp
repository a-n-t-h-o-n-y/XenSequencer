#include <xen/sequencer_session.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>
#include <nlohmann/json.hpp>

#include <xen/actions.hpp>
#include <xen/command_transaction.hpp>
#include <xen/document_storage.hpp>
#include <xen/project_validation.hpp>
#include <xen/selection.hpp>
#include <xen/serialize.hpp>
#include <xen/string_manip.hpp>
#include <xen/text_file.hpp>
#include <xen/user_directory.hpp>

namespace
{

auto error_result(std::string message) -> xen::CommandApplicationResult
{
    return {
        .status = {xen::MessageLevel::Error, std::move(message)},
        .suggested_selection = std::nullopt,
    };
}

auto preview_error(std::string message) -> xen::PreviewControlResult
{
    return {
        .status = {xen::MessageLevel::Error, std::move(message)},
    };
}

auto modulation_preview_error(std::string message,
                              xen::ModulationPreviewUpdate const &update,
                              xen::ProjectRevision project_revision,
                              xen::StateRevision state_revision,
                              std::uint64_t accepted_sequence = 0)
    -> xen::ModulationPreviewUpdateResult
{
    return {
        .status = {xen::MessageLevel::Error, std::move(message)},
        .preview_id = update.preview_id,
        .accepted_update_sequence = accepted_sequence,
        .accepted = false,
        .project_changed = false,
        .project_revision = project_revision,
        .state_revision = state_revision,
    };
}

auto now_unix_ms() -> std::uint64_t
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void validate_binding(xen::InstanceBinding const &binding)
{
    if (binding.session_id.empty() || binding.instance_id.empty() ||
        binding.channel_id.empty())
    {
        throw std::invalid_argument{"Instance binding fields must not be empty."};
    }
    if (binding.session_id.size() > xen::MAX_PERSISTED_STRING_BYTES ||
        binding.instance_id.size() > xen::MAX_PERSISTED_STRING_BYTES ||
        binding.channel_id.size() > xen::MAX_PERSISTED_STRING_BYTES)
    {
        throw std::invalid_argument{"Instance binding field is too long."};
    }
}

auto is_valid_recovery_artifact(juce::File const &file,
                                xen::SessionId const &session_id) -> bool
{
    try
    {
        auto const text = xen::read_text_file(file.getFullPathName().toStdString(),
                                              xen::MAX_PERSISTED_STATE_BYTES);
        return text.has_value() &&
               xen::deserialize_recovery_state(*text).session_id == session_id;
    }
    catch (std::exception const &)
    {
        return false;
    }
}

void recover_recovery_file(std::filesystem::path const &path,
                           xen::SessionId const &session_id)
{
    auto const destination = juce::File{path.string()};
    auto const parent = destination.getParentDirectory();
    if (!parent.isDirectory())
    {
        return;
    }
    auto found = parent.findChildFiles(juce::File::findFiles, false,
                                       destination.getFileName() + ".xen-tmp.*");
    auto temporaries = std::vector<juce::File>{};
    temporaries.reserve(static_cast<std::size_t>(found.size()));
    for (auto const &temporary : found)
    {
        temporaries.push_back(temporary);
    }
    std::ranges::sort(temporaries, std::ranges::greater{}, [](juce::File const &file) {
        return file.getLastModificationTime().toMilliseconds();
    });

    auto canonical_valid = is_valid_recovery_artifact(destination, session_id);
    if (!canonical_valid)
    {
        if (destination.exists() && !destination.deleteFile())
        {
            return;
        }
        for (auto const &temporary : temporaries)
        {
            if (is_valid_recovery_artifact(temporary, session_id) &&
                temporary.moveFileTo(destination))
            {
                canonical_valid = true;
                break;
            }
        }
    }
    for (auto const &temporary : temporaries)
    {
        if (canonical_valid || !is_valid_recovery_artifact(temporary, session_id))
        {
            (void)temporary.deleteFile();
        }
    }
}

void log_json_command_exception(std::string const &command_string,
                                xen::CommandContext const &context,
                                nlohmann::json::exception const &error)
{
    auto message = juce::String{"XenSequencer command JSON exception: "} +
                   error.what() + "\ncommand: " + command_string;
    if (context.expected_project_revision.has_value())
    {
        message += "\nexpected_project_revision: " +
                   juce::String{static_cast<juce::int64>(
                       context.expected_project_revision->value())};
    }
    else
    {
        message += "\nexpected_project_revision: <none>";
    }
    message += "\nselection: ";
    message += context.selection.has_value() ? "present" : "none";
    message += juce::String{"\ncursor: "} +
               juce::String{static_cast<juce::int64>(context.cursor.row_coordinate)} +
               "," +
               juce::String{static_cast<juce::int64>(context.cursor.column_coordinate)};
    juce::Logger::writeToLog(message);

    try
    {
        auto const log_file =
            xen::get_user_settings_directory().getChildFile("command-errors.log");
        log_file.appendText(message + "\n\n", false, false, "\n");
    }
    catch (std::exception const &)
    {
    }
}

auto validate_selection_target(xen::TargetRequirement requirement,
                               std::optional<xen::SelectionPath> const &selection,
                               xen::ProjectState const &project,
                               xen::CompositionCursor const &cursor)
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

    auto const *cell = static_cast<sequence::Cell const *>(nullptr);
    try
    {
        cell = &xen::selected_sequence(project, cursor);
    }
    catch (std::exception const &)
    {
        return error_result("active composition cursor does not resolve");
    }

    try
    {
        switch (requirement)
        {
        case Cell:
            (void)xen::get_selected_cell_const(*cell, *selection);
            return std::nullopt;
        case Element:
            (void)xen::get_selected_element_const(*cell, *selection);
            return std::nullopt;
        case CellOrElement:
            if (xen::selection_kind(*selection) == xen::SelectionKind::Element)
            {
                (void)xen::get_selected_element_const(*cell, *selection);
            }
            else
            {
                (void)xen::get_selected_cell_const(*cell, *selection);
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
    recover_content_directory(state_.workspace.content_directory);
    (void)execute_command_string("load scales", CommandContext{});
    (void)execute_command_string("load chords", CommandContext{});
    state_.document.saved_project_digest =
        text_revision(serialize_project(state_.timeline.get_state()));
}

auto SequencerSession::project_snapshot() const -> ProjectSnapshot
{
    return ProjectSnapshot{
        .project = state_.timeline.get_state(),
        .history_entry_id = state_.timeline.get_current_entry_id(),
        .project_revision = state_.timeline.get_project_revision(),
        .state_revision = state_.state_revision,
        .preview_active = active_preview_.has_value(),
        .document = state_.document,
        .recovery = state_.recovery,
    };
}

auto SequencerSession::persistent_project_snapshot() const -> ProjectSnapshot
{
    if (active_preview_.has_value())
    {
        return active_preview_->baseline;
    }
    return project_snapshot();
}

auto SequencerSession::begin_preview(ProjectRevision expected_revision)
    -> PreviewControlResult
{
    if (active_preview_.has_value())
    {
        return preview_error("A project preview is already active.");
    }
    if (state_.recovery.has_value())
    {
        return preview_error("Restore or discard the pending recovery before editing.");
    }
    auto const current_revision = state_.timeline.get_project_revision();
    if (expected_revision != current_revision)
    {
        return preview_error("stale project revision: expected " +
                             std::to_string(expected_revision.value()) + ", current " +
                             std::to_string(current_revision.value()));
    }

    auto id = juce::Uuid{}.toString().toStdString();
    auto const baseline = project_snapshot();
    active_preview_ = ActivePreview{
        .id = id,
        .baseline = baseline,
        .command_session = state_.command_session,
        .kind = ActivePreview::Kind::Command,
    };
    state_.command_session.transform_cycle.reset();
    advance_state_revision();
    return {
        .status = {MessageLevel::Info, "Preview started."},
        .preview_id = std::move(id),
    };
}

auto SequencerSession::begin_modulation_preview(ProjectRevision expected_revision,
                                                ModulationTarget target)
    -> PreviewControlResult
{
    if (active_preview_.has_value())
    {
        return preview_error("A project preview is already active.");
    }
    if (state_.recovery.has_value())
    {
        return preview_error("Restore or discard the pending recovery before editing.");
    }
    auto const current_revision = state_.timeline.get_project_revision();
    if (expected_revision != current_revision)
    {
        return preview_error("stale project revision: expected " +
                             std::to_string(expected_revision.value()) + ", current " +
                             std::to_string(current_revision.value()));
    }
    if (target.pattern.intervals.empty() ||
        std::ranges::any_of(target.pattern.intervals,
                            [](auto interval) { return interval == 0; }))
    {
        return preview_error("Modulation pattern intervals must be positive.");
    }

    try
    {
        auto const &root =
            selected_sequence(state_.timeline.get_state(), target.cursor);
        auto const matches_pattern = [&target](sequence::Sequence const &sequence) {
            for (auto i = std::size_t{}; i < sequence.cells.size(); ++i)
            {
                if (sequence::pattern_contains(target.pattern, i))
                {
                    return true;
                }
            }
            return false;
        };
        auto target_matches = false;
        if (selection_kind(target.selection) == SelectionKind::Cell)
        {
            auto const &cell = get_selected_cell_const(root, target.selection);
            target_matches = std::ranges::any_of(
                cell.elements, [&matches_pattern](auto const &element) {
                    auto const *sequence = std::get_if<sequence::Sequence>(&element);
                    return sequence != nullptr && matches_pattern(*sequence);
                });
        }
        else
        {
            auto const &element = get_selected_element_const(root, target.selection);
            auto const *sequence = std::get_if<sequence::Sequence>(&element);
            target_matches = sequence != nullptr && matches_pattern(*sequence);
        }
        if (!target_matches)
        {
            throw std::invalid_argument{
                "Modulation target and pattern select no Sequence cells."};
        }
    }
    catch (std::exception const &error)
    {
        return preview_error(error.what());
    }

    auto id = juce::Uuid{}.toString().toStdString();
    auto const baseline = project_snapshot();
    active_preview_ = ActivePreview{
        .id = id,
        .baseline = baseline,
        .command_session = state_.command_session,
        .kind = ActivePreview::Kind::Modulation,
        .modulation_target = std::move(target),
    };
    state_.command_session.transform_cycle.reset();
    advance_state_revision();
    return {
        .status = {MessageLevel::Info, "Modulation preview started."},
        .preview_id = std::move(id),
    };
}

auto SequencerSession::update_modulation_preview(ModulationPreviewUpdate const &update)
    -> ModulationPreviewUpdateResult
{
    auto const current_revision = state_.timeline.get_project_revision();
    if (!active_preview_.has_value() || active_preview_->id != update.preview_id ||
        active_preview_->kind != ActivePreview::Kind::Modulation ||
        !active_preview_->modulation_target.has_value())
    {
        return modulation_preview_error("Unknown modulation preview.", update,
                                        current_revision, state_.state_revision);
    }
    auto const accepted_sequence = active_preview_->accepted_update_sequence;
    if (update.update_sequence <= accepted_sequence)
    {
        return {
            .status = {MessageLevel::Info, "Modulation update already superseded."},
            .preview_id = update.preview_id,
            .accepted_update_sequence = accepted_sequence,
            .accepted = false,
            .project_changed = false,
            .project_revision = current_revision,
            .state_revision = state_.state_revision,
        };
    }
    if (update.expected_project_revision != current_revision)
    {
        return modulation_preview_error(
            "stale project revision: expected " +
                std::to_string(update.expected_project_revision.value()) +
                ", current " + std::to_string(current_revision.value()),
            update, current_revision, state_.state_revision, accepted_sequence);
    }

    try
    {
        auto candidate = active_preview_->baseline.project;
        candidate = action::apply_modulation(
            std::move(candidate), *active_preview_->modulation_target,
            update.destination, update.output_range, update.modulation);
        auto const changed = state_.timeline.stage(std::move(candidate));
        active_preview_->accepted_update_sequence = update.update_sequence;
        if (changed)
        {
            advance_state_revision();
        }
        return {
            .status = {MessageLevel::Info, "Modulation preview updated."},
            .preview_id = update.preview_id,
            .accepted_update_sequence = update.update_sequence,
            .accepted = true,
            .project_changed = changed,
            .project_revision = state_.timeline.get_project_revision(),
            .state_revision = state_.state_revision,
        };
    }
    catch (std::exception const &error)
    {
        return modulation_preview_error(
            error.what(), update, state_.timeline.get_project_revision(),
            state_.state_revision, active_preview_->accepted_update_sequence);
    }
}

auto SequencerSession::commit_preview(PreviewId const &preview_id,
                                      ProjectRevision expected_revision)
    -> PreviewControlResult
{
    if (!active_preview_.has_value() || active_preview_->id != preview_id)
    {
        return preview_error("Unknown project preview.");
    }
    auto const current_revision = state_.timeline.get_project_revision();
    if (expected_revision != current_revision)
    {
        return preview_error("stale project revision: expected " +
                             std::to_string(expected_revision.value()) + ", current " +
                             std::to_string(current_revision.value()));
    }

    auto project_digest = std::string{};
    try
    {
        project_digest = text_revision(serialize_project(state_.timeline.get_state()));
    }
    catch (std::length_error const &error)
    {
        return preview_error(error.what());
    }
    auto const committed = state_.timeline.commit(state_.timeline.get_state());
    active_preview_.reset();
    state_.command_session.transform_cycle.reset();
    advance_state_revision();
    if (committed)
    {
        refresh_document_dirty(project_digest);
        schedule_recovery();
    }
    return {
        .status = {MessageLevel::Info,
                   committed ? "Preview committed." : "Preview unchanged."},
    };
}

auto SequencerSession::cancel_preview(PreviewId const &preview_id,
                                      ProjectRevision expected_revision)
    -> PreviewControlResult
{
    if (!active_preview_.has_value() || active_preview_->id != preview_id)
    {
        return preview_error("Unknown project preview.");
    }
    auto const current_revision = state_.timeline.get_project_revision();
    if (expected_revision != current_revision)
    {
        return preview_error("stale project revision: expected " +
                             std::to_string(expected_revision.value()) + ", current " +
                             std::to_string(current_revision.value()));
    }

    auto command_session = std::move(active_preview_->command_session);
    (void)state_.timeline.reset_stage();
    active_preview_.reset();
    state_.command_session = std::move(command_session);
    advance_state_revision();
    return {
        .status = {MessageLevel::Info, "Preview cancelled."},
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

auto SequencerSession::instance_binding() const -> InstanceBinding
{
    return instance_binding_;
}

auto SequencerSession::command_catalog() const noexcept -> CommandCatalog const &
{
    return command_catalog_;
}

auto SequencerSession::command_catalog_metadata() const
    -> std::vector<CatalogCommandMetadata>
{
    return command_catalog_.metadata();
}

auto SequencerSession::command_session() const noexcept -> CommandSessionState const &
{
    return state_.command_session;
}

auto SequencerSession::execute_command_string(std::string const &command_string,
                                              CommandContext const &context)
    -> CommandApplicationResult
{
    if (command_string.size() > 64 * 1'024)
    {
        return error_result("Command exceeds the permitted size.");
    }
    try
    {
        auto const parsed = parse_command_chain(command_string);
        if (parsed.size() == 1)
        {
            auto const &words = parsed.front().input.words;
            auto const word_is = [&words](std::size_t index, std::string_view value) {
                return index < words.size() && to_lower(words[index]) == value;
            };
            auto const require_revision = [&context]() -> ProjectRevision {
                if (!context.expected_project_revision.has_value())
                {
                    throw DocumentError{DocumentErrorCode::StaleProject,
                                        "Expected project revision is required."};
                }
                return *context.expected_project_revision;
            };
            auto result = std::optional<DocumentOperationResult>{};
            auto message = std::string{};
            if (words.size() == 2 && word_is(0, "project") && word_is(1, "new"))
            {
                result = create_project(require_revision(), false);
                message = "New Project";
            }
            else if (words.size() == 3 && word_is(0, "project") && word_is(1, "open"))
            {
                result = open_project(words[2], require_revision(), false);
                message = "Project Opened";
            }
            else if (words.size() == 2 && word_is(0, "project") && word_is(1, "save"))
            {
                result = save_project(require_revision());
                message = "Project Saved";
            }
            else if (words.size() == 4 && word_is(0, "project") && word_is(1, "save") &&
                     word_is(2, "as"))
            {
                result = save_project_as(words[3], require_revision(), std::nullopt);
                message = "Project Saved";
            }
            else if (words.size() == 3 && word_is(0, "load") && word_is(1, "cell"))
            {
                result = import_cell(words[2], require_revision(), context.cursor);
                message = "Cell Loaded";
            }
            else if (words.size() == 3 && word_is(0, "save") && word_is(1, "cell"))
            {
                if (!context.selection.has_value())
                {
                    throw DocumentError{DocumentErrorCode::InvalidDocument,
                                        "Cell selection is required."};
                }
                result = save_cell(words[2], require_revision(), context.cursor,
                                   *context.selection, std::nullopt);
                message = "Cell Saved";
            }
            if (result.has_value())
            {
                return {
                    .status = {MessageLevel::Info, std::move(message)},
                    .suggested_selection = std::move(result->suggested_selection),
                };
            }
        }
        else
        {
            auto const contains_document_command =
                std::ranges::any_of(parsed, [](CommandInvocation const &invocation) {
                    auto const &words = invocation.input.words;
                    if (words.size() < 2)
                    {
                        return false;
                    }
                    auto const first = to_lower(words[0]);
                    auto const second = to_lower(words[1]);
                    return first == "project" ||
                           ((first == "load" || first == "save") && second == "cell");
                });
            if (contains_document_command)
            {
                return error_result(
                    "Project and Cell document commands must be submitted alone.");
            }
        }
        auto expanded = std::vector<CommandInvocation>{};
        for (auto const &invocation : parsed)
        {
            auto const result = command_catalog_.bind_invocation(invocation);
            if (!result.has_value())
            {
                return error_result(result.error().message);
            }
            auto const &step = result.value();
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
        if (!bind_result.has_value())
        {
            return error_result(bind_result.error().message);
        }
        auto const &steps = bind_result.value();

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
        auto const project_mutating =
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
                            return typed_step.policy.project ==
                                       ProjectOperation::Edit ||
                                   typed_step.policy.project ==
                                       ProjectOperation::ReplaceHistory ||
                                   typed_step.policy.project ==
                                       ProjectOperation::NavigateHistory;
                        }
                    },
                    step);
            });
        auto const workspace_mutating =
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
                            return typed_step.policy.workspace ==
                                   WorkspaceAccess::Mutate;
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

        auto const preview_submission = context.preview_id.has_value();
        if (preview_submission)
        {
            if (!active_preview_.has_value() ||
                active_preview_->id != *context.preview_id)
            {
                return error_result("Unknown project preview.");
            }
            if (active_preview_->kind != ActivePreview::Kind::Command)
            {
                return error_result(
                    "Modulation previews accept only modulation updates.");
            }
            auto const preview_safe =
                !steps.empty() && std::ranges::all_of(steps, [](BoundStep const &step) {
                    auto const *command = std::get_if<ExecutableCommand>(&step);
                    return command != nullptr &&
                           command->policy.project == ProjectOperation::Edit &&
                           command->policy.library != LibraryAccess::Mutate &&
                           command->policy.workspace != WorkspaceAccess::Mutate &&
                           command->policy.files != FileAccess::Write &&
                           command->policy.copy_buffer != CopyBufferAccess::Write;
                });
            if (!preview_safe)
            {
                return error_result(
                    "Project previews accept only reversible project-edit commands.");
            }
        }
        else if (active_preview_.has_value() && project_mutating)
        {
            return error_result(
                "A project preview is active; commit or cancel it before editing.");
        }
        if ((project_mutating || workspace_mutating) && state_.recovery.has_value())
        {
            return error_result(
                "Restore or discard the pending recovery before editing.");
        }

        auto const history_count =
            std::ranges::count_if(steps, [](BoundStep const &step) {
                return std::holds_alternative<ExecutableHistoryNavigation>(step);
            });
        if (history_count > 0 && steps.size() != 1)
        {
            return error_result("undo and redo must be submitted alone.");
        }
        auto const replacement_count =
            std::ranges::count_if(steps, [](BoundStep const &step) {
                auto const *command = std::get_if<ExecutableCommand>(&step);
                return command != nullptr &&
                       command->policy.project == ProjectOperation::ReplaceHistory;
            });
        if (replacement_count > 0 && steps.size() != 1)
        {
            return error_result(
                "Project history replacement commands must be submitted alone.");
        }

        auto transaction = CommandTransaction{state_, effect_failure_};
        if (history_count == 1)
        {
            transaction.invalidate_transform_sessions();
        }
        auto execution_context = CommandExecutionContext{
            .selection = context.selection,
            .cursor = context.cursor,
        };
        auto result = CommandApplicationResult{};
        auto const initial_revision = state_.timeline.get_project_revision();
        auto const initial_content_directory = state_.workspace.content_directory;

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
                    transaction.project(), execution_context.cursor);
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

            if (command.policy.project == ProjectOperation::ReplaceHistory)
            {
                transaction.plan_history(HistoryPlan{.kind = HistoryPlanKind::Replace});
                transaction.clear_project_sessions();
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

        if (!preview_submission && steps.size() != 1 &&
            transaction.history_plan_is_amend())
        {
            transaction.plan_history(HistoryPlan{.kind = HistoryPlanKind::Commit});
        }
        if (!preview_submission && history_count == 0 &&
            transaction.project_changed() && !transaction.has_history_plan())
        {
            transaction.plan_history(HistoryPlan{.kind = HistoryPlanKind::Commit});
        }

        auto const project_changed = transaction.project_changed();
        auto project_digest = std::optional<std::string>{};
        if (project_changed)
        {
            project_digest = text_revision(serialize_project(transaction.project()));
        }
        if (transaction.workspace_changed() &&
            transaction.workspace().content_directory != initial_content_directory)
        {
            recover_content_directory(transaction.workspace().content_directory);
        }
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
        auto const final_revision = state_.timeline.get_project_revision();
        auto state_revision_advanced = false;
        if (final_revision != initial_revision)
        {
            advance_state_revision();
            state_revision_advanced = true;
            if (!preview_submission)
            {
                if (project_digest.has_value())
                {
                    refresh_document_dirty(*project_digest);
                }
                else
                {
                    refresh_document_dirty(
                        text_revision(serialize_project(state_.timeline.get_state())));
                }
                schedule_recovery();
            }
        }
        if (state_.workspace.content_directory != initial_content_directory)
        {
            state_.document.relative_path.reset();
            state_.document.file_revision.reset();
            state_.document.saved_project_digest.reset();
            state_.document.dirty = true;
            clear_recovery();
            schedule_recovery();
            if (!state_revision_advanced)
            {
                advance_state_revision();
            }
        }
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
        return result;
    }
    catch (nlohmann::json::exception const &e)
    {
        log_json_command_exception(command_string, context, e);
        return error_result(e.what());
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

auto SequencerSession::create_project(ProjectRevision expected_revision,
                                      bool discard_unsaved) -> DocumentOperationResult
{
    require_document_operation(expected_revision, discard_unsaved);
    auto project = ProjectState{};
    state_.timeline.replace_history(project);
    state_.command_session = CommandSessionState{};
    state_.document = ProjectDocumentState{
        .saved_project_digest = text_revision(serialize_project(project)),
        .dirty = false,
    };
    clear_recovery();
    advance_state_revision();
    return {.snapshot = project_snapshot(), .suggested_selection = SelectionPath{}};
}

auto SequencerSession::open_project(std::string relative_path,
                                    ProjectRevision expected_revision,
                                    bool discard_unsaved) -> DocumentOperationResult
{
    require_document_operation(expected_revision, discard_unsaved);
    auto const stored =
        read_content_file(state_.workspace.content_directory, relative_path, ".xenproj",
                          MAX_PROJECT_FILE_BYTES);
    auto project = ProjectState{};
    try
    {
        project = deserialize_project(stored.text);
    }
    catch (std::exception const &error)
    {
        throw DocumentError{DocumentErrorCode::InvalidDocument, error.what()};
    }
    auto const digest = text_revision(serialize_project(project));
    state_.timeline.replace_history(std::move(project));
    state_.command_session = CommandSessionState{};
    state_.document = ProjectDocumentState{
        .relative_path = stored.file.relative_path,
        .file_revision = stored.file.file_revision,
        .saved_project_digest = digest,
        .dirty = false,
    };
    clear_recovery();
    advance_state_revision();
    return {.snapshot = project_snapshot(),
            .file = stored.file,
            .suggested_selection = SelectionPath{}};
}

auto SequencerSession::save_project(ProjectRevision expected_revision)
    -> DocumentOperationResult
{
    require_document_operation(expected_revision, true);
    if (state_.recovery.has_value())
    {
        throw DocumentError{DocumentErrorCode::RecoveryConflict,
                            "A newer recovery snapshot must be restored or discarded."};
    }
    if (!state_.document.relative_path.has_value())
    {
        throw DocumentError{
            DocumentErrorCode::ProjectPathRequired,
            "Project must be named with Save As before it can be saved."};
    }
    auto text = std::string{};
    try
    {
        text = serialize_project(state_.timeline.get_state());
    }
    catch (std::length_error const &error)
    {
        throw DocumentError{DocumentErrorCode::FileTooLarge, error.what()};
    }
    auto const file = write_content_file(
        state_.workspace.content_directory, *state_.document.relative_path, ".xenproj",
        text, MAX_PROJECT_FILE_BYTES, state_.document.file_revision);
    state_.document.file_revision = file.file_revision;
    state_.document.saved_project_digest = text_revision(text);
    state_.document.dirty = false;
    clear_recovery();
    state_.library_revision = detail::allocate_library_revision();
    advance_state_revision();
    return {.snapshot = project_snapshot(), .file = file};
}

auto SequencerSession::save_project_as(
    std::string relative_path, ProjectRevision expected_revision,
    std::optional<std::string> expected_file_revision) -> DocumentOperationResult
{
    require_document_operation(expected_revision, true);
    if (state_.recovery.has_value())
    {
        throw DocumentError{DocumentErrorCode::RecoveryConflict,
                            "A newer recovery snapshot must be restored or discarded."};
    }
    auto text = std::string{};
    try
    {
        text = serialize_project(state_.timeline.get_state());
    }
    catch (std::length_error const &error)
    {
        throw DocumentError{DocumentErrorCode::FileTooLarge, error.what()};
    }
    auto const file = write_content_file(
        state_.workspace.content_directory, relative_path, ".xenproj", text,
        MAX_PROJECT_FILE_BYTES, expected_file_revision);
    state_.document = ProjectDocumentState{
        .relative_path = file.relative_path,
        .file_revision = file.file_revision,
        .saved_project_digest = text_revision(text),
        .dirty = false,
    };
    clear_recovery();
    state_.library_revision = detail::allocate_library_revision();
    advance_state_revision();
    return {.snapshot = project_snapshot(), .file = file};
}

auto SequencerSession::restore_recovery(std::string recovery_revision,
                                        ProjectRevision expected_revision,
                                        bool discard_unsaved) -> DocumentOperationResult
{
    require_document_operation(expected_revision, discard_unsaved, false);
    if (!recovery_candidate_.has_value() || !state_.recovery.has_value() ||
        state_.recovery->revision != recovery_revision)
    {
        throw DocumentError{DocumentErrorCode::RecoveryConflict,
                            "Recovery snapshot is no longer available."};
    }
    detail::reserve_project_revision(recovery_candidate_->project_revision);
    detail::reserve_state_revision(recovery_candidate_->state_revision);
    state_.timeline.replace_history(recovery_candidate_->project);
    state_.document = recovery_candidate_->document;
    state_.document.dirty = true;
    state_.command_session = CommandSessionState{};
    state_.recovery.reset();
    advance_state_revision();
    schedule_recovery();
    return {.snapshot = project_snapshot(), .suggested_selection = SelectionPath{}};
}

auto SequencerSession::discard_recovery(std::string recovery_revision)
    -> DocumentOperationResult
{
    if (active_preview_.has_value())
    {
        throw DocumentError{DocumentErrorCode::PreviewActive,
                            "Finish the active preview before discarding recovery."};
    }
    if (!state_.recovery.has_value() || state_.recovery->revision != recovery_revision)
    {
        throw DocumentError{DocumentErrorCode::RecoveryConflict,
                            "Recovery snapshot is no longer available."};
    }
    if (!recovery_file_.empty())
    {
        auto error = std::error_code{};
        (void)std::filesystem::remove(recovery_file_, error);
        if (error)
        {
            throw DocumentError{DocumentErrorCode::Io,
                                "Unable to delete the recovery snapshot."};
        }
    }
    clear_recovery();
    advance_state_revision();
    return {.snapshot = project_snapshot()};
}

auto SequencerSession::import_cell(std::string relative_path,
                                   ProjectRevision expected_revision,
                                   CompositionCursor cursor) -> DocumentOperationResult
{
    require_document_operation(expected_revision, true);
    if (state_.recovery.has_value())
    {
        throw DocumentError{DocumentErrorCode::RecoveryConflict,
                            "A newer recovery snapshot must be restored or discarded."};
    }
    auto const stored =
        read_content_file(state_.workspace.content_directory, relative_path, ".xencell",
                          MAX_CELL_FILE_BYTES);
    auto cell = sequence::Cell{};
    try
    {
        cell = deserialize_cell_file(stored.text);
    }
    catch (std::exception const &error)
    {
        throw DocumentError{DocumentErrorCode::InvalidDocument, error.what()};
    }
    auto project = state_.timeline.get_state();
    auto project_digest = std::string{};
    try
    {
        auto const name = std::filesystem::path{relative_path}.stem().string();
        auto const id = create_sequence(project.sequence_bank, std::move(cell));
        project.sequence_bank.sequences.back().name = name;
        assign_sequence_reference(project.composition, cursor.row_coordinate,
                                  cursor.column_coordinate, id);
        project_digest = text_revision(serialize_project(project));
    }
    catch (std::length_error const &error)
    {
        throw DocumentError{DocumentErrorCode::FileTooLarge, error.what()};
    }
    catch (std::exception const &error)
    {
        throw DocumentError{DocumentErrorCode::InvalidDocument, error.what()};
    }
    (void)state_.timeline.commit(std::move(project));
    state_.command_session = CommandSessionState{};
    refresh_document_dirty(project_digest);
    schedule_recovery();
    advance_state_revision();
    return {.snapshot = project_snapshot(),
            .file = stored.file,
            .suggested_selection = SelectionPath{}};
}

auto SequencerSession::save_cell(std::string relative_path,
                                 ProjectRevision expected_revision,
                                 CompositionCursor cursor, SelectionPath selection,
                                 std::optional<std::string> expected_file_revision)
    -> DocumentOperationResult
{
    require_document_operation(expected_revision, true);
    sequence::Cell const *cell = nullptr;
    try
    {
        cell = &get_selected_cell_const(
            selected_sequence(state_.timeline.get_state(), cursor), selection);
    }
    catch (std::exception const &error)
    {
        throw DocumentError{DocumentErrorCode::InvalidDocument, error.what()};
    }
    auto text = std::string{};
    try
    {
        text = serialize_cell_file(*cell);
    }
    catch (std::length_error const &error)
    {
        throw DocumentError{DocumentErrorCode::FileTooLarge, error.what()};
    }
    catch (std::exception const &error)
    {
        throw DocumentError{DocumentErrorCode::InvalidDocument, error.what()};
    }
    auto const file = write_content_file(state_.workspace.content_directory,
                                         relative_path, ".xencell", text,
                                         MAX_CELL_FILE_BYTES, expected_file_revision);
    state_.library_revision = detail::allocate_library_revision();
    return {
        .snapshot = project_snapshot(), .file = file, .suggested_selection = selection};
}

void SequencerSession::require_document_operation(
    ProjectRevision expected_revision, bool discard_unsaved,
    bool pending_recovery_requires_discard) const
{
    if (active_preview_.has_value())
    {
        throw DocumentError{DocumentErrorCode::PreviewActive,
                            "Finish the active preview before changing documents."};
    }
    auto const current = state_.timeline.get_project_revision();
    if (current != expected_revision)
    {
        throw DocumentError{DocumentErrorCode::StaleProject,
                            "Project changed before the document operation completed."};
    }
    if ((state_.document.dirty ||
         (pending_recovery_requires_discard && state_.recovery.has_value())) &&
        !discard_unsaved)
    {
        throw DocumentError{DocumentErrorCode::UnsavedChanges,
                            "Project has unsaved changes."};
    }
}

void SequencerSession::advance_state_revision()
{
    state_.state_revision = detail::allocate_state_revision();
}

void SequencerSession::refresh_document_dirty(std::string const &project_digest)
{
    state_.document.dirty = !state_.document.saved_project_digest.has_value() ||
                            *state_.document.saved_project_digest != project_digest;
}

void SequencerSession::schedule_recovery()
{
    if (!state_.document.dirty || recovery_file_.empty())
    {
        if (!state_.document.dirty)
        {
            clear_recovery();
        }
        return;
    }
    if (state_.recovery.has_value())
    {
        return;
    }
    if (!recovery_due_unix_ms_.has_value())
    {
        recovery_due_unix_ms_ = now_unix_ms() + 2000;
    }
}

void SequencerSession::clear_recovery()
{
    if (!recovery_file_.empty())
    {
        auto error = std::error_code{};
        (void)std::filesystem::remove(recovery_file_, error);
        if (error)
        {
            juce::Logger::writeToLog("XenSequencer recovery cleanup error: " +
                                     juce::String{error.message()});
        }
    }
    recovery_candidate_.reset();
    recovery_due_unix_ms_.reset();
    state_.recovery.reset();
}

void SequencerSession::configure_recovery(
    std::optional<StateRevision> baseline_revision)
{
    auto const previous_recovery = state_.recovery;
    recovery_candidate_.reset();
    recovery_due_unix_ms_.reset();
    state_.recovery.reset();
    recovery_file_.clear();
    if (instance_binding_.session_id.empty())
    {
        if (previous_recovery.has_value())
        {
            advance_state_revision();
        }
        return;
    }
    auto const directory = get_user_settings_directory().getChildFile("recovery");
    recovery_file_ = std::filesystem::path{
        directory
            .getChildFile(text_revision(instance_binding_.session_id).substr(7) +
                          ".xenrecovery")
            .getFullPathName()
            .toStdString()};
    recover_recovery_file(recovery_file_, instance_binding_.session_id);
    try
    {
        auto const text = read_text_file(recovery_file_, MAX_PERSISTED_STATE_BYTES);
        if (!text.has_value())
        {
            schedule_recovery();
            if (previous_recovery.has_value())
            {
                advance_state_revision();
            }
            return;
        }
        auto candidate = deserialize_recovery_state(*text);
        if (candidate.session_id != instance_binding_.session_id)
        {
            throw std::invalid_argument{"Recovery session ID does not match."};
        }
        detail::reserve_project_revision(candidate.project_revision);
        detail::reserve_state_revision(candidate.state_revision);
        auto const newer_than_baseline = !baseline_revision.has_value() ||
                                         candidate.state_revision > *baseline_revision;
        auto const differs_from_baseline =
            !baseline_revision.has_value() ||
            candidate.project != state_.timeline.get_state() ||
            candidate.document != state_.document;
        if (newer_than_baseline && differs_from_baseline)
        {
            auto const revision = text_revision(*text);
            state_.recovery = RecoveryMetadata{
                .revision = revision,
                .saved_at_unix_ms = candidate.saved_at_unix_ms,
                .relative_path = candidate.document.relative_path,
                .project_revision = candidate.project_revision,
            };
            recovery_candidate_ = std::move(candidate);
            if (previous_recovery != state_.recovery)
            {
                advance_state_revision();
            }
        }
        else
        {
            clear_recovery();
            schedule_recovery();
            if (previous_recovery.has_value())
            {
                advance_state_revision();
            }
        }
    }
    catch (std::exception const &error)
    {
        juce::Logger::writeToLog("XenSequencer recovery load error: " +
                                 juce::String{error.what()});
        auto remove_error = std::error_code{};
        (void)std::filesystem::remove(recovery_file_, remove_error);
        recovery_candidate_.reset();
        state_.recovery.reset();
        schedule_recovery();
        if (previous_recovery.has_value())
        {
            advance_state_revision();
        }
    }
}

void SequencerSession::perform_recovery_maintenance(std::uint64_t now, bool force)
{
    if (!state_.document.dirty || recovery_file_.empty() ||
        state_.recovery.has_value() || !recovery_due_unix_ms_.has_value())
    {
        return;
    }
    if (!force && now < *recovery_due_unix_ms_)
    {
        return;
    }
    auto const previous_state_revision = state_.state_revision;
    advance_state_revision();
    auto const &persistent_project = active_preview_.has_value()
                                         ? active_preview_->baseline.project
                                         : state_.timeline.get_state();
    auto const persistent_project_revision =
        active_preview_.has_value() ? active_preview_->baseline.project_revision
                                    : state_.timeline.get_project_revision();
    auto persisted = PersistedRecoveryState{
        .session_id = instance_binding_.session_id,
        .project = persistent_project,
        .project_revision = persistent_project_revision,
        .state_revision = state_.state_revision,
        .document = state_.document,
        .saved_at_unix_ms = now,
    };
    auto text = std::string{};
    try
    {
        text = serialize_recovery_state(persisted);
        atomic_write_text_file(recovery_file_, text);
    }
    catch (...)
    {
        state_.state_revision = previous_state_revision;
        recovery_due_unix_ms_ = now + 2000;
        throw;
    }
    if (active_preview_.has_value())
    {
        active_preview_->baseline.state_revision = state_.state_revision;
        active_preview_->baseline.recovery.reset();
    }
    recovery_candidate_ = std::move(persisted);
    recovery_due_unix_ms_.reset();
}

void SequencerSession::replace_project_history(ProjectState state)
{
    auto const project_digest = text_revision(serialize_project(state));
    active_preview_.reset();
    state_.timeline.replace_history(std::move(state));
    state_.command_session = CommandSessionState{};
    refresh_document_dirty(project_digest);
    schedule_recovery();
    advance_state_revision();
}

void SequencerSession::replace_library(ContentLibrary library)
{
    state_.library = std::move(library);
    state_.library_revision = detail::allocate_library_revision();
}

void SequencerSession::replace_instance_binding(InstanceBinding binding)
{
    validate_binding(binding);
    auto const session_changed = instance_binding_.session_id != binding.session_id;
    auto const baseline_revision = instance_binding_.session_id.empty()
                                       ? std::optional<StateRevision>{}
                                       : std::optional{state_.state_revision};
    instance_binding_ = std::move(binding);
    if (session_changed)
    {
        configure_recovery(baseline_revision);
    }
}

void SequencerSession::replace_project_history_and_binding(ProjectState state,
                                                           InstanceBinding binding)
{
    validate_binding(binding);
    auto const project_digest = text_revision(serialize_project(state));
    auto const session_changed = instance_binding_.session_id != binding.session_id;
    instance_binding_ = std::move(binding);
    active_preview_.reset();
    state_.timeline.replace_history(std::move(state));
    state_.command_session = CommandSessionState{};
    refresh_document_dirty(project_digest);
    schedule_recovery();
    advance_state_revision();
    if (session_changed)
    {
        configure_recovery(state_.state_revision);
    }
}

void SequencerSession::restore_persisted_state(PersistedProcessorState state)
{
    validate_persisted_processor_state(state);
    auto const project_digest = text_revision(serialize_project(state.project));
    auto const previous_state_revision = state_.state_revision;
    auto const restores_current_snapshot =
        state.saved_project_revision == state_.timeline.get_project_revision() &&
        state.saved_state_revision == previous_state_revision &&
        state.project == state_.timeline.get_state() &&
        state.document == state_.document;
    detail::reserve_project_revision(state.saved_project_revision);
    detail::reserve_state_revision(state.saved_state_revision);
    instance_binding_ = std::move(state.binding);
    active_preview_.reset();
    state_.timeline.restore_history(std::move(state.project),
                                    state.saved_project_revision);
    state_.document = std::move(state.document);
    refresh_document_dirty(project_digest);
    if (state.saved_state_revision > previous_state_revision)
    {
        state_.state_revision = state.saved_state_revision;
    }
    else if (!restores_current_snapshot)
    {
        advance_state_revision();
    }
    state_.command_session = CommandSessionState{};
    configure_recovery(state.saved_state_revision);
}

void SequencerSession::set_channel_id(ChannelId channel_id)
{
    if (channel_id.empty())
    {
        throw std::invalid_argument{"Instance channel ID must not be empty."};
    }
    if (channel_id.size() > MAX_PERSISTED_STRING_BYTES)
    {
        throw std::invalid_argument{"Instance channel ID is too long."};
    }
    instance_binding_.channel_id = std::move(channel_id);
}

} // namespace xen
