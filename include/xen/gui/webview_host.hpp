#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <xen/webview_bridge.hpp>
#include <xen/xen_processor.hpp>

namespace xen::gui
{

class WebviewHost : public juce::Component, private juce::Timer
{
  public:
    explicit WebviewHost(XenProcessor &processor);
    ~WebviewHost() override;

  public:
    void resized() override;

  private:
    void timerCallback() override;

  private:
    [[nodiscard]] auto create_browser_options() -> juce::WebBrowserComponent::Options;

#if XEN_WEB_UI_USE_DEV_SERVER
    void load_current_dev_server_url();
    void load_next_dev_server_url();
    void handle_dev_server_load_success(juce::String const &url);
    auto handle_dev_server_load_failure(juce::String const &error_info) -> bool;
    void show_dev_server_error_page();
#endif

#if XEN_WEB_UI_USE_EMBEDDED
    [[nodiscard]] auto provide_embedded_resource(juce::String const &resource_path)
        const -> std::optional<juce::WebBrowserComponent::Resource>;
#endif

    void load_initial_url();
    void emit_state_changed_event();
    void emit_library_changed_event();
    void emit_keymap_changed_event();
    void emit_transport_events();

  private:
    XenProcessor &processor_;
    WebviewBridge bridge_;
    std::unique_ptr<juce::WebBrowserComponent> browser_;
#if XEN_WEB_UI_USE_DEV_SERVER
    std::vector<juce::String> candidate_urls_{};
    std::vector<juce::String> attempted_urls_{};
    std::size_t current_candidate_index_{0};
    bool dev_server_load_succeeded_{false};
    bool final_failure_page_shown_{false};
#endif
    ProjectRevision last_project_revision_{};
    LibraryRevision last_library_revision_{};
    std::uint64_t last_keymap_revision_{};
    std::uint64_t last_midi_fault_count_{};
    bool last_transport_active_{false};
};

} // namespace xen::gui
