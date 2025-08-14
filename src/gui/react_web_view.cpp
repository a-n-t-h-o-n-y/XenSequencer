#include <xen/gui/react_web_view.hpp>

#include <optional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <embed_reactui.hpp>

#include <xen/serialize.hpp>

namespace
{

/// false for debugging with external node server, true for embedded binary html.
auto const USE_EMBEDED_UI = false;
auto const DEV_ENDPOINT = juce::String{"http://localhost:5173"};

[[nodiscard]]
auto resource_provider(juce::String const &path)
    -> std::optional<juce::WebBrowserComponent::Resource>
{
    if (path == "/")
    {
        // Serve index.html
        return juce::WebBrowserComponent::Resource{
            .data = std::vector<std::byte>(
                reinterpret_cast<const std::byte *>(embed_reactui::index_html),
                reinterpret_cast<const std::byte *>(embed_reactui::index_html) +
                    embed_reactui::index_htmlSize),
            .mimeType = "text/html"};
    }
    return std::nullopt;
}

[[nodiscard]]
auto get_plugin_state_native_fn(xen::PluginState &ps)
    -> juce::WebBrowserComponent::NativeFunction
{
    return [&ps](juce::Array<juce::var> const &, auto complete) -> void {
        auto const json_str =
            juce::String{xen::serialize_plugin(ps.timeline.get_state().sequencer)};
        complete(json_str);
    };
}

[[nodiscard]]
auto build_browser_options(xen::PluginState &ps)
{
    return juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withWinWebView2Options(
            juce::WebBrowserComponent::Options::WinWebView2{}.withUserDataFolder(
                juce::File::getSpecialLocation(
                    juce::File::SpecialLocationType::tempDirectory)))
        .withNativeIntegrationEnabled()
        .withNativeFunction("get_plugin_state", get_plugin_state_native_fn(ps))
        .withResourceProvider(resource_provider, USE_EMBEDED_UI
                                                     ? std::nullopt
                                                     : std::optional{DEV_ENDPOINT});
}

} // namespace

namespace xen::gui
{

ReactWebView::ReactWebView(PluginState &ps) : browser_{build_browser_options(ps)}
{
    this->addAndMakeVisible(browser_);
    browser_.goToURL(USE_EMBEDED_UI ? browser_.getResourceProviderRoot()
                                    : DEV_ENDPOINT);
}

void ReactWebView::resized()
{
    browser_.setBounds(this->getLocalBounds());
}

} // namespace xen::gui