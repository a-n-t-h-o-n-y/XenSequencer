#include <xen/gui/webview_host.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <sequence/measure.hpp>

#if XEN_WEB_UI_USE_EMBEDDED
#include <embed_webui.hpp>
#endif

namespace
{
auto const inactive_note_start_time = xen::Clock::time_point{};

auto parse_json_to_var_or_throw(std::string const &json_text,
                                std::string const &context) -> juce::var
{
    auto const juce_text =
        juce::String::fromUTF8(json_text.data(), (int)json_text.size());
    auto parsed = juce::var{};
    if (auto const parse_result = juce::JSON::parse(juce_text, parsed);
        parse_result.failed())
    {
        throw std::runtime_error(
            context + " produced invalid JSON: " +
            parse_result.getErrorMessage().toStdString());
    }
    return parsed;
}

auto default_mime_type() -> juce::String
{
    return "application/octet-stream";
}

auto mime_type_for_path(juce::String path) -> juce::String
{
    auto const extension = path.fromLastOccurrenceOf(".", false, false).toLowerCase();

    if (extension == ".html" || extension == ".htm")
        return "text/html; charset=utf-8";
    if (extension == ".js" || extension == ".mjs")
        return "text/javascript; charset=utf-8";
    if (extension == ".css")
        return "text/css; charset=utf-8";
    if (extension == ".json")
        return "application/json; charset=utf-8";
    if (extension == ".svg")
        return "image/svg+xml";
    if (extension == ".png")
        return "image/png";
    if (extension == ".jpg" || extension == ".jpeg")
        return "image/jpeg";
    if (extension == ".gif")
        return "image/gif";
    if (extension == ".ico")
        return "image/x-icon";
    if (extension == ".webp")
        return "image/webp";
    if (extension == ".woff")
        return "font/woff";
    if (extension == ".woff2")
        return "font/woff2";
    if (extension == ".ttf")
        return "font/ttf";
    if (extension == ".map")
        return "application/json; charset=utf-8";

    return default_mime_type();
}

auto normalize_resource_path(juce::String resource_path) -> std::optional<juce::String>
{
    auto normalized = resource_path.upToFirstOccurrenceOf("?", false, false);
    normalized = normalized.upToFirstOccurrenceOf("#", false, false);

    auto const scheme_index = normalized.indexOf("://");
    if (scheme_index >= 0)
    {
        auto const path_index = normalized.indexOfChar(scheme_index + 3, '/');
        normalized = path_index >= 0 ? normalized.substring(path_index) : "/";
    }

    if (normalized.isEmpty() || normalized == "/")
    {
        normalized = "/index.html";
    }

    while (normalized.startsWithChar('/'))
    {
        normalized = normalized.substring(1);
    }

    if (normalized.contains(".."))
    {
        return std::nullopt;
    }

    return normalized;
}

auto resource_path_matches_embedded_file(juce::String const &normalized_request_path,
                                         juce::String original_filename)
    -> bool
{
    auto original_path = original_filename.replaceCharacter('\\', '/');
    while (original_path.startsWith("./"))
    {
        original_path = original_path.substring(2);
    }

    if (original_path == normalized_request_path)
    {
        return true;
    }

    auto const request_basename = juce::File{normalized_request_path}.getFileName();
    auto const original_basename = juce::File{original_path}.getFileName();
    if (!request_basename.isEmpty() && request_basename == original_basename)
    {
        return true;
    }

    return false;
}

auto is_note_active(xen::Clock::time_point note_start_time) -> bool
{
    return note_start_time != inactive_note_start_time;
}

auto loop_duration_seconds(sequence::TimeSignature const &time_signature, float bpm)
    -> double
{
    if (bpm <= 0.f || time_signature.numerator == 0 || time_signature.denominator == 0)
    {
        return 0.0;
    }

    auto const quarters_per_loop =
        (double)time_signature.numerator * (4.0 / (double)time_signature.denominator);
    if (quarters_per_loop <= 0.0)
    {
        return 0.0;
    }

    return quarters_per_loop * 60.0 / (double)bpm;
}

} // namespace

namespace xen::gui
{

WebviewHost::WebviewHost(XenProcessor &processor)
    : processor_{processor}, bridge_{processor}
{
    browser_ =
        std::make_unique<juce::WebBrowserComponent>(create_browser_options());

    this->addAndMakeVisible(*browser_);
    this->setWantsKeyboardFocus(true);

    load_initial_url();

    last_snapshot_version_ = processor_.get_ui_snapshot_version();
    this->startTimerHz(30);
}

void WebviewHost::resized()
{
    if (browser_ != nullptr)
    {
        browser_->setBounds(this->getLocalBounds());
    }
}

void WebviewHost::timerCallback()
{
    auto const version = processor_.get_ui_snapshot_version();
    if (version != last_snapshot_version_)
    {
        last_snapshot_version_ = version;
        emit_state_changed_event();
    }

    emit_transport_events();
}

auto WebviewHost::create_browser_options() -> juce::WebBrowserComponent::Options
{
    auto options = juce::WebBrowserComponent::Options{}
                       .withNativeIntegrationEnabled()
                       .withNativeFunction(
                           "xenBridgeRequest",
                           [this](juce::Array<juce::var> const &args,
                                  juce::WebBrowserComponent::NativeFunctionCompletion
                                      completion) {
                               auto request_json = std::string{};
                               if (!args.isEmpty())
                               {
                                   request_json =
                                       args[0].toString().toStdString();
                               }
                               auto const response_json =
                                   bridge_.handle_request_json(request_json);
                               completion(parse_json_to_var_or_throw(
                                   response_json, "xenBridgeRequest"));
                           });

#if XEN_WEB_UI_USE_EMBEDDED
    options = options.withResourceProvider(
        [this](juce::String const &resource_path) {
            return provide_embedded_resource(resource_path);
        });
#endif

    return options;
}

#if XEN_WEB_UI_USE_EMBEDDED
auto WebviewHost::provide_embedded_resource(juce::String const &resource_path) const
    -> std::optional<juce::WebBrowserComponent::Resource>
{
    auto const normalized_opt = normalize_resource_path(resource_path);
    if (!normalized_opt.has_value())
    {
        return std::nullopt;
    }

    auto const normalized = *normalized_opt;
    auto const dist_root = juce::File{XEN_WEB_UI_DIST_DIR};

    for (auto i = 0; i < embed_webui::namedResourceListSize; ++i)
    {
        auto const *resource_name = embed_webui::namedResourceList[i];
        auto const *original_filename =
            embed_webui::getNamedResourceOriginalFilename(resource_name);
        if (original_filename == nullptr)
        {
            continue;
        }

        auto relative_path = juce::File{original_filename}
                                 .getRelativePathFrom(dist_root)
                                 .replaceCharacter('\\', '/');

        // JUCE BinaryData may flatten source paths to basenames.
        if (relative_path != normalized &&
            !resource_path_matches_embedded_file(normalized, original_filename))
        {
            continue;
        }

        auto size = 0;
        auto const *data = embed_webui::getNamedResource(resource_name, size);
        if (data == nullptr || size <= 0)
        {
            return std::nullopt;
        }

        auto bytes = std::vector<std::byte>{};
        bytes.resize((std::size_t)size);
        std::memcpy(bytes.data(), data, (std::size_t)size);

        return juce::WebBrowserComponent::Resource{
            .data = std::move(bytes),
            .mimeType = mime_type_for_path(normalized),
        };
    }

    return std::nullopt;
}
#endif

void WebviewHost::load_initial_url()
{
#if XEN_WEB_UI_USE_DEV_SERVER
    browser_->goToURL(juce::String{XEN_WEB_UI_DEV_URL});
#elif XEN_WEB_UI_USE_EMBEDDED
    auto const initial_url = juce::WebBrowserComponent::getResourceProviderRoot();
    browser_->goToURL(initial_url);
#else
#error "Invalid Web UI mode compile definitions."
#endif
}

void WebviewHost::emit_state_changed_event()
{
    auto const event_json = bridge_.make_state_changed_event_json();
    browser_->emitEventIfBrowserIsVisible(
        "xenBridgeEvent",
        parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
}

void WebviewHost::emit_transport_events()
{
    auto const transport_state = processor_.audio_thread_state_for_gui.read();

    for (auto i = std::size_t{0}; i < transport_state.note_start_times.size(); ++i)
    {
        auto const was_active = is_note_active(previous_note_start_times_[i]);
        auto const is_active = is_note_active(transport_state.note_start_times[i]);
        if (was_active == is_active)
        {
            continue;
        }

        auto const event_json = bridge_.make_trigger_note_event_json(i, is_active);
        browser_->emitEventIfBrowserIsVisible(
            "xenBridgeEvent",
            parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
    }

    previous_note_start_times_ = transport_state.note_start_times;

    auto phase_payload = std::vector<WebviewBridge::SequencePhase>{};
    if (transport_state.daw.bpm > 0.f)
    {
        auto const snapshot = processor_.get_engine_snapshot();
        auto const now = Clock::now();
        for (auto i = std::size_t{0}; i < transport_state.note_start_times.size(); ++i)
        {
            auto const note_start = transport_state.note_start_times[i];
            if (!is_note_active(note_start))
            {
                continue;
            }

            auto const loop_seconds = loop_duration_seconds(
                snapshot.engine.sequence_bank[i].time_signature, transport_state.daw.bpm);
            if (loop_seconds <= 0.0)
            {
                continue;
            }

            auto const elapsed_seconds =
                std::chrono::duration<double>(now - note_start).count();
            if (elapsed_seconds < 0.0)
            {
                continue;
            }

            auto const phase =
                std::fmod(elapsed_seconds, loop_seconds) / loop_seconds;
            phase_payload.push_back({.sequence_index = i, .phase = phase});
        }
    }

    if (!phase_payload.empty())
    {
        auto const event_json =
            bridge_.make_phase_sync_event_json(phase_payload, transport_state.daw.bpm);
        browser_->emitEventIfBrowserIsVisible(
            "xenBridgeEvent",
            parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
    }
}

} // namespace xen::gui
