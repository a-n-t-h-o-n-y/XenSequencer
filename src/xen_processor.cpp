#include <xen/xen_processor.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <sequence/measure.hpp>

#include <xen/midi.hpp>
#include <xen/serialize.hpp>
#include <xen/user_directory.hpp>
#include <xen/utility.hpp>
#include <xen/xen_command_tree.hpp>
#include <xen/xen_editor.hpp>

namespace
{

/**
 * Compares two juce::MidiMessage objects for equality.
 *
 * @param a First MidiMessage to compare.
 * @param b Second MidiMessage to compare.
 * @return bool True if the MidiMessages are equal, false otherwise.
 */
[[nodiscard]] auto are_midi_messages_equal(juce::MidiMessage const &a,
                                           juce::MidiMessage const &b) -> bool
{
    if (a.getRawDataSize() != b.getRawDataSize())
    {
        return false;
    }

    if (std::memcmp(a.getRawData(), b.getRawData(), (std::size_t)a.getRawDataSize()) !=
        0)
    {
        return false;
    }

    return true;
}

} // namespace

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

auto XenProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                juce::MidiBuffer &midi_buffer) -> void
{
    buffer.clear();

    { // Update DAWState
        auto const bpm = [this] {
            auto *playhead = this->getPlayHead();
            auto const position = playhead->getPosition();
            if (!position.hasValue())
            {
                throw std::runtime_error{"PlayHead position is not valid"};
            }
            auto const bpm = position->getBpm();
            return bpm ? static_cast<float>(*bpm) : 120.f;
        }();

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
    }

    // Calculate MIDI buffer slice
    auto next_slice = audio_thread_state_.midi_engine.step(
        midi_buffer, audio_thread_state_.accumulated_sample_count,
        buffer.getNumSamples());

    midi_buffer.swapWith(next_slice);

    audio_thread_state_.accumulated_sample_count += buffer.getNumSamples();
}

auto XenProcessor::processBlock(juce::AudioBuffer<double> &buffer,
                                juce::MidiBuffer &midi_buffer) -> void
{
    // Just forward to float version, this is a midi-only plugin.
    buffer.clear();
    auto empty = juce::AudioBuffer<float>{};
    this->processBlock(empty, midi_buffer);
}

auto XenProcessor::createEditor() -> juce::AudioProcessorEditor *
{
    return new gui::XenEditor{*this};
}

auto XenProcessor::getStateInformation(juce::MemoryBlock &dest_data) -> void
{
    auto const json_str = serialize_plugin(plugin_state.timeline.get_state().sequencer,
                                           plugin_state.display_name);
    dest_data.setSize(json_str.size());
    std::memcpy(dest_data.getData(), json_str.data(), json_str.size());
}

auto XenProcessor::setStateInformation(void const *data, int sizeInBytes) -> void
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

} // namespace xen
