#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>

#include <xen/sequencer_session_port.hpp>
#include <xen/webview_bridge_services.hpp>

namespace xen
{

class WebviewBridge
{
  public:
    struct MeasurePhase
    {
        double phase{0.0};
    };

  public:
    explicit WebviewBridge(
        SequencerSessionPort &session,
        std::filesystem::path keymap_file = KeymapStore::default_file(),
        std::filesystem::path preferences_file = PreferencesStore::default_file());

    [[nodiscard]] auto handle_request_json(std::string const &request_json)
        -> std::string;

    [[nodiscard]] auto make_state_changed_event_json() const -> std::string;
    [[nodiscard]] auto make_library_changed_event_json() const -> std::string;
    [[nodiscard]] auto make_keymap_changed_event_json() -> std::string;
    [[nodiscard]] auto make_preferences_changed_event_json() -> std::string;
    [[nodiscard]] auto make_phase_sync_event_json(MeasurePhase phase, float bpm) const
        -> std::string;
    [[nodiscard]] auto make_transport_stopped_event_json() const -> std::string;
    [[nodiscard]] auto keymap_revision() const noexcept -> std::uint64_t;
    auto refresh_keymap() noexcept -> bool;
    [[nodiscard]] auto preferences_revision() const noexcept -> std::uint64_t;
    auto refresh_preferences() noexcept -> bool;

  private:
    bridge::SequencerApplicationBridgeService application_service_;
    bridge::JuceLibraryFilePort library_files_;
    bridge::JuceLibraryBridgeService library_service_;
    bridge::StoreKeymapBridgeService keymap_service_;
    bridge::StorePreferencesBridgeService preferences_service_;
    bridge::BridgeRequestDispatcher dispatcher_;
};

} // namespace xen
