#pragma once

#include <optional>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

namespace xen::gui
{

// ============================================================================
// LED Toggle Button Component
// ============================================================================
class LEDButton : public juce::Component
{
  public:
    sl::Signal<void(bool)> on_toggle;

  public:
    void set_on(bool should_be_on);

    [[nodiscard]]
    auto is_on() const -> bool;

  public:
    void paint(juce::Graphics &g) override;

    void mouseDown(juce::MouseEvent const &e) override;

  private:
    bool is_on_ = false;
};

// ============================================================================
// Bias/Amplitude Slider Component
// ============================================================================
class BiasAmplitudeSlider : public juce::Component
{
  public:
    sl::Signal<void(float, float)> on_change; // (bias, amplitude)
    sl::Signal<void()> on_release;

    struct Options
    {
        float bias_min;
        float bias_max;
        float initial_bias;
        float initial_amplitude;
    };

  public:
    /// Slider range is defined by the bias range. Amplitude max is half of bias range.
    BiasAmplitudeSlider(Options const &options);

    void set_bias_range(float min, float max);

    /// std::pair<min, max>
    [[nodiscard]]
    auto get_bias_range() const -> std::pair<float, float>;

  public:
    void paint(juce::Graphics &g) override;

    void mouseDown(juce::MouseEvent const &e) override;

    void mouseDrag(juce::MouseEvent const &e) override;

    void mouseUp(juce::MouseEvent const &e) override;

    void mouseDoubleClick(juce::MouseEvent const &e) override;

  public:
    void set_bias(float bias);

    void set_amplitude(float amp);

    void set_active(bool active);

    auto get_bias() const -> float;

    auto get_amplitude() const -> float;

  private:
    /// Does complete update of bias, including checking of left btn and ctrl/cmd.
    void update_bias_from_mouse(juce::MouseEvent const &e);

    /// Does complete update of amplitude, including checking of left btn and !ctrl/cmd.
    void update_amp_from_mouse(juce::MouseEvent const &e);

  private:
    float bias_min_;
    float bias_max_;
    float bias_;
    float amplitude_;
    bool is_active_ = true;
    bool is_dragging_ = false;
    std::optional<float> amp_at_drag_start_ = std::nullopt;
    static constexpr float DRAG_RANGE = 200.f;
};

// ============================================================================
// LFO Modulation Slider - Parent Component
// ============================================================================
class LFOModulationSlider : public juce::Component
{
  public:
    sl::Signal<void(float, float)> on_change; // (bias, amplitude)
    sl::Signal<void()> on_commit;

  public:
    LFOModulationSlider(juce::String label_text,
                        BiasAmplitudeSlider::Options const &options);

    void resized() override;

  public:
    void set_bias(float bias);

    void set_amplitude(float amp);

    void set_active(bool active);

    /// Returns std::nullopt if not active.
    [[nodiscard]]
    auto get_bias() const -> std::optional<float>;

    /// Returns std::nullopt if not active.
    [[nodiscard]]
    auto get_amplitude() const -> std::optional<float>;

    // TODO make this into a struct with member names
    /// std::pair<bias, amplitude>
    [[nodiscard]]
    auto get_values() const -> std::optional<std::pair<float, float>>;

    [[nodiscard]]
    auto is_active() const -> bool;

    /// std::pair<min, max>
    [[nodiscard]]
    auto get_bias_range() const -> std::pair<float, float>;

  public:
    void mouseUp(juce::MouseEvent const &e) override;

  private:
    void update_label_color(bool is_active);

    LEDButton led_btn_;
    juce::Label label_;
    BiasAmplitudeSlider slider_;
};

} // namespace xen::gui