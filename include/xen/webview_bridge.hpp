#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>

#include <xen/keymap.hpp>
#include <xen/sequencer_session.hpp>

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
    explicit WebviewBridge(SequencerSession &session,
                           juce::File keymap_file = KeymapStore::default_file());

    [[nodiscard]] auto handle_request_json(std::string const &request_json)
        -> std::string;

    [[nodiscard]] auto make_state_changed_event_json() const -> std::string;
    [[nodiscard]] auto make_library_changed_event_json() const -> std::string;
    [[nodiscard]] auto make_keymap_changed_event_json() const -> std::string;
    [[nodiscard]] auto make_phase_sync_event_json(MeasurePhase phase, float bpm) const
        -> std::string;
    [[nodiscard]] auto make_transport_stopped_event_json() const -> std::string;
    [[nodiscard]] auto keymap_revision() const noexcept -> std::uint64_t;

  private:
    SequencerSession &session_;
    KeymapStore keymap_store_;
};

} // namespace xen
