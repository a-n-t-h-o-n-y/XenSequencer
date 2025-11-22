#include <xen/gui/modulation_slider.hpp>

namespace xen::gui
{

void LEDButton::set_on(bool should_be_on)
{
    if (is_on_ != should_be_on)
    {
        is_on_ = should_be_on;
        this->on_toggle(is_on_);
        this->repaint();
    }
}

auto LEDButton::is_on() const -> bool
{
    return is_on_;
}

void LEDButton::paint(juce::Graphics &g)
{
    // Create a 16x16 square centered in the component
    auto const bounds = juce::Rectangle{16.f, 16.f}.withCentre(
        this->getLocalBounds().toFloat().getCentre());

    // Outer border
    g.setColour(is_on_ ? juce::Colour(0xff333333) : juce::Colour(0xff1a1a1a));
    g.fillRect(bounds);

    // LED square
    g.setColour(is_on_ ? juce::Colour(0xffef4444) : juce::Colour(0xff666666));
    g.fillRect(bounds.reduced(2));

    // Glow effect when on
    if (is_on_)
    {
        g.setColour(juce::Colour(0xffef4444).withAlpha(0.3f));
        g.fillRect(bounds.expanded(2));
    }
}

void LEDButton::mouseDown(juce::MouseEvent const &e)
{
    if (e.mods.isLeftButtonDown())
    {
        this->set_on(!is_on_);
    }
}

// -------------------------------------------------------------------------------------

BiasAmplitudeSlider::BiasAmplitudeSlider(Options const &options)
    : bias_min_{options.bias_min}, bias_max_{options.bias_max},
      bias_{(options.bias_min + options.bias_max) / 2}, amplitude_{0.f}
{
    if (bias_min_ >= bias_max_)
    {
        throw std::invalid_argument("bias_min must be less than bias_max.");
    }
}

void BiasAmplitudeSlider::set_bias_range(float min, float max)
{
    bias_min_ = min;
    bias_max_ = max;
    bias_ = std::clamp(bias_, bias_min_, bias_max_);
    auto const amp_abs = (bias_max_ - bias_min_) / 2.f;
    amplitude_ = std::clamp(amplitude_, -amp_abs, amp_abs);
    this->repaint();
}

auto BiasAmplitudeSlider::get_bias_range() const -> std::pair<float, float>
{
    return {bias_min_, bias_max_};
}

void BiasAmplitudeSlider::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().toFloat();
    auto const opacity = is_active_ ? 1.f : 0.4f;

    // Background
    g.setColour(juce::Colour{0xff2a2a2a});
    g.fillRoundedRectangle(bounds, 3.f);

    // Inner shadow
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.f, 1.f);

    // Center reference line (always at midpoint of min/max range)
    auto const center_x = bounds.getX() + bounds.getWidth() * 0.5f;
    g.setColour(juce::Colour{0xff666666}.withAlpha(opacity));
    g.drawLine(center_x, bounds.getY(), center_x, bounds.getBottom(), 1.f);

    // Calculate visual positions
    auto const bias_x = [&] {
        auto const range = bias_max_ - bias_min_;
        auto const normalized = (bias_ - bias_min_) / range;
        return bounds.getX() + bounds.getWidth() * normalized;
    }();

    auto const amp_width = [&] {
        auto const bias_range = bias_max_ - bias_min_;
        auto const normalized = std::abs(amplitude_) / bias_range;
        return bounds.getWidth() * normalized;
    }();

    auto const is_reversed = amplitude_ < 0.f;

    // Draw amplitude band
    if (std::abs(amplitude_) > 0.001f && is_active_)
    {
        auto const color =
            is_reversed ? juce::Colour{0xfff87171} : juce::Colour{0xff34d399};

        g.setColour(color.withAlpha(0.5f * opacity));

        auto const left = bias_x - amp_width;
        auto const width = amp_width * 2.f;

        g.fillRect(left, bounds.getY(), width, bounds.getHeight());
    }

    // Draw bias line
    g.setColour(juce::Colour{0xff60a5fa}.withAlpha(opacity));
    g.fillRect(bias_x - 1.f, bounds.getY(), 2.f, bounds.getHeight());

    // Bias line glow
    if (is_active_)
    {
        g.setColour(juce::Colour{0xff60a5fa}.withAlpha(0.3f));
        g.fillRect(bias_x - 2.f, bounds.getY(), 4.f, bounds.getHeight());
    }
}

void BiasAmplitudeSlider::mouseDown(juce::MouseEvent const &e)
{
    this->update_bias_from_mouse(e);
}

void BiasAmplitudeSlider::mouseDrag(juce::MouseEvent const &e)
{
    if (e.mods.isLeftButtonDown() && not amp_at_drag_start_.has_value())
    {
        amp_at_drag_start_ = amplitude_;
    }
    this->update_bias_from_mouse(e);
    this->update_amp_from_mouse(e);
}

void BiasAmplitudeSlider::mouseUp(juce::MouseEvent const &e)
{
    if (e.mods.isLeftButtonDown() && amp_at_drag_start_.has_value())
    {
        amp_at_drag_start_ = std::nullopt;
        this->on_release();
    }
}

void BiasAmplitudeSlider::mouseDoubleClick(juce::MouseEvent const &e)
{
    if (e.mods.isLeftButtonDown())
    {
        amplitude_ = 0.f;
        bias_ = (bias_min_ + bias_max_) * 0.5f;
        this->on_change(bias_, amplitude_);
        this->on_release();
        this->repaint();
    }
}

void BiasAmplitudeSlider::set_bias(float bias)
{
    bias_ = std::clamp(bias, bias_min_, bias_max_);
    this->repaint();
}

