#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <xen/state.hpp>

namespace xen::gui
{

class ReactWebView : public juce::Component
{
  public:
    ReactWebView(PluginState &ps);

  public:
    void resized() override;

  private:
    juce::WebBrowserComponent browser_;
};

} // namespace xen::gui