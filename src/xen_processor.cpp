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
#include <xen/gui/themes.hpp>
#include <xen/guide_text.hpp>
#include <xen/midi.hpp>
#include <xen/serialize.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/utility.hpp>
#include <xen/xen_editor.hpp>

namespace xen
{

XenProcessor::XenProcessor()
{
    initialize_demo_files();
    runtime_state.shared.theme = gui::find_theme("apollo");

    // Send initial state to Audio Thread.
    if (auto initial = engine.take_pending_sequencer_update(); initial.has_value())
    {
        pending_state_update.set(std::move(*initial));
    }

    // Engine-owned startup content.
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

        update_needed = !utility::compare_within_tolerance(audio_thread_state_.daw.bpm,
                                                           bpm, 0.0001f) ||
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
        (SampleCount)buffer.getNumSamples(), audio_thread_state_.daw);

    midi_buffer.swapWith(next_slice);

    audio_thread_state_.accumulated_sample_count += (SampleCount)buffer.getNumSamples();

    audio_thread_state_for_gui.write({
        .daw = audio_thread_state_.daw,
        .note_start_times =
            audio_thread_state_.midi_engine.get_trigger_note_start_times(),
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
        auto const json_str = serialize_plugin(engine.state().timeline.get_state().sequencer);
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
    engine.load_serialized_state(std::move(state));
    if (auto pending = engine.take_pending_sequencer_update(); pending.has_value())
    {
        pending_state_update.set(std::move(*pending));
    }
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
    auto commands = split(command_string, ';');
    auto status = std::pair<MessageLevel, std::string>{MessageLevel::Debug, ""};
    auto ran_non_empty = false;

    for (auto &command : commands)
    {
        command = minimize_spaces(command);
        if (command.empty())
        {
            continue;
        }

        ran_non_empty = true;

        if (to_lower(command) == "again")
        {
            if (previous_command_string_.empty())
            {
                status = minfo("Nothing to repeat.");
            }
            else
            {
                status = this->execute_command_string(previous_command_string_);
            }
            continue;
        }

        if (auto runtime_status = runtime_command_tree_.execute(runtime_state, command);
            runtime_status.has_value())
        {
            status = std::move(*runtime_status);
            continue;
        }

        status = engine.execute_command(command);
        if (auto pending = engine.take_pending_sequencer_update(); pending.has_value())
        {
            pending_state_update.set(std::move(*pending));
        }
    }

    if (ran_non_empty)
    {
        // Persist expanded command text so `again` repeats the previous action string.
        auto expanded = join(commands, ';');
        if (!expanded.empty())
        {
            previous_command_string_ = std::move(expanded);
        }
    }

    return status;
}

auto XenProcessor::guide_text(std::string const &partial_command) const -> std::string
{
    auto const runtime = runtime_command_tree_.guide_text(partial_command);
    if (!runtime.empty())
    {
        return runtime;
    }
    return generate_guide_text(engine.command_tree(), partial_command);
}

auto XenProcessor::complete_id(std::string const &partial_command) const -> std::string
{
    auto const runtime = runtime_command_tree_.complete_id(partial_command);
    if (!runtime.empty())
    {
        return runtime;
    }
    return xen::complete_id(engine.command_tree(), partial_command);
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
