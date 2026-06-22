#include <xen/xen_processor.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/command.hpp>
#include <xen/command_catalog.hpp>
#include <xen/command_transaction.hpp>
#include <xen/midi.hpp>
#include <xen/selection.hpp>
#include <xen/serialize.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/submission_effects.hpp>
#include <xen/user_directory.hpp>
#include <xen/utility.hpp>
#include <xen/xen_editor.hpp>

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

[[nodiscard]] auto valid_bpm(double value) -> bool
{
    return std::isfinite(value) &&
           value >= static_cast<double>(std::numeric_limits<float>::denorm_min()) &&
           value <= static_cast<double>(std::numeric_limits<float>::max());
}

[[nodiscard]] auto valid_sample_rate(double value) -> bool
{
    return std::isfinite(value) && value >= 1.0 &&
           value <= static_cast<double>(std::numeric_limits<std::uint32_t>::max());
}

[[nodiscard]] auto ppq_to_samples(double ppq, float bpm, std::uint32_t sample_rate)
    -> xen::SampleIndex
{
    if (!std::isfinite(ppq) || ppq < 0.0)
    {
        return 0;
    }

    auto const samples = static_cast<long double>(ppq) * 60.0L /
                         static_cast<long double>(bpm) *
                         static_cast<long double>(sample_rate);
    if (!std::isfinite(samples) || samples < 0.0L ||
        samples >
            static_cast<long double>(std::numeric_limits<xen::SampleIndex>::max()))
    {
        return 0;
    }
    return static_cast<xen::SampleIndex>(samples);
}

} // namespace

