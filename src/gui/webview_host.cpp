#include <xen/gui/webview_host.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#if XEN_WEB_UI_USE_EMBEDDED
#include <embed_webui.hpp>
#endif

namespace
{
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

#if XEN_WEB_UI_USE_EMBEDDED
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
        auto entry =
            comma_index >= 0 ? remaining.substring(0, comma_index) : remaining;
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
            attempted_urls_html +=
                "<li><code>" + escape_html(url) + "</code></li>";
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
    : processor_{processor}, bridge_{processor}
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
    browser_ =
        std::make_unique<juce::WebBrowserComponent>(create_browser_options());
#endif

    this->addAndMakeVisible(*browser_);
    this->setWantsKeyboardFocus(true);

    load_initial_url();

    last_snapshot_version_ = processor_.get_ui_snapshot_version();
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
                       .withWinWebView2Options(
                           juce::WebBrowserComponent::Options::WinWebView2{}
                               .withBuiltInErrorPageDisabled())
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
        final_failure_page_shown_ ||
        current_candidate_index_ >= candidate_urls_.size())
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

auto WebviewHost::handle_dev_server_load_failure(juce::String const &error_info)
    -> bool
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
        "xenBridgeEvent",
        parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
}

void WebviewHost::emit_transport_events()
{
    auto const transport_state = processor_.audio_thread_state_for_gui.read();
    if (!transport_state.transport_active)
    {
        if (!last_transport_active_)
        {
            return;
        }

        last_transport_active_ = false;
        auto const event_json = bridge_.make_transport_stopped_event_json();
        browser_->emitEventIfBrowserIsVisible(
            "xenBridgeEvent",
            parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
        return;
    }

    last_transport_active_ = true;
    auto const event_json = bridge_.make_phase_sync_event_json(
        {.phase = transport_state.loop_phase}, transport_state.daw.bpm);
    browser_->emitEventIfBrowserIsVisible(
        "xenBridgeEvent",
        parse_json_to_var_or_throw(event_json, "xenBridgeEvent"));
}

} // namespace xen::gui
