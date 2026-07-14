#include <xen/webview_bridge.hpp>

#include <filesystem>
#include <optional>
#include <utility>

#include <nlohmann/json.hpp>

#include <xen/bridge_serialize.hpp>
#include <xen/webview_bridge_protocol.hpp>

namespace xen
{

WebviewBridge::WebviewBridge(SequencerSessionPort &session,
                             std::filesystem::path keymap_file,
                             std::filesystem::path preferences_file)
    : application_service_{session}, library_files_{}, library_service_{library_files_},
      keymap_service_{std::move(keymap_file)},
      preferences_service_{std::move(preferences_file)},
      dispatcher_{application_service_, library_service_, keymap_service_,
                  preferences_service_}
{
}

auto WebviewBridge::handle_request_json(std::string const &request_json) -> std::string
{
    return dispatcher_.handle_request_json(request_json);
}

auto WebviewBridge::make_state_changed_event_json() const -> std::string
{
    auto const payload =
        bridge::make_project_snapshot(application_service_.project_snapshot());
    return bridge::make_envelope("event", "state.changed", std::nullopt, payload)
        .dump();
}

auto WebviewBridge::make_library_changed_event_json() const -> std::string
{
    return bridge::make_envelope(
               "event", "library.changed", std::nullopt,
               library_service_.make_payload(application_service_.library_snapshot()))
        .dump();
}

auto WebviewBridge::make_keymap_changed_event_json() -> std::string
{
    return bridge::make_envelope("event", "keymap.changed", std::nullopt,
                                 bridge::make_keymap_payload(keymap_service_.read()))
        .dump();
}

auto WebviewBridge::make_preferences_changed_event_json() -> std::string
{
    return bridge::make_envelope(
               "event", "preferences.changed", std::nullopt,
               bridge::make_preferences_payload(preferences_service_.read()))
        .dump();
}

auto WebviewBridge::make_phase_sync_event_json(MeasurePhase phase, float bpm) const
    -> std::string
{
    auto const payload = nlohmann::json{
        {"bpm", bpm},
        {"phase", phase.phase},
    };
    return bridge::make_envelope("event", "transport.phase.sync", std::nullopt, payload)
        .dump();
}

auto WebviewBridge::make_transport_stopped_event_json() const -> std::string
{
    return bridge::make_envelope("event", "transport.stopped", std::nullopt,
                                 nlohmann::json::object())
        .dump();
}

auto WebviewBridge::keymap_revision() const noexcept -> std::uint64_t
{
    return keymap_service_.revision();
}

auto WebviewBridge::refresh_keymap() noexcept -> bool
{
    try
    {
        return keymap_service_.refresh();
    }
    catch (KeymapStorageError const &)
    {
        // Reads still surface the actionable error. Polling cannot publish an
        // opaque JSON resource until the external file becomes valid again.
        return false;
    }
}

auto WebviewBridge::preferences_revision() const noexcept -> std::uint64_t
{
    return preferences_service_.revision();
}

auto WebviewBridge::refresh_preferences() noexcept -> bool
{
    try
    {
        return preferences_service_.refresh();
    }
    catch (PreferencesStorageError const &)
    {
        // Reads still surface the actionable error. Polling cannot publish an
        // opaque JSON resource until the external file becomes valid again.
        return false;
    }
}

} // namespace xen