namespace xen
{

XenProcessor::XenProcessor(SubmissionEffects::FailurePoint effect_failure)
    : plugin_state{.timeline = XenTimeline{EngineState{}}},
      command_catalog_{create_command_catalog()}, effect_failure_{effect_failure}
{
    initialize_demo_files();

    // Send initial state to Audio Thread
    pending_engine_state_update.publish(plugin_state.timeline.get_state());
    notify_ui_state_changed();

    this->execute_command_string("load scales", CommandContext{});
    this->execute_command_string("load chords", CommandContext{});
}

auto XenProcessor::command_catalog() const noexcept -> CommandCatalog const &
{
    return command_catalog_;
}

auto XenProcessor::get_engine_snapshot() const -> EngineSnapshot
{
    return EngineSnapshot{
        .engine = plugin_state.timeline.get_state(),
        .history_entry_id = plugin_state.timeline.get_current_entry_id(),
        .project_revision = plugin_state.timeline.get_project_revision(),
        .snapshot_version = ui_snapshot_version_.load(std::memory_order_acquire),
    };
}

auto XenProcessor::get_ui_snapshot_version() const noexcept -> std::uint64_t
{
    return ui_snapshot_version_.load(std::memory_order_acquire);
}

void XenProcessor::notify_ui_state_changed() noexcept
{
    ui_snapshot_version_.fetch_add(1, std::memory_order_release);
}

void XenProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                juce::MidiBuffer &midi_buffer)
{
    buffer.clear();

    bool update_needed = false;
    auto transport_offset = SampleIndex{0};

    { // Update DAWState
        auto bpm =
            audio_thread_state_.daw.bpm > 0.f ? audio_thread_state_.daw.bpm : 120.f;
        auto is_playing = false;
        if (auto *playhead = this->getPlayHead(); playhead != nullptr)
        {
            auto const position = playhead->getPosition();
            if (position.hasValue())
            {
                if (auto const bpm_opt = position->getBpm(); bpm_opt)
                {
                    if (valid_bpm(*bpm_opt))
                    {
                        bpm = static_cast<float>(*bpm_opt);
                    }
                }
                is_playing = position->getIsPlaying();
            }
        }

        auto sample_rate = audio_thread_state_.daw.sample_rate > 0
                               ? audio_thread_state_.daw.sample_rate
                               : std::uint32_t{44'100};
        if (valid_sample_rate(this->getSampleRate()))
        {
            sample_rate = static_cast<std::uint32_t>(this->getSampleRate());
        }

        update_needed = !utility::compare_within_tolerance(audio_thread_state_.daw.bpm,
                                                           bpm, 0.0001f) ||
                        audio_thread_state_.daw.sample_rate != sample_rate ||
                        audio_thread_state_.daw.is_playing != is_playing;

        if (auto *playhead = this->getPlayHead(); playhead != nullptr)
        {
            auto const position = playhead->getPosition();
            if (position.hasValue())
            {
                if (auto const samples_opt = position->getTimeInSamples();
                    samples_opt.hasValue())
                {
                    if (*samples_opt >= 0)
                    {
                        transport_offset = static_cast<SampleIndex>(*samples_opt);
                    }
                }
                else if (auto const ppq_opt = position->getPpqPosition();
                         ppq_opt.hasValue() && bpm > 0.f && sample_rate > 0)
                {
                    transport_offset = ppq_to_samples(*ppq_opt, bpm, sample_rate);
                }
            }
        }

        audio_thread_state_.daw = DAWState{
            .bpm = bpm,
            .sample_rate = sample_rate,
            .is_playing = is_playing,
        };
    }

    if (auto const snapshot = pending_engine_state_update.try_consume_latest())
    {
        audio_thread_state_.sequencer = &snapshot->state();
        update_needed = true;
    }

    if (update_needed && audio_thread_state_.sequencer != nullptr)
    {
        audio_thread_state_.midi_engine.update(*audio_thread_state_.sequencer,
                                               audio_thread_state_.daw);
    }

    // Calculate MIDI buffer slice
    auto const block_size = buffer.getNumSamples();
    auto const sample_count =
        block_size >= 0 ? static_cast<SampleCount>(block_size) : SampleCount{0};
    auto next_slice = audio_thread_state_.midi_engine.step(
        midi_buffer, transport_offset, sample_count, audio_thread_state_.daw);

    midi_buffer.swapWith(next_slice);

    audio_thread_state_for_gui.write({
        .daw = audio_thread_state_.daw,
        .loop_phase = audio_thread_state_.midi_engine.get_loop_phase(
            transport_offset, audio_thread_state_.daw),
        .transport_active = audio_thread_state_.daw.is_playing,
    });
}

void XenProcessor::processBlock(juce::AudioBuffer<double> &buffer,
                                juce::MidiBuffer &midi_buffer)
{
    // Just forward to float version, this is a midi-only plugin.
    buffer.clear();
    auto empty = juce::AudioBuffer<float>{};
    this->processBlock(empty, midi_buffer);
}

auto XenProcessor::createEditor() -> juce::AudioProcessorEditor *
{
    return new gui::XenEditor{*this, editor_width, editor_height};
}

void XenProcessor::getStateInformation(juce::MemoryBlock &dest_data)
{
    try
    {
        auto const json_str = serialize_plugin(plugin_state.timeline.get_state());
        dest_data.setSize(json_str.size());
        std::memcpy(dest_data.getData(), json_str.data(), json_str.size());
    }
    catch (std::exception const &e)
    {
        juce::Logger::writeToLog("XenSequencer getStateInformation error: " +
                                 juce::String{e.what()});
        dest_data.setSize(0);
    }
}

void XenProcessor::setStateInformation(void const *data, int sizeInBytes)
{
    try
    {
        auto const json_str =
            std::string(static_cast<char const *>(data), (std::size_t)sizeInBytes);
        auto state = deserialize_plugin(json_str);
        plugin_state.timeline.replace_history(std::move(state));
        pending_engine_state_update.publish(plugin_state.timeline.get_state());
        notify_ui_state_changed();
    }
    catch (std::exception const &e)
    {
        juce::Logger::writeToLog("XenSequencer setStateInformation error: " +
                                 juce::String{e.what()});
    }
}

auto XenProcessor::execute_command_string(std::string const &command_string,
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
                if (previous_command_chain_.empty())
                {
                    return error_result("No previous command to repeat.");
                }
                expanded.insert(expanded.end(), previous_command_chain_.begin(),
                                previous_command_chain_.end());
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
        auto const current_revision = plugin_state.timeline.get_project_revision();
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

        auto transaction = CommandTransaction{plugin_state, effect_failure_};
        auto execution_context =
            CommandExecutionContext{.selection = context.selection};
        auto result = CommandApplicationResult{};
        auto const initial_engine = plugin_state.timeline.get_state();
        auto const initial_revision = plugin_state.timeline.get_project_revision();

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
                    transaction.project().measure);
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
                transaction.library_changed())
            {
                transaction.invalidate_transform_sessions();
            }
            if (changed &&
                command.policy.repeat == RepeatPolicy::OnSuccessfulProjectChange)
            {
                transaction.record_repeat(command.invocation);
            }
        }

        if (history_count == 0 && transaction.project_changed())
        {
            transaction.plan_history(HistoryPlan{.kind = HistoryPlanKind::Commit});
        }

        auto const project_changed = transaction.project_changed();
        transaction.prepare();
        try
        {
            transaction.apply_effects();
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
            previous_command_chain_ = *transaction.repeat_candidate();
        }
        auto const &final_engine = plugin_state.timeline.get_state();
        auto const final_revision = plugin_state.timeline.get_project_revision();
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
            pending_engine_state_update.publish(final_engine);
        }
        if (!steps.empty())
        {
            notify_ui_state_changed();
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

void XenProcessor::prepareToPlay(double, int)
{
}

void XenProcessor::releaseResources()
{
}

auto XenProcessor::getName() const -> juce::String const
{
    return "XenSequencer";
}

auto XenProcessor::hasEditor() const -> bool
{
    return true;
}

auto XenProcessor::supportsMPE() const -> bool
{
    return true;
}

auto XenProcessor::acceptsMidi() const -> bool
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

auto XenProcessor::producesMidi() const -> bool
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

auto XenProcessor::isMidiEffect() const -> bool
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

auto XenProcessor::getTailLengthSeconds() const -> double
{
    return 0.;
}

auto XenProcessor::getNumPrograms() -> int
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0
              // programs, so this should be at least 1, even if you're not really
              // implementing programs.
}

auto XenProcessor::getCurrentProgram() -> int
{
    return 0;
}

void XenProcessor::setCurrentProgram(int)
{
}

auto XenProcessor::getProgramName(int index) -> juce::String const
{
    return "Program " + juce::String(index);
}

void XenProcessor::changeProgramName(int, const juce::String &)
{
}

} // namespace xen

// Definition is needed by JUCE
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new xen::XenProcessor{};
}
