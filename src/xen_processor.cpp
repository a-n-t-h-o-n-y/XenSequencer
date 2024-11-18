#include <xen/xen_processor.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>

#include <xen/command.hpp>
#include <xen/midi.hpp>
#include <xen/serialize.hpp>
#include <xen/state.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/utility.hpp>
#include <xen/xen_command_tree.hpp>
#include <xen/xen_editor.hpp>

namespace xen
{

XenProcessor::XenProcessor()
    : plugin_state{.timeline = XenTimeline{{.sequencer = {}, .aux = {}}}},
      command_tree{create_command_tree()}
{
    initialize_demo_files();

    // Send initial state to Audio Thread
    pending_state_update.set(plugin_state.timeline.get_state().sequencer);

    this->execute_command_string("load scales");
    this->execute_command_string("load chords");
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

    if (auto new_state = pending_state_update.get(); new_state.has_value())
    {
        audio_thread_state_.sequencer = std::move(new_state.value());
        update_needed = true;
    }

    if (update_needed)
    {
        audio_thread_state_.midi_engine.update(audio_thread_state_.sequencer,
                                               audio_thread_state_.daw);
    }

    // Calculate MIDI buffer slice
    auto next_slice = audio_thread_state_.midi_engine.step(
        midi_buffer, audio_thread_state_.accumulated_sample_count,
        (SampleCount)buffer.getNumSamples());

    midi_buffer.swapWith(next_slice);

    audio_thread_state_.accumulated_sample_count += (SampleCount)buffer.getNumSamples();

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
    try
    {
        auto const json_str =
            serialize_plugin(plugin_state.timeline.get_state().sequencer);
        dest_data.setSize(json_str.size());
        std::memcpy(dest_data.getData(), json_str.data(), json_str.size());
    }
    catch (std::exception const &e)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "State Save Error",
            "Error in getStateInformation: " + juce::String{e.what()});
    }
}

void XenProcessor::setStateInformation(void const *data, int sizeInBytes)
{
    auto const json_str =
        std::string(static_cast<char const *>(data), (std::size_t)sizeInBytes);
    auto state = deserialize_plugin(json_str);
    plugin_state.timeline.stage({std::move(state), {}});
    plugin_state.timeline.commit();
    pending_state_update.set(plugin_state.timeline.get_state().sequencer);
    auto *const editor_base = this->getActiveEditor();
    if (editor_base != nullptr)
    {
        auto *const editor = dynamic_cast<gui::XenEditor *>(editor_base);
        if (editor != nullptr)
        {
            editor->update();
        }
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
            auto commands = split(command_string, ';');
            auto status = std::pair<MessageLevel, std::string>{MessageLevel::Debug, ""};
            for (auto &command : commands)
            {
                command = minimize_spaces(command);
                if (normalize_id(command) == "again")
                {
                    command = previous_command_string_;
                }
                if (command.empty())
                {
                    continue;
                }
                status = execute(command_tree, ps, command);
            }
            if (ps.timeline.get_commit_flag())
            {
                // join() so that 'again' is replaced with the full command string
                previous_command_string_ = join(commands, ';');
                ps.timeline.commit();
            }
            if (auto const id = ps.timeline.get_current_commit_id();
                id != previous_commit_id_)
            {
                previous_commit_id_ = id;
                pending_state_update.set(ps.timeline.get_state().sequencer);
            }
            return status;
        }
        catch (...)
        {
            // FIXME: This roundabout way can set an invalid selection if a string of
            // commands is executed that includes splitting and movement. But it isn't a
            // huge deal and this behaviour is more desirable that without this patch.

            // Roundabout way to revert partial changes but keep the selected state.
            auto aux = ps.timeline.get_state().aux;
            ps.timeline.reset_stage();
            auto state = ps.timeline.get_state();
            state.aux = std::move(aux);
            ps.timeline.stage(std::move(state));
            throw; // rethrow so you can return proper message without duplicating above
        }
    }
    catch (ErrorNoMatch const &)
    {
        return {MessageLevel::Error, "Command not found: " + command_string};
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
