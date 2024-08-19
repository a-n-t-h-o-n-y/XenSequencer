#include <xen/gui/title_bar.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/constants.hpp>
#include <xen/gui/color_ids.hpp>
#include <xen/gui/fonts.hpp>

namespace xen::gui
{

TitleBar::TitleBar()
    : menu_button{"menu_button", juce::DrawableButton::ButtonStyle::ImageFitted}
{
    this->addAndMakeVisible(version);
    this->addAndMakeVisible(title);
    this->addAndMakeVisible(menu_button);

    version.setFont(fonts::monospaced().regular.withHeight(16.f));
    version.setText(juce::String{"v"} + VERSION, juce::dontSendNotification);

    title.setFont(fonts::monospaced().semi_bold.withHeight(18.f));
    title.setJustificationType(juce::Justification::centred);
    title.setText("XenSequencer", juce::dontSendNotification);

    closed_menu_.setPath(juce::Drawable::parseSVGPath(
        "M120-240v-80h720v80H120Zm0-200v-80h720v80H120Zm0-200v-80h720v80H120Z"));
    open_menu_.setPath(juce::Drawable::parseSVGPath(
        "M120-240v-80h520v80H120Zm664-40L584-480l200-200 56 56-144 144 144 144-56 "
        "56ZM120-440v-80h400v80H120Zm0-200v-80h520v80H120Z"));

    this->lookAndFeelChanged();
}

auto TitleBar::resized() -> void
{
    auto flexbox = juce::FlexBox();
    flexbox.flexDirection = juce::FlexBox::Direction::row;
    flexbox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

    flexbox.items.add(juce::FlexItem(version).withWidth(60));
    flexbox.items.add(juce::FlexItem(title).withWidth(100));
    flexbox.items.add(juce::FlexItem(menu_button).withWidth(23));

    flexbox.performLayout(this->getLocalBounds());
}

auto TitleBar::paint(juce::Graphics &g) -> void
{
    g.fillAll(title.findColour((int)DirectoryViewColorIDs::ItemBackground));
}

auto TitleBar::lookAndFeelChanged() -> void
{
    version.setColour(juce::Label::textColourId,
                      title.findColour((int)DirectoryViewColorIDs::ItemText));
    version.setColour(juce::Label::backgroundColourId,
                      title.findColour((int)DirectoryViewColorIDs::ItemBackground));

    title.setColour(juce::Label::textColourId,
                    title.findColour((int)DirectoryViewColorIDs::ItemText));
    title.setColour(juce::Label::backgroundColourId,
                    title.findColour((int)DirectoryViewColorIDs::ItemBackground));

    menu_button.setColour(
        juce::DrawableButton::ColourIds::backgroundColourId,
        menu_button.findColour((int)DirectoryViewColorIDs::ItemBackground));
    open_menu_.setFill(menu_button.findColour((int)DirectoryViewColorIDs::ItemText));
    closed_menu_.setFill(menu_button.findColour((int)DirectoryViewColorIDs::ItemText));
    menu_button.setImages(is_menu_open_ ? &open_menu_ : &closed_menu_);
}

} // namespace xen::gui