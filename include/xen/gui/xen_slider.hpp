#pragma once

#include <optional>
#include <string>

#include <signals_light/signal.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

// TODO onDragEnd: std::function<void()> is available on Slider type, instead of
// listening for mouse up?

// TODO single click text edit box by creating look and feel type for this that creates
// a textbox and setEditable with proper params.

/**
 * A Slider with a an on_release signal that is emitted when the user releases the
 * mouse after dragging the slider, in addition to an on_change signal that is
 * emitted whenever the slider value changes.
 */
class XenSlider : public juce::Component
{
  private:
    struct InternalSlider : juce::Slider
    {
        auto snapValue(double attemptedValue, DragMode drag_mode) -> double override;
    } slider; // TODO change to slider_ ?

  public:
    struct Metadata
    {
        // std::string id;
        // std::string display_name;
        float initial;
        float min;
        float max;
        std::optional<float> midpoint = std::nullopt; // skew;
    };

  public:
    sl::Signal<void()> on_release;
    sl::Signal<void(float)> on_change;

  public:
    XenSlider(Metadata const &data,
              juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal);

  public:
    // void paint(juce::Graphics &g) override;

    [[nodiscard]]
    auto get_value() const -> float
    {
        return (float)slider.getValue();
    }

    void resized() override;

    void mouseUp(juce::MouseEvent const &e) override;

    // float vertical_margin = 2.f;
    // float horizontal_margin = 2.f;
    // float border_thickness = 3.f;
};

} // namespace xen::gui