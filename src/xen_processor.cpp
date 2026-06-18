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
#include <xen/midi.hpp>
#include <xen/serialize.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/submission_effects.hpp>
#include <xen/user_directory.hpp>
#include <xen/utility.hpp>
#include <xen/xen_editor.hpp>

namespace
{

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

    this->execute_command_string("load scales");
    this->execute_command_string("load chords");
}

auto XenProcessor::command_catalog() noexcept -> CommandCatalog &
{
    return command_catalog_;
}

auto XenProcessor::command_catalog() const noexcept -> CommandCatalog const &
{
    return command_catalog_;
}

auto XenProcessor::get_engine_snapshot() const -> EngineSnapshot
{
    return EngineSnapshot{
        .engine = plugin_state.timeline.get_state(),
        .editor = plugin_state.editor,
        .commit_id = plugin_state.timeline.get_current_commit_id(),
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
        plugin_state.timeline.stage(std::move(state));
        plugin_state.timeline.commit();
        pending_engine_state_update.publish(plugin_state.timeline.get_state());
        notify_ui_state_changed();
    }
    catch (std::exception const &e)
    {
        juce::Logger::writeToLog("XenSequencer setStateInformation error: " +
                                 juce::String{e.what()});
    }
}

auto XenProcessor::execute_command_string(std::string const &command_string)
    -> std::pair<MessageLevel, std::string>
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
                return {MessageLevel::Error,
                        std::get<CatalogBindError>(result).message};
            }
            auto const &step = std::get<BoundStep>(result);
            if (std::holds_alternative<RepeatPrevious>(step))
            {
                if (previous_command_chain_.empty())
                {
                    return {MessageLevel::Error, "No previous command to repeat."};
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
            return {MessageLevel::Error,
                    std::get<CatalogBindError>(bind_result).message};
        }
        auto const &steps = std::get<std::vector<BoundStep>>(bind_result);
        auto commands = std::vector<ExecutableCommand>{};
        commands.reserve(steps.size());
        for (auto const &step : steps)
        {
            if (std::holds_alternative<RepeatPrevious>(step))
            {
                return {MessageLevel::Error,
                        "Recursive 'again' expansion is not allowed."};
            }
            commands.push_back(std::get<ExecutableCommand>(step));
        }

        auto const history_count =
            std::ranges::count_if(commands, [](ExecutableCommand const &command) {
                return command.execution_role != ExecutionRole::Normal;
            });
        if (history_count > 0 && commands.size() != 1)
        {
            return {MessageLevel::Error, "undo and redo must be submitted alone."};
        }

        auto working = plugin_state;
        auto effects = SubmissionEffects{effect_failure_};
        auto status = std::pair<MessageLevel, std::string>{MessageLevel::Debug, ""};
        auto repeat_target = std::vector<CommandInvocation>{};
        auto const initial_engine = working.timeline.get_state();
        auto const initial_commit_id = working.timeline.get_current_commit_id();

        for (auto const &command : commands)
        {
            auto const before = working.timeline.get_state();
            status = command.execute(working, effects);
            if (status.first == MessageLevel::Error)
            {
                return status;
            }
            auto const changed = working.timeline.get_state() != before;
            if (changed && command.repeat_policy == RepeatPolicy::EngineEdit)
            {
                repeat_target.push_back(command.invocation);
            }
        }

        if (history_count == 0 && working.timeline.get_state() != initial_engine)
        {
            working.timeline.commit();
        }

        auto const final_engine = working.timeline.get_state();
        auto const final_commit_id = working.timeline.get_current_commit_id();
        effects.prepare();
        auto const original_state = plugin_state;
        try
        {
            effects.apply();
            plugin_state = std::move(working);
        }
        catch (std::exception const &e)
        {
            auto const rollback_failures = effects.rollback();
            auto state_rollback_failed = false;
            try
            {
                plugin_state = original_state;
            }
            catch (...)
            {
                state_rollback_failed = true;
            }
            auto message = std::string{e.what()};
            if (!rollback_failures.empty())
            {
                message += "; rollback failed for: " + rollback_failures;
            }
            if (state_rollback_failed)
            {
                message += "; backend state rollback failed";
            }
            return {MessageLevel::Error, std::move(message)};
        }

        effects.finalize();
        if (!repeat_target.empty())
        {
            previous_command_chain_ = std::move(repeat_target);
        }
        if (final_commit_id != initial_commit_id || final_engine != initial_engine)
        {
            pending_engine_state_update.publish(final_engine);
        }
        if (!commands.empty())
        {
            notify_ui_state_changed();
        }
        return status;
    }
    catch (std::exception const &e)
    {
        return {MessageLevel::Error, e.what()};
    }
    catch (...)
    {
        return {MessageLevel::Error, "Unknown error"};
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
