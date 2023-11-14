#pragma once

#include <filesystem>
#include <map>
#include <string>

#include <juce_gui_basics/juce_gui_basics.h>
#include <signals_light/signal.hpp>

#include <xen/gui/plugin_window.hpp>
#include <xen/state.hpp>
#include <xen/xen_processor.hpp>
#include <xen/xen_timeline.hpp>

namespace xen
{

class XenEditor : public juce::AudioProcessorEditor
{
  public:
    explicit XenEditor(XenProcessor &);

  public:
    auto update(State const &, AuxState const &, Metadata const &) -> void;

  protected:
    auto resized() -> void override;

    auto paint(juce::Graphics &g) -> void override;

  private:
    XenTimeline const &timeline_;

    gui::PluginWindow plugin_window_;

    sl::Lifetime lifetime_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XenEditor)
};

} // namespace xen