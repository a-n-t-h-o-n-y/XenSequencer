#include <xen/gui/accordion.hpp>

#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/color_ids.hpp>
#include <xen/gui/fonts.hpp>

namespace
{
/**
 * For AccordionTop.
 */
[[nodiscard]] auto create_triangle_path(bool open) -> juce::Path
{
    return open ? juce::Drawable::parseSVGPath(
                      "M504-480 320-664l56-56 240 240-240 240-56-56 184-184Z")
                : juce::Drawable::parseSVGPath(
                      "M560-240 320-480l240-240 56 56-184 184 184 184-56 56Z");
}

} // namespace

namespace xen::gui
{

VLabel::VLabel(juce::String text)
    : text_{std::move(text)}, letter_spacing_{0.f},
      font_{fonts::monospaced().regular.withHeight(15.f)}
{
}

void VLabel::set_text(juce::String text)
{
    text_ = std::move(text);
    this->repaint();
}

void VLabel::set_letter_spacing(float spacing)
{
    letter_spacing_ = spacing;
    this->repaint();
}

void VLabel::paint(juce::Graphics &g)
{
    g.fillAll(this->findColour((int)AccordionColorIDs::Background));
    g.setColour(this->findColour((int)AccordionColorIDs::Text));

    g.setFont(font_);
    auto const bounds = this->getLocalBounds();
    auto const font_height = font_.getHeight();

    auto const top_margin = 0;
    auto y = top_margin;

    for (auto const ch : text_)
    {
        g.drawText(juce::String::charToString(ch), bounds.getX(), y, bounds.getWidth(),
                   (int)font_height, juce::Justification::centred);
        y += (int)(font_height + letter_spacing_);
        if (y > bounds.getHeight())
        {
            break;
        }
    }
}

// -------------------------------------------------------------------------------------

AccordionTop::AccordionTop(juce::String title_)
    : title{std::move(title_)},
      toggle_button{"toggle_button", juce::DrawableButton::ButtonStyle::ImageFitted}
{
    toggle_button.setWantsKeyboardFocus(false);

    this->addAndMakeVisible(title);

    open_triangle_.setPath(create_triangle_path(true));
    closed_triangle_.setPath(create_triangle_path(false));

    toggle_button.setImages(&closed_triangle_);
    this->addAndMakeVisible(toggle_button);

    this->lookAndFeelChanged();
}

void AccordionTop::toggle()
{
    is_expanded_ = !is_expanded_;
    toggle_button.setImages(is_expanded_ ? &open_triangle_ : &closed_triangle_);
}

void AccordionTop::resized()
{
    auto flexbox = juce::FlexBox{};
    flexbox.flexDirection = juce::FlexBox::Direction::column;

    flexbox.items.add(juce::FlexItem{toggle_button}.withHeight(23.f));
    flexbox.items.add(juce::FlexItem{title}.withFlex(1.f));

    flexbox.performLayout(this->getLocalBounds());
}

void AccordionTop::lookAndFeelChanged()
{
    auto const background_color = this->findColour((int)AccordionColorIDs::Background);
    auto const triangle_color = this->findColour((int)AccordionColorIDs::Triangle);

    toggle_button.setColour(juce::DrawableButton::ColourIds::backgroundColourId,
                            background_color);

    open_triangle_.setFill(triangle_color);
    closed_triangle_.setFill(triangle_color);
    toggle_button.setImages(is_expanded_ ? &open_triangle_ : &closed_triangle_);
}

void AccordionTop::paintOverChildren(juce::Graphics &g)
{
    g.setColour(this->findColour((int)AccordionColorIDs::TitleUnderline));
    g.drawRect(this->getLocalBounds(), 1);
}

} // namespace xen::gui