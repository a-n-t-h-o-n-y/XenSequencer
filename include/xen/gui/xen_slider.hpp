#pragma once

#include <optional>
#include <string>

#include <signals_light/signal.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

/**
 * A Slider with a Label on top.
 */
class XenSlider : public juce::Component
{
  public:
    struct Metadata
    {
        std::string id;
        std::string display_name;
        float initial;
        float min;
        float max;
        std::optional<float> midpoint = std::nullopt; // skew;
    };

  public:
    juce::Slider slider;
    juce::Label label;

    sl::Signal<void()> on_release;
    sl::Signal<void(float)> on_change;

  public:
    XenSlider(Metadata const &data,
              juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal);

  public:
    void resized() override;

    void mouseUp(juce::MouseEvent const &e) override;
};

} // namespace xen::gui