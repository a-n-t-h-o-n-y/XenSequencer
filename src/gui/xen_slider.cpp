#include <xen/gui/xen_slider.hpp>

#include <xen/gui/fonts.hpp>

namespace xen::gui
{

XenSlider::XenSlider(Metadata const &data, juce::Slider::SliderStyle style)
{
    if (data.initial < data.min || data.initial > data.max)
    {
        throw std::invalid_argument{
            "XenSlider: Initial value must be within min and max."};
    }
    if (data.midpoint.has_value() &&
        (*data.midpoint < data.min || *data.midpoint > data.max))
    {
        throw std::invalid_argument{"XenSlider: Midpoint must be within min and max."};
    }

    this->addAndMakeVisible(label);
    this->addAndMakeVisible(slider);

    label.setText(data.display_name, juce::dontSendNotification);
    label.setFont(fonts::monospaced().bold.withHeight((float)label.getHeight() * 0.7f));
    label.setJustificationType(juce::Justification::centred);

    slider.setComponentID(data.id);
    slider.setRange(data.min, data.max);
    slider.setValue(data.initial);
    if (data.midpoint.has_value())
    {
        slider.setSkewFactorFromMidPoint(*data.midpoint);
    }

    slider.setNumDecimalPlacesToDisplay(2);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                           slider.getTextBoxWidth() / 2, slider.getTextBoxHeight());
    slider.setSliderStyle(style);

    slider.onValueChange = [this] { this->on_change((float)slider.getValue()); };

    slider.addMouseListener(this, false);
}

void XenSlider::resized()
{
    auto fb = juce::FlexBox{};

    fb.flexDirection = juce::FlexBox::Direction::column;
    fb.items.add(juce::FlexItem{label}.withFlex(1.f));
    fb.items.add(juce::FlexItem{slider}.withFlex(2.f));

    fb.performLayout(this->getLocalBounds());

    label.setFont(fonts::monospaced().bold.withHeight((float)label.getHeight() * 0.7f));
    // slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
    //                        slider.getTextBoxWidth() / 2, slider.getTextBoxHeight());
}

void XenSlider::mouseUp(const juce::MouseEvent &e)
{
    if (e.eventComponent == &slider && e.mods.isLeftButtonDown())
    {
        this->on_release();
    }
}

} // namespace xen::gui