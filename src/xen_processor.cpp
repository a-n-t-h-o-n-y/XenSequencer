#include <xen/xen_processor.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/command.hpp>
#include <xen/midi.hpp>
#include <xen/serialize.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/utility.hpp>
#include <xen/command_catalog.hpp>
#include <xen/xen_editor.hpp>

namespace xen
{

XenProcessor::XenProcessor()
    : plugin_state{.timeline = XenTimeline{{.sequencer = {}, .aux = {}}}}
{
    initialize_demo_files();

    // Send initial state to Audio Thread
    pending_engine_state_update.publish(plugin_state.timeline.get_state().sequencer);
    notify_ui_state_changed();

    this->execute_command_string("load scales");
    this->execute_command_string("load chords");
}

auto XenProcessor::get_engine_snapshot() const -> EngineSnapshot
{
    auto const tracked = plugin_state.timeline.get_state();
    return EngineSnapshot{
        .engine = tracked.sequencer,
        .editor = tracked.aux,
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
        auto bpm = audio_thread_state_.daw.bpm > 0.f ? audio_thread_state_.daw.bpm
                                                      : 120.f;
        auto is_playing = false;
        if (auto *playhead = this->getPlayHead(); playhead != nullptr)
        {
            auto const position = playhead->getPosition();
            if (position.hasValue())
            {
                if (auto const bpm_opt = position->getBpm(); bpm_opt)
                {
                    bpm = static_cast<float>(*bpm_opt);
                }
                is_playing = position->getIsPlaying();
            }
        }

        auto sample_rate = static_cast<std::uint32_t>(this->getSampleRate());
        if (sample_rate == 0)
        {
            sample_rate = audio_thread_state_.daw.sample_rate > 0
                              ? audio_thread_state_.daw.sample_rate
                              : 44'100;
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
                    transport_offset =
                        (SampleIndex)std::max<std::int64_t>(*samples_opt, 0);
                }
                else if (auto const ppq_opt = position->getPpqPosition();
                         ppq_opt.hasValue() && bpm > 0.f && sample_rate > 0)
                {
                    auto const samples =
                        *ppq_opt * (60.0 / (double)bpm) * (double)sample_rate;
                    transport_offset = (SampleIndex)std::max<double>(samples, 0.0);
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
    auto next_slice = audio_thread_state_.midi_engine.step(
        midi_buffer, transport_offset,
        (SampleCount)buffer.getNumSamples(), audio_thread_state_.daw);

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
        auto const json_str =
            serialize_plugin(plugin_state.timeline.get_state().sequencer);
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
        plugin_state.timeline.stage({std::move(state), {}});
        plugin_state.timeline.commit();
        pending_engine_state_update.publish(plugin_state.timeline.get_state().sequencer);
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
        auto &ps = plugin_state;
        try
        {
            auto status = std::pair<MessageLevel, std::string>{MessageLevel::Debug, ""};
            auto executed_any_action = false;
            auto auto_commit_candidate = false;
            auto force_commit_requested = false;
            auto context = ExecutionContext{ps.timeline.get_state().aux};
            auto executed_chain = std::vector<CommandAction>{};
            auto const invocations = parse_command_chain(command_string);
            auto expansion_count = std::size_t{0};

            auto const apply_action_result = [&](CommandActionResult const &action_result) {
                status = action_result.status;

                if (action_result.commit_intent == CommitIntent::Force)
                {
                    force_commit_requested = true;
                }
                if (action_result.engine_mutated &&
                    action_result.commit_intent != CommitIntent::Defer)
                {
                    auto_commit_candidate = true;
                }

                context = action_result.context;
            };

            auto const apply_action = [&](CommandAction const &action) {
                executed_any_action = true;
                executed_chain.push_back(action);

                auto const action_result = execute_command_action(ps, context, action);
                apply_action_result(action_result);
            };

            for (auto const &invocation : invocations)
            {
                auto const bound_result = bind_invocation(invocation);
                if (std::holds_alternative<CatalogBindError>(bound_result))
                {
                    auto const &bind_error =
                        std::get<CatalogBindError>(bound_result);
                    if (bind_error.kind == CatalogBindErrorKind::UnknownCommand)
                    {
                        if (!bind_error.token.empty())
                        {
                            status = {MessageLevel::Error,
                                      "Command not found: " +
                                          bind_error.token};
                        }
                        else
                        {
                            status = {MessageLevel::Error, "Command not found"};
                        }
                    }
                    else
                    {
                        status = {MessageLevel::Error, bind_error.message};
                    }
                    break;
                }

                auto const &typed_action =
                    std::get<BoundCommand>(bound_result).action;

                if (is_again_action(typed_action))
                {
                    if (previous_action_chain_.empty())
                    {
                        status = {MessageLevel::Error,
                                  "No previous command to repeat."};
                        break;
                    }

                    if (++expansion_count > 64)
                    {
                        status = {MessageLevel::Error,
                                  "Recursive 'again' expansion exceeded safe "
                                  "limit."};
                        break;
                    }

                    auto const replay_chain = previous_action_chain_;
                    for (auto const &replay_action : replay_chain)
                    {
                        apply_action(replay_action);
                        if (status.first == MessageLevel::Error)
                        {
                            break;
                        }
                    }
                }
                else
                {
                    apply_action(typed_action);
                }

                if (status.first == MessageLevel::Error)
                {
                    break;
                }
            }

            if (executed_any_action)
            {
                auto final_state = ps.timeline.get_state();
                final_state.aux = context;
                ps.timeline.stage(std::move(final_state));
            }

            if (executed_any_action)
            {
                previous_action_chain_ = executed_chain;
            }

            auto const stage_engine = ps.timeline.get_state().sequencer;
            auto const committed_engine = ps.timeline.get_committed_state().sequencer;
            auto const staged_engine_differs_from_commit =
                stage_engine != committed_engine;

            if (force_commit_requested ||
                (staged_engine_differs_from_commit && auto_commit_candidate))
            {
                ps.timeline.commit();
            }
            if (auto const id = ps.timeline.get_current_commit_id();
                id != previous_commit_id_)
            {
                previous_commit_id_ = id;
                pending_engine_state_update.publish(ps.timeline.get_state().sequencer);
            }
            if (executed_any_action)
            {
                notify_ui_state_changed();
            }
            return status;
        }
        catch (...)
        {
            ps.timeline.reset_stage();
            throw; // rethrow so you can return proper message without duplicating above
        }
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
