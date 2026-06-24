#include <xen/xen_processor.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/midi.hpp>
#include <xen/serialize.hpp>
#include <xen/state.hpp>
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

XenProcessor::XenProcessor(SubmissionEffects::FailurePoint effect_failure,
                           std::filesystem::path workspace_settings_file)
    : session_{effect_failure, std::move(workspace_settings_file)}
{
}

auto XenProcessor::session() noexcept -> SequencerSession &
{
    return session_;
}

auto XenProcessor::session() const noexcept -> SequencerSession const &
{
    return session_;
}

auto XenProcessor::audio_thread_state_snapshot() const noexcept
    -> AudioThreadStateForGUI
{
    return audio_thread_state_for_gui_.read();
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

    if (auto const snapshot = session_.try_consume_audio_project_update())
    {
        audio_thread_state_.project = &snapshot->state().project;
        update_needed = true;
    }

    if (update_needed && audio_thread_state_.project != nullptr)
    {
        audio_thread_state_.midi_engine.update(*audio_thread_state_.project,
                                               audio_thread_state_.daw);
    }

    // Calculate MIDI buffer slice
    auto const block_size = buffer.getNumSamples();
    auto const sample_count =
        block_size >= 0 ? static_cast<SampleCount>(block_size) : SampleCount{0};
    auto next_slice = audio_thread_state_.midi_engine.step(
        midi_buffer, transport_offset, sample_count, audio_thread_state_.daw);

    midi_buffer.swapWith(next_slice);

    audio_thread_state_for_gui_.write({
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
        auto const json_str = serialize_project(session_.project_snapshot().project);
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
        auto state = deserialize_project(json_str);
        session_.replace_project_history(std::move(state));
    }
    catch (std::exception const &e)
    {
        juce::Logger::writeToLog("XenSequencer setStateInformation error: " +
                                 juce::String{e.what()});
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
