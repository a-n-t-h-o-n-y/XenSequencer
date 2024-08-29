#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

class TitleBar : public juce::Component
{
  public:
    juce::Label version;
    juce::Label title;
    juce::DrawableButton menu_button;

  public:
    TitleBar();

  public:
    void resized() override;

    void paint(juce::Graphics &g) override;

    void lookAndFeelChanged() override;

  private:
    juce::DrawablePath closed_menu_;
    juce::DrawablePath open_menu_;
    bool is_menu_open_ = false;
};

} // namespace xen::gui