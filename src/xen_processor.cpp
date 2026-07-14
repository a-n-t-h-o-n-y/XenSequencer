#include <xen/xen_processor.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/coordinator_registry.hpp>
#include <xen/ipc_sequencer_session_client.hpp>
#include <xen/message_level.hpp>
#include <xen/serialize.hpp>
#include <xen/state.hpp>
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

[[nodiscard]] auto make_id(char const *prefix) -> std::string
{
    return std::string{prefix} + "-" + juce::Uuid{}.toString().toStdString();
}

[[nodiscard]] auto startup_session_id() -> xen::SessionId
{
    if (auto const *disable_discovery =
            std::getenv("XEN_SEQUENCER_DISABLE_ACTIVE_SESSION_DISCOVERY");
        disable_discovery != nullptr && std::string{disable_discovery} == "1")
    {
        return make_id("session");
    }

    auto const active_sessions = xen::ipc::CoordinatorRegistry::active_entries();
    if (active_sessions.empty())
    {
        return make_id("session");
    }
    if (active_sessions.size() == 1)
    {
        return active_sessions.front().session_id;
    }
    throw std::runtime_error{
        "Multiple active XenSequencer sessions are running; refusing to guess."};
}

class OfflineSequencerSession final : public xen::ProcessorSessionPort
{
  public:
    OfflineSequencerSession(xen::InstanceBinding binding, std::string error_message)
        : binding_{std::move(binding)}, error_message_{std::move(error_message)},
          command_catalog_{xen::create_command_catalog()}
    {
    }

    [[nodiscard]] auto project_snapshot() const -> xen::ProjectSnapshot override
    {
        return snapshot_;
    }

    [[nodiscard]] auto persistent_project_snapshot() const
        -> xen::ProjectSnapshot override
    {
        return snapshot_;
    }

    [[nodiscard]] auto library_snapshot() const -> xen::LibrarySnapshot override
    {
        return library_;
    }

    [[nodiscard]] auto instance_binding() const -> xen::InstanceBinding override
    {
        return binding_;
    }

    [[nodiscard]] auto command_catalog_metadata() const
        -> std::vector<xen::CatalogCommandMetadata> override
    {
        return command_catalog_.metadata();
    }

    [[nodiscard]] auto execute_command_string(std::string const &,
                                              xen::CommandContext const &)
        -> xen::CommandApplicationResult override
    {
        return {
            .status = {xen::MessageLevel::Error, error_message_},
            .suggested_selection = std::nullopt,
        };
    }

    [[nodiscard]] auto begin_preview(xen::ProjectRevision)
        -> xen::PreviewControlResult override
    {
        return {.status = {xen::MessageLevel::Error, error_message_}};
    }

    [[nodiscard]] auto commit_preview(xen::PreviewId const &, xen::ProjectRevision)
        -> xen::PreviewControlResult override
    {
        return {.status = {xen::MessageLevel::Error, error_message_}};
    }

    [[nodiscard]] auto cancel_preview(xen::PreviewId const &, xen::ProjectRevision)
        -> xen::PreviewControlResult override
    {
        return {.status = {xen::MessageLevel::Error, error_message_}};
    }

    [[nodiscard]] auto create_project(xen::ProjectRevision, bool)
        -> xen::DocumentOperationResult override
    {
        throw xen::DocumentError{xen::DocumentErrorCode::Io, error_message_};
    }

    [[nodiscard]] auto open_project(std::string, xen::ProjectRevision, bool)
        -> xen::DocumentOperationResult override
    {
        throw xen::DocumentError{xen::DocumentErrorCode::Io, error_message_};
    }

    [[nodiscard]] auto save_project(xen::ProjectRevision)
        -> xen::DocumentOperationResult override
    {
        throw xen::DocumentError{xen::DocumentErrorCode::Io, error_message_};
    }

    [[nodiscard]] auto save_project_as(std::string, xen::ProjectRevision,
                                       std::optional<std::string>)
        -> xen::DocumentOperationResult override
    {
        throw xen::DocumentError{xen::DocumentErrorCode::Io, error_message_};
    }

    [[nodiscard]] auto restore_recovery(std::string, xen::ProjectRevision, bool)
        -> xen::DocumentOperationResult override
    {
        throw xen::DocumentError{xen::DocumentErrorCode::Io, error_message_};
    }

    [[nodiscard]] auto discard_recovery(std::string)
        -> xen::DocumentOperationResult override
    {
        throw xen::DocumentError{xen::DocumentErrorCode::Io, error_message_};
    }

    [[nodiscard]] auto import_cell(std::string, xen::ProjectRevision,
                                   xen::CompositionCursor)
        -> xen::DocumentOperationResult override
    {
        throw xen::DocumentError{xen::DocumentErrorCode::Io, error_message_};
    }

    [[nodiscard]] auto save_cell(std::string, xen::ProjectRevision,
                                 xen::CompositionCursor, xen::SelectionPath,
                                 std::optional<std::string>)
        -> xen::DocumentOperationResult override
    {
        throw xen::DocumentError{xen::DocumentErrorCode::Io, error_message_};
    }

    void set_channel_id(xen::ChannelId) override
    {
        throw std::runtime_error{error_message_};
    }

    [[nodiscard]] auto compiled_midi_generation() const noexcept
        -> std::uint64_t override
    {
        return 0;
    }

    [[nodiscard]] auto try_consume_compiled_midi() noexcept
        -> std::optional<xen::CompiledMidiMailbox::ReadView> override
    {
        return std::nullopt;
    }

    [[nodiscard]] auto midi_compilation_status() const
        -> xen::MidiCompilationStatus override
    {
        return {};
    }

  private:
    xen::InstanceBinding binding_;
    std::string error_message_;
    xen::CommandCatalog command_catalog_;
    xen::ProjectSnapshot snapshot_{
        .project = xen::ProjectState{},
        .history_entry_id = xen::HistoryEntryId{1},
        .project_revision = xen::detail::allocate_project_revision(),
        .state_revision = xen::detail::allocate_state_revision(),
    };
    xen::LibrarySnapshot library_{};
};

} // namespace

namespace xen
{

XenProcessor::XenProcessor(SubmissionEffects::FailurePoint effect_failure,
                           std::filesystem::path workspace_settings_file)
{
    (void)effect_failure;
    (void)workspace_settings_file;
    try
    {
        auto binding = InstanceBinding{
            .session_id = startup_session_id(),
            .instance_id = make_id("instance"),
            .channel_id = {},
        };
        session_ = std::make_unique<ipc::IpcSequencerSessionClient>(binding);
    }
    catch (std::exception const &e)
    {
        auto binding = InstanceBinding{
            .session_id = make_id("session"),
            .instance_id = make_id("instance"),
            .channel_id = DEFAULT_CHANNEL_ID,
        };
        auto message =
            std::string{"XenSequencerCoordinator startup failed: "} + e.what();
        juce::Logger::writeToLog(message);
        session_ =
            std::make_unique<OfflineSequencerSession>(std::move(binding), message);
    }
}

auto XenProcessor::session() noexcept -> SequencerSessionPort &
{
    return *session_;
}

auto XenProcessor::session() const noexcept -> SequencerSessionPort const &
{
    return *session_;
}

auto XenProcessor::audio_thread_state_snapshot() const noexcept
    -> AudioThreadStateForGUI
{
    return audio_thread_state_for_gui_.read();
}

auto XenProcessor::midi_compilation_status() const -> MidiCompilationStatus
{
    return session_->midi_compilation_status();
}

void XenProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                juce::MidiBuffer &midi_buffer)
{
    buffer.clear();
    process_midi_block(buffer.getNumSamples(), midi_buffer);
}

void XenProcessor::processBlock(juce::AudioBuffer<double> &buffer,
                                juce::MidiBuffer &midi_buffer)
{
    buffer.clear();
    process_midi_block(buffer.getNumSamples(), midi_buffer);
}

void XenProcessor::process_midi_block(int sample_count,
                                      juce::MidiBuffer &midi_buffer) noexcept
{
    auto bpm = audio_thread_state_.daw.bpm > 0.0F
                   ? static_cast<double>(audio_thread_state_.daw.bpm)
                   : 120.0;
    auto playing = false;
    auto has_ppq = false;
    auto ppq = 0.0;
    if (auto *playhead = getPlayHead(); playhead != nullptr)
    {
        auto const position = playhead->getPosition();
        if (position.hasValue())
        {
            playing = position->getIsPlaying();
            if (auto const host_bpm = position->getBpm();
                host_bpm.hasValue() && valid_bpm(*host_bpm))
            {
                bpm = *host_bpm;
            }
            if (auto const host_ppq = position->getPpqPosition();
                host_ppq.hasValue() && std::isfinite(*host_ppq))
            {
                ppq = *host_ppq;
                has_ppq = true;
            }
        }
    }
    auto const sample_rate = valid_sample_rate(getSampleRate())
                                 ? static_cast<std::uint32_t>(getSampleRate())
                                 : std::uint32_t{0};
    audio_thread_state_.daw = {
        .bpm = static_cast<float>(bpm),
        .sample_rate = sample_rate,
        .is_playing = playing,
    };

    if (auto const update = session_->try_consume_compiled_midi())
    {
        audio_thread_state_.midi_player.adopt(update->update());
    }
    audio_thread_state_.midi_player.process(
        {
            .playing = playing,
            .has_ppq = has_ppq,
            .ppq = ppq,
            .bpm = bpm,
            .sample_rate = sample_rate,
            .sample_count = sample_count,
        },
        midi_buffer);

    audio_thread_state_for_gui_.write({
        .daw = audio_thread_state_.daw,
        .loop_phase = audio_thread_state_.midi_player.loop_phase(),
        .transport_active = playing,
        .midi_status = audio_thread_state_.midi_player.status(),
    });
}

auto XenProcessor::createEditor() -> juce::AudioProcessorEditor *
{
    return new gui::XenEditor{*this, editor_width, editor_height};
}

void XenProcessor::getStateInformation(juce::MemoryBlock &dest_data)
{
    try
    {
        auto const json_str = serialize_processor_state(
            session_->instance_binding(), session_->persistent_project_snapshot());
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
        if (data == nullptr || sizeInBytes <= 0 ||
            static_cast<std::size_t>(sizeInBytes) > MAX_PERSISTED_STATE_BYTES)
        {
            throw std::invalid_argument{"Processor state has an invalid size."};
        }
        auto const json_str =
            std::string(static_cast<char const *>(data), (std::size_t)sizeInBytes);
        auto state = deserialize_processor_state(json_str);
        session_ =
            std::make_unique<ipc::IpcSequencerSessionClient>(state.binding, state);
    }
    catch (std::exception const &e)
    {
        juce::Logger::writeToLog("XenSequencer setStateInformation error: " +
                                 juce::String{e.what()});
    }
}

void XenProcessor::prepareToPlay(double sample_rate, int samples_per_block)
{
    auto const rate = valid_sample_rate(sample_rate)
                          ? static_cast<std::uint32_t>(sample_rate)
                          : std::uint32_t{0};
    audio_thread_state_.midi_player.prepare(rate, samples_per_block);
}

void XenProcessor::releaseResources()
{
    audio_thread_state_.midi_player.release();
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
