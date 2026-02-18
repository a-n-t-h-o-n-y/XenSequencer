#pragma once

#include <memory>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/webview_host.hpp>
#include <xen/xen_processor.hpp>

namespace xen::gui
{

class XenEditor : public juce::AudioProcessorEditor
{
  public:
    explicit XenEditor(XenProcessor &, int width, int height);
    ~XenEditor() override = default;

  public:
    void resized() override;

    [[nodiscard]] auto createKeyboardFocusTraverser()
        -> std::unique_ptr<juce::ComponentTraverser> override;

  private:
    XenProcessor &processor_;
    std::unique_ptr<WebviewHost> webview_host_;
};

} // namespace xen::gui
