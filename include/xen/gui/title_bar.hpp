#pragma once

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
    auto resized() -> void override;

    auto paint(juce::Graphics &g) -> void override;

    auto lookAndFeelChanged() -> void override;

  private:
    juce::DrawablePath closed_menu_;
    juce::DrawablePath open_menu_;
    bool is_menu_open_ = false;
};

} // namespace xen::gui