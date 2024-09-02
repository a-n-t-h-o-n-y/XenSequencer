#include <xen/xen_processor.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include <sequence/measure.hpp>

#include <xen/midi.hpp>
#include <xen/serialize.hpp>
#include <xen/state.hpp>
#include <xen/user_directory.hpp>
#include <xen/utility.hpp>
#include <xen/xen_command_tree.hpp>
#include <xen/xen_editor.hpp>

namespace xen
{

XenProcessor::XenProcessor()
    : plugin_state{.timeline = XenTimeline{{init_state(), {}}}},
      active_sessions{plugin_state.PROCESS_UUID, plugin_state.display_name},
      command_tree{create_command_tree()} /*, sequencer_state_copy_{init_state()} */
{
    initialize_demo_files();

    active_sessions.on_display_name_request.connect(
        [this] { return plugin_state.display_name; });

    active_sessions.on_measure_request.connect([this](std::size_t measure_index) {
        auto const [state, _] = plugin_state.timeline.get_state();
        return state.sequence_bank[measure_index];
    });

    active_sessions.on_measure_response.connect(
        [this](sequence::Measure const &measure) {
            auto [state, aux] = plugin_state.timeline.get_state();

            state.sequence_bank[aux.selected.measure] = measure;
            aux.selected.cell.clear();

            plugin_state.timeline.stage({state, aux});
            plugin_state.timeline.commit();

            auto *const editor_base = this->getActiveEditor();
            if (editor_base != nullptr)
            {
                auto *const editor = dynamic_cast<gui::XenEditor *>(editor_base);
                if (editor != nullptr)
                {
                    editor->update_ui();
                }
            }
        });

    // Send initial state to Audio Thread
    (void)new_state_transfer_queue.push(plugin_state.timeline.get_state().sequencer);
}

void XenProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                juce::MidiBuffer &midi_buffer)
{
    buffer.clear();

    bool update_needed = false;

    { // Update DAWState
        auto const bpm = [this] {
            auto *playhead = this->getPlayHead();
            auto const position = playhead->getPosition();
            if (!position.hasValue())
            {
                throw std::runtime_error{"PlayHead position is not valid"};
            }
            auto const bpm_opt = position->getBpm();
            return bpm_opt ? static_cast<float>(*bpm_opt) : 120.f;
        }();

        auto const sample_rate = static_cast<std::uint32_t>(this->getSampleRate());

        update_needed =
            !compare_within_tolerance(audio_thread_state_.daw.bpm, bpm, 0.0001f) ||
            audio_thread_state_.daw.sample_rate != sample_rate;

        audio_thread_state_.daw = DAWState{
            .bpm = bpm,
            .sample_rate = static_cast<std::uint32_t>(this->getSampleRate()),
        };
    }

    { // Receive new SequencerState data (if any) and render MIDI.
        auto new_state = SequencerState{};
        bool new_state_received = false;

        while (new_state_transfer_queue.pop(new_state)) // Keep only the most recent
        {
            new_state_received = true;
        }

        if (new_state_received)
        {
            audio_thread_state_.midi_engine.update(std::move(new_state),
                                                   audio_thread_state_.daw);
        }
        else if (update_needed)
        {
            audio_thread_state_.midi_engine.update(audio_thread_state_.daw);
        }
    }

    // Calculate MIDI buffer slice
    auto next_slice = audio_thread_state_.midi_engine.step(
        midi_buffer, audio_thread_state_.accumulated_sample_count,
        buffer.getNumSamples());

    midi_buffer.swapWith(next_slice);

    audio_thread_state_.accumulated_sample_count +=
        (std::uint64_t)buffer.getNumSamples();

    audio_thread_state_for_gui.write({
        .daw = audio_thread_state_.daw,
        .accumulated_sample_count = audio_thread_state_.accumulated_sample_count,
        .note_start_times = audio_thread_state_.midi_engine.get_note_start_samples(),
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
    auto const json_str = serialize_plugin(plugin_state.timeline.get_state().sequencer,
                                           plugin_state.display_name);
    dest_data.setSize(json_str.size());
    std::memcpy(dest_data.getData(), json_str.data(), json_str.size());
}

void XenProcessor::setStateInformation(void const *data, int sizeInBytes)
{
    auto const json_str =
        std::string(static_cast<char const *>(data), (std::size_t)sizeInBytes);
    auto [state, dn] = deserialize_plugin(json_str);
    plugin_state.display_name = std::move(dn);
    plugin_state.timeline.stage({std::move(state), {}});
    plugin_state.timeline.commit();
    (void)new_state_transfer_queue.push(plugin_state.timeline.get_state().sequencer);
    auto *const editor_base = this->getActiveEditor();
    if (editor_base != nullptr)
    {
        auto *const editor = dynamic_cast<gui::XenEditor *>(editor_base);
        if (editor != nullptr)
        {
            editor->update_ui();
        }
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
    return JucePlugin_Name;
}

auto XenProcessor::hasEditor() const -> bool
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
