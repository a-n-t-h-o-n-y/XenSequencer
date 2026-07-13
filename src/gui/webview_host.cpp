#include <xen/gui/webview_host.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <xen/user_directory.hpp>

#if XEN_WEB_UI_USE_EMBEDDED
#include <embed_webui.hpp>
#endif

namespace
{
auto midi_fault_name(xen::RealtimeMidiFault fault) -> juce::String
{
    switch (fault)
    {
    case xen::RealtimeMidiFault::None:
        return "none";
    case xen::RealtimeMidiFault::CompilationFailed:
        return "compilation failed";
    case xen::RealtimeMidiFault::MissingPpq:
        return "host PPQ unavailable";
    case xen::RealtimeMidiFault::InvalidTransport:
        return "invalid transport";
    case xen::RealtimeMidiFault::BlockTooLarge:
        return "audio block exceeds prepared maximum";
    case xen::RealtimeMidiFault::EventCapacityExceeded:
        return "MIDI event capacity exceeded";
    case xen::RealtimeMidiFault::MidiByteCapacityExceeded:
        return "MIDI byte capacity exceeded";
    }
    return "unknown";
}

void append_webview_error_log(juce::String const &message)
{
    juce::Logger::writeToLog(message);

    try
    {
        auto const log_file =
            xen::get_user_settings_directory().getChildFile("webview-errors.log");
        log_file.appendText(message + "\n\n", false, false, "\n");
    }
    catch (std::exception const &)
    {
    }
}

auto truncate_for_log(std::string const &text) -> juce::String
{
    auto value = juce::String{text};
    if (value.length() > 4096)
    {
        value = value.substring(0, 4096) + "...<truncated>";
    }
    return value;
}

auto parse_json_to_var_or_throw(std::string const &json_text,
                                std::string const &context) -> juce::var
{
    auto const juce_text =
        juce::String::fromUTF8(json_text.data(), (int)json_text.size());
    auto parsed = juce::var{};
    if (auto const parse_result = juce::JSON::parse(juce_text, parsed);
        parse_result.failed())
    {
        throw std::runtime_error(context + " produced invalid JSON: " +
                                 parse_result.getErrorMessage().toStdString());
    }
    return parsed;
}

#if XEN_WEB_UI_USE_EMBEDDED
auto default_mime_type() -> juce::String
{
    return "application/octet-stream";
}

auto webview_debug_script() -> juce::String
{
    return R"JS(
(function () {
  const emit = function (level, message, detail) {
    try {
      if (!window.__JUCE__ || !window.__JUCE__.backend) {
        return;
      }
      window.__JUCE__.backend.emitEvent("xenWebviewDebug", {
        level: level,
        message: String(message),
        detail: detail == null ? "" : String(detail)
      });
    } catch (_) {
    }
  };

  const stringify = function (value) {
    try {
      if (value instanceof Error) {
        return value.name + ": " + value.message + "\n" + (value.stack || "");
      }
      if (typeof value === "object") {
        return JSON.stringify(value);
      }
      return String(value);
    } catch (_) {
      return String(value);
    }
  };

  window.addEventListener("error", function (event) {
    emit("error", event.message || "window error", [
      event.filename || "",
      event.lineno || 0,
      event.colno || 0,
      stringify(event.error)
    ].join(":"));
  });

  window.addEventListener("unhandledrejection", function (event) {
    emit("error", "unhandled promise rejection", stringify(event.reason));
  });

  ["error", "warn", "log"].forEach(function (level) {
    const original = console[level];
    console[level] = function () {
      const args = Array.prototype.slice.call(arguments).map(stringify);
      emit(level, "console." + level, args.join(" "));
      if (typeof original === "function") {
        original.apply(console, arguments);
      }
    };
  });

  emit("info", "webview debug hook installed", window.location.href);
}());
)JS";
}

auto debug_payload_to_log_message(juce::var const &payload) -> juce::String
{
    if (auto const *object = payload.getDynamicObject(); object != nullptr)
    {
        auto const level = object->getProperty("level").toString();
        auto const message = object->getProperty("message").toString();
        auto const detail = object->getProperty("detail").toString();
        return "XenSequencer WebView debug [" + level + "]: " + message +
               (detail.isNotEmpty() ? "\n" + detail : "");
    }

    return "XenSequencer WebView debug: " + payload.toString();
}

auto mime_type_for_path(juce::String path) -> juce::String
{
    auto const extension = juce::File{path}.getFileExtension().toLowerCase();

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
                                         juce::String original_filename) -> bool
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

auto embedded_resource_manifest_summary() -> juce::String
{
    auto summary = juce::String{};

    for (auto i = 0; i < embed_webui::namedResourceListSize; ++i)
    {
        auto const *resource_name = embed_webui::namedResourceList[i];
        auto const *original_filename =
            embed_webui::getNamedResourceOriginalFilename(resource_name);
        auto size = 0;
        (void)embed_webui::getNamedResource(resource_name, size);

        summary += "\n  ";
        summary += resource_name != nullptr ? resource_name : "<null resource>";
        summary += " <- ";
        summary += original_filename != nullptr ? original_filename
                                                : "<null original filename>";
        summary += " (";
        summary += juce::String{size};
        summary += " bytes)";
    }

    return summary;
}

#endif

#if XEN_WEB_UI_USE_DEV_SERVER
class CallbackWebBrowserComponent final : public juce::WebBrowserComponent
{
  public:
    using LoadSuccessHandler = std::function<void(juce::String const &)>;
    using LoadFailureHandler = std::function<bool(juce::String const &)>;

    CallbackWebBrowserComponent(juce::WebBrowserComponent::Options const &options,
                                LoadSuccessHandler on_load_success,
                                LoadFailureHandler on_load_failure)
        : juce::WebBrowserComponent{options},
          on_load_success_{std::move(on_load_success)},
          on_load_failure_{std::move(on_load_failure)}
    {
    }

    void pageFinishedLoading(juce::String const &url) override
    {
        if (on_load_success_ != nullptr)
        {
            on_load_success_(url);
        }
    }

    auto pageLoadHadNetworkError(juce::String const &error_info) -> bool override
    {
        if (on_load_failure_ == nullptr)
        {
            return true;
        }

        return on_load_failure_(error_info);
    }

  private:
    LoadSuccessHandler on_load_success_;
    LoadFailureHandler on_load_failure_;
};

auto parse_dev_server_urls(juce::String configured_urls) -> std::vector<juce::String>
{
    auto urls = std::vector<juce::String>{};
    auto remaining = std::move(configured_urls);

    while (true)
    {
        auto const comma_index = remaining.indexOfChar(',');
        auto entry = comma_index >= 0 ? remaining.substring(0, comma_index) : remaining;
        entry = entry.trim();

        if (entry.isNotEmpty())
        {
            urls.push_back(entry);
        }

        if (comma_index < 0)
        {
            break;
        }

        remaining = remaining.substring(comma_index + 1);
    }

    return urls;
}

auto escape_html(juce::String text) -> juce::String
{
    text = text.replace("&", "&amp;");
    text = text.replace("<", "&lt;");
    text = text.replace(">", "&gt;");
    text = text.replace("\"", "&quot;");
    text = text.replace("'", "&#39;");
    return text;
}

auto make_dev_server_error_page(juce::String const &configured_urls,
                                std::vector<juce::String> const &attempted_urls)
    -> juce::String
{
    auto attempted_urls_html = juce::String{};
    if (attempted_urls.empty())
    {
        attempted_urls_html = "<li>No usable URLs were configured.</li>";
    }
    else
    {
        for (auto const &url : attempted_urls)
        {
            attempted_urls_html += "<li><code>" + escape_html(url) + "</code></li>";
        }
    }

    return "<!doctype html>"
           "<html>"
           "<head>"
           "<meta charset=\"utf-8\">"
           "<title>XenSequencer Web UI</title>"
           "<style>"
           "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
           "margin:0;padding:24px;background:#111827;color:#e5e7eb;}"
           "main{max-width:720px;margin:0 auto;}"
           "h1{font-size:20px;margin:0 0 12px;}"
           "p{line-height:1.5;margin:0 0 12px;}"
           "code{font-family:'SFMono-Regular','Consolas','Menlo',monospace;"
           "background:#1f2937;padding:2px 6px;border-radius:4px;}"
           "ul{margin:0 0 16px 20px;padding:0;}"
           "li{margin:0 0 8px;}"
           "</style>"
           "</head>"
           "<body>"
           "<main>"
           "<h1>Unable to load the dev server UI.</h1>"
           "<p>XenSequencer could not reach any configured dev-server URL.</p>"
           "<p>Attempted URLs:</p>"
           "<ul>" +
           attempted_urls_html +
           "</ul>"
           "<p>Configured <code>XEN_WEB_UI_DEV_URL</code> value:</p>"
           "<p><code>" +
           escape_html(configured_urls) +
           "</code></p>"
           "</main>"
           "</body>"
           "</html>";
}
#endif

} // namespace

namespace xen::gui
{

WebviewHost::WebviewHost(XenProcessor &processor)
    : processor_{processor}, bridge_{processor.session()}
{
#if XEN_WEB_UI_USE_DEV_SERVER
    candidate_urls_ = parse_dev_server_urls(juce::String{XEN_WEB_UI_DEV_URL});
    browser_ = std::make_unique<CallbackWebBrowserComponent>(
        create_browser_options(),
        [this](juce::String const &url) { handle_dev_server_load_success(url); },
        [this](juce::String const &error_info) {
            return handle_dev_server_load_failure(error_info);
        });
#else
    browser_ = std::make_unique<juce::WebBrowserComponent>(create_browser_options());
    append_webview_error_log(
        juce::String{"XenSequencer embedded WebView startup"} +
        "\nresource_provider_root: " +
        juce::WebBrowserComponent::getResourceProviderRoot() +
        "\ndist_root: " + juce::String{XEN_WEB_UI_DIST_DIR} +
        "\nembedded_resources:" + embedded_resource_manifest_summary());
#endif

    this->addAndMakeVisible(*browser_);
    this->setWantsKeyboardFocus(true);

    load_initial_url();

    last_project_revision_ = processor_.session().project_snapshot().project_revision;
    last_library_revision_ = processor_.session().library_snapshot().library_revision;
    last_keymap_revision_ = bridge_.keymap_revision();
    this->startTimerHz(30);
}

WebviewHost::~WebviewHost()
{
    this->stopTimer();
    browser_.reset();
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
    auto const project_revision =
        processor_.session().project_snapshot().project_revision;
    if (project_revision != last_project_revision_)
    {
        last_project_revision_ = project_revision;
        emit_state_changed_event();
    }
    auto const library_revision =
        processor_.session().library_snapshot().library_revision;
    if (library_revision != last_library_revision_)
    {
        last_library_revision_ = library_revision;
        emit_library_changed_event();
    }
    (void)bridge_.refresh_keymap();
    auto const keymap_revision = bridge_.keymap_revision();
    if (keymap_revision != last_keymap_revision_)
    {
        last_keymap_revision_ = keymap_revision;
        emit_keymap_changed_event();
    }

    emit_transport_events();
}

auto WebviewHost::create_browser_options() -> juce::WebBrowserComponent::Options
{
    auto options =
        juce::WebBrowserComponent::Options{}
            .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
                                        .withBuiltInErrorPageDisabled())
            .withNativeIntegrationEnabled()
            .withNativeFunction(
                "xenBridgeRequest",
                [this](juce::Array<juce::var> const &args,
                       juce::WebBrowserComponent::NativeFunctionCompletion completion) {
                    auto request_json = std::string{};
                    if (!args.isEmpty())
                    {
                        request_json = args[0].toString().toStdString();
                    }
                    try
                    {
                        auto const response_json =
                            bridge_.handle_request_json(request_json);
                        completion(parse_json_to_var_or_throw(response_json,
                                                              "xenBridgeRequest"));
                    }
                    catch (std::exception const &error)
                    {
                        append_webview_error_log(
                            juce::String{"XenSequencer WebView bridge exception: "} +
                            error.what() +
                            "\nraw_request: " + truncate_for_log(request_json));
                        throw;
                    }
                });

#if XEN_WEB_UI_USE_EMBEDDED
    options = options.withEventListener("xenWebviewDebug", [](juce::var payload) {
        append_webview_error_log(debug_payload_to_log_message(payload));
    });
    options = options.withUserScript(webview_debug_script());
    options = options.withResourceProvider([this](juce::String const &resource_path) {
        return provide_embedded_resource(resource_path);
    });
#endif

    return options;
}

#if XEN_WEB_UI_USE_EMBEDDED
auto WebviewHost::provide_embedded_resource(juce::String const &resource_path) const
    -> std::optional<juce::WebBrowserComponent::Resource>
{
    append_webview_error_log("XenSequencer embedded WebView resource request: " +
                             resource_path);

    auto const normalized_opt = normalize_resource_path(resource_path);
    if (!normalized_opt.has_value())
    {
        append_webview_error_log("XenSequencer embedded WebView resource rejected: " +
                                 resource_path);
        return std::nullopt;
    }

    auto const normalized = *normalized_opt;
    append_webview_error_log("XenSequencer embedded WebView normalized path: " +
                             normalized);

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
            append_webview_error_log(
                "XenSequencer embedded WebView resource data unavailable: " +
                normalized + "\nresource_name: " + resource_name +
                "\noriginal_filename: " + original_filename);
            return std::nullopt;
        }

        auto bytes = std::vector<std::byte>{};
        bytes.resize((std::size_t)size);
        std::memcpy(bytes.data(), data, (std::size_t)size);

        append_webview_error_log("XenSequencer embedded WebView resource hit: " +
                                 normalized + "\nresource_name: " + resource_name +
                                 "\noriginal_filename: " + original_filename +
                                 "\nrelative_path: " + relative_path +
                                 "\nmime_type: " + mime_type_for_path(normalized) +
                                 "\nsize: " + juce::String{size});

        return juce::WebBrowserComponent::Resource{
            .data = std::move(bytes),
            .mimeType = mime_type_for_path(normalized),
        };
    }

    append_webview_error_log("XenSequencer embedded WebView resource miss: " +
                             normalized + "\nraw_request: " + resource_path);
    return std::nullopt;
}
#endif

void WebviewHost::load_initial_url()
{
#if XEN_WEB_UI_USE_DEV_SERVER
    if (candidate_urls_.empty())
    {
        show_dev_server_error_page();
        return;
    }

    load_current_dev_server_url();
#elif XEN_WEB_UI_USE_EMBEDDED
    auto const initial_url = juce::WebBrowserComponent::getResourceProviderRoot();
    browser_->goToURL(initial_url);
#else
#error "Invalid Web UI mode compile definitions."
#endif
}

#if XEN_WEB_UI_USE_DEV_SERVER
void WebviewHost::load_current_dev_server_url()
{
    if (browser_ == nullptr || dev_server_load_succeeded_ ||
        final_failure_page_shown_ || current_candidate_index_ >= candidate_urls_.size())
    {
        return;
    }

    browser_->goToURL(candidate_urls_[current_candidate_index_]);
}

void WebviewHost::load_next_dev_server_url()
{
    ++current_candidate_index_;

    if (current_candidate_index_ < candidate_urls_.size())
    {
        load_current_dev_server_url();
        return;
    }

    show_dev_server_error_page();
}

void WebviewHost::handle_dev_server_load_success(juce::String const &url)
{
    juce::ignoreUnused(url);

    if (dev_server_load_succeeded_ || final_failure_page_shown_)
    {
        return;
    }

    dev_server_load_succeeded_ = true;
}

auto WebviewHost::handle_dev_server_load_failure(juce::String const &error_info) -> bool
{
    juce::ignoreUnused(error_info);

    if (dev_server_load_succeeded_ || final_failure_page_shown_)
    {
        return false;
    }

    if (current_candidate_index_ < candidate_urls_.size())
    {
        attempted_urls_.push_back(candidate_urls_[current_candidate_index_]);
    }

    load_next_dev_server_url();
    return false;
}

void WebviewHost::show_dev_server_error_page()
{
    if (browser_ == nullptr || final_failure_page_shown_)
    {
        return;
    }

    final_failure_page_shown_ = true;

    auto const html =
        make_dev_server_error_page(juce::String{XEN_WEB_UI_DEV_URL}, attempted_urls_);
    auto const data_url =
        "data:text/html;charset=utf-8," + juce::URL::addEscapeChars(html, true);
    browser_->goToURL(data_url);
}
#endif

void WebviewHost::emit_state_changed_event()
{
    auto const event_json = bridge_.make_state_changed_event_json();
    browser_->emitEventIfBrowserIsVisible(
        "xenBridgeEvent", parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
}

void WebviewHost::emit_library_changed_event()
{
    auto const event_json = bridge_.make_library_changed_event_json();
    browser_->emitEventIfBrowserIsVisible(
        "xenBridgeEvent", parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
}

void WebviewHost::emit_keymap_changed_event()
{
    auto const event_json = bridge_.make_keymap_changed_event_json();
    browser_->emitEventIfBrowserIsVisible(
        "xenBridgeEvent", parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
}

void WebviewHost::emit_transport_events()
{
    auto const transport_state = processor_.audio_thread_state_snapshot();
    if (transport_state.midi_status.fault_count != last_midi_fault_count_)
    {
        last_midi_fault_count_ = transport_state.midi_status.fault_count;
        juce::Logger::writeToLog(
            "XenSequencer real-time MIDI fault: " +
            midi_fault_name(transport_state.midi_status.last_fault));
    }
    if (!transport_state.transport_active)
    {
        if (!last_transport_active_)
        {
            return;
        }

        last_transport_active_ = false;
        auto const event_json = bridge_.make_transport_stopped_event_json();
        browser_->emitEventIfBrowserIsVisible(
            "xenBridgeEvent", parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
        return;
    }

    last_transport_active_ = true;
    auto const event_json = bridge_.make_phase_sync_event_json(
        {.phase = transport_state.loop_phase}, transport_state.daw.bpm);
    browser_->emitEventIfBrowserIsVisible(
        "xenBridgeEvent", parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
}

} // namespace xen::gui