void BiasAmplitudeSlider::set_amplitude(float amp)
{
    amplitude_ = std::clamp(amp, bias_min_, bias_max_);
    this->repaint();
}

void BiasAmplitudeSlider::set_active(bool active)
{
    if (active != is_active_)
    {
        is_active_ = active;
        this->repaint();
    }
}

auto BiasAmplitudeSlider::get_bias() const -> float
{
    return bias_;
}

auto BiasAmplitudeSlider::get_amplitude() const -> float
{
    return amplitude_;
}

void BiasAmplitudeSlider::update_bias_from_mouse(juce::MouseEvent const &e)
{
    if (e.mods.isLeftButtonDown() && e.mods.isCommandDown())
    {
        auto const bounds = this->getLocalBounds().toFloat();
        auto const new_bias = [&] {
            auto const center_x = bounds.getX() + bounds.getWidth() * 0.5f;
            auto const delta_x = e.position.x - center_x;
            auto const half_width = bounds.getWidth() * 0.5f;

            auto const center_val = (bias_min_ + bias_max_) * 0.5f;
            auto const half_range = (bias_max_ - bias_min_) * 0.5f;
            return std::clamp(center_val + (delta_x / half_width) * half_range,
                              bias_min_, bias_max_);
        }();

        if (std::abs(new_bias - bias_) > 0.001f)
        {
            bias_ = new_bias;
            this->on_change(bias_, amplitude_);
            this->repaint();
        }
    }
}

void BiasAmplitudeSlider::update_amp_from_mouse(juce::MouseEvent const &e)
{

    if (e.mods.isLeftButtonDown() && !(e.mods.isCommandDown()) &&
        amp_at_drag_start_.has_value())
    {
        auto const bounds = this->getLocalBounds().toFloat();
        auto const amp_diff = [&] {
            auto const center_y = bounds.getY() + bounds.getHeight() * 0.5f;
            auto const delta_y = center_y - e.position.y;
            auto const amp_range = (bias_max_ - bias_min_) / 2.f;
            return delta_y / DRAG_RANGE * amp_range;
        }();

        if (std::abs(amp_diff) > 0.001f)
        {
            amplitude_ = *amp_at_drag_start_ + amp_diff;
            auto const amp_range = (bias_max_ - bias_min_) / 2.f;
            amplitude_ = std::clamp(amplitude_, -1.f * amp_range, amp_range);
            this->on_change(bias_, amplitude_);
            this->repaint();
        }
    }
}

// -------------------------------------------------------------------------------------

LFOModulationSlider::LFOModulationSlider(juce::String label_text,
                                         BiasAmplitudeSlider::Options const &options)
    : slider_{options}
{
    this->addAndMakeVisible(led_btn_);
    this->addAndMakeVisible(label_);
    this->addAndMakeVisible(slider_);

    // Configure label
    label_.setText(label_text, juce::dontSendNotification);
    label_.setFont(label_.getFont().withHeight(13.f));
    label_.setJustificationType(juce::Justification::centredLeft);
    label_.setInterceptsMouseClicks(false, false);

    // Connect LED button toggle to slider active state and label color
    led_btn_.on_toggle.connect([this](bool is_on) {
        slider_.set_active(is_on);
        this->update_label_color(is_on);
    });

    // Forward slider signals
    slider_.on_change.connect([this](float bias, float amplitude) {
        this->set_active(true);
        this->on_change(bias, amplitude);
    });

    slider_.on_release.connect([this]() { on_commit(); });

    // Initialize label color
    this->update_label_color(false);
}

void LFOModulationSlider::resized()
{
    auto fb = juce::FlexBox{};

    fb.flexDirection = juce::FlexBox::Direction::row;

    fb.items.add(juce::FlexItem{led_btn_}.withWidth(24.f));
    fb.items.add(juce::FlexItem{label_}.withFlex(1.f));
    fb.items.add(juce::FlexItem{slider_}.withFlex(2.f));

    fb.performLayout(this->getLocalBounds());
}

void LFOModulationSlider::set_bias(float bias)
{
    slider_.set_bias(bias);
}

void LFOModulationSlider::set_amplitude(float amp)
{
    slider_.set_amplitude(amp);
}

void LFOModulationSlider::set_active(bool active)
{
    led_btn_.set_on(active);
    slider_.set_active(active);
}

auto LFOModulationSlider::get_bias() const -> std::optional<float>
{
    return this->is_active() ? std::optional{slider_.get_bias()} : std::nullopt;
}

auto LFOModulationSlider::get_amplitude() const -> std::optional<float>
{
    return this->is_active() ? std::optional{slider_.get_amplitude()} : std::nullopt;
}

auto LFOModulationSlider::get_values() const -> std::optional<std::pair<float, float>>
{
    return this->is_active() ? std::optional{std::pair{
                                   slider_.get_bias(),
                                   slider_.get_amplitude(),
                               }}
                             : std::nullopt;
}

auto LFOModulationSlider::is_active() const -> bool
{
    return led_btn_.is_on();
}

auto LFOModulationSlider::get_bias_range() const -> std::pair<float, float>
{
    return slider_.get_bias_range();
}

void LFOModulationSlider::update_label_color(bool active)
{
    label_.setColour(juce::Label::textColourId,
                     active ? juce::Colours::white : juce::Colour(0xff888888));
}

void LFOModulationSlider::mouseUp(juce::MouseEvent const &e)
{
    //  Only triggered if slider is clicked.
    if (e.mods.isLeftButtonDown())
    {
        this->set_active(not this->is_active());
    }
}

} // namespace xen::gui