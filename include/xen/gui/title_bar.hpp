#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/constants.hpp>
#include <xen/gui/color_ids.hpp>

#include <embed_fonts.hpp>

namespace xen::gui
{

class TitleBar : public juce::Component
{
  public:
    juce::Label version;
    juce::Label title;
    juce::DrawableButton menu_button;

  public:
    TitleBar()
        : menu_button{"menu_button", juce::DrawableButton::ButtonStyle::ImageFitted}
    {
        this->addAndMakeVisible(version);
        this->addAndMakeVisible(title);
        this->addAndMakeVisible(menu_button);

        version.setText(juce::String{"v"} + VERSION, juce::dontSendNotification);
        version.setFont({
            juce::Font::getDefaultMonospacedFontName(),
            16.f,
            juce::Font::plain,
        });

        auto font = juce::Font{build_title_typeface()};
        font.setHeight(20.f);
        title.setFont(font);
        title.setJustificationType(juce::Justification::centred);
        title.setText("XenSequencer", juce::dontSendNotification);

        closed_menu_.setPath(juce::Drawable::parseSVGPath(
            "M120-240v-80h720v80H120Zm0-200v-80h720v80H120Zm0-200v-80h720v80H120Z"));
        open_menu_.setPath(juce::Drawable::parseSVGPath(
            "M120-240v-80h520v80H120Zm664-40L584-480l200-200 56 56-144 144 144 144-56 "
            "56ZM120-440v-80h400v80H120Zm0-200v-80h520v80H120Z"));

        this->lookAndFeelChanged();
    }

  public:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox();
        flexbox.flexDirection = juce::FlexBox::Direction::row;
        flexbox.items.add(juce::FlexItem(version).withWidth(60));
        flexbox.items.add(juce::FlexItem(title).withFlex(1));
        flexbox.items.add(juce::FlexItem(menu_button).withWidth(60));

        flexbox.performLayout(this->getLocalBounds());
    }

    auto lookAndFeelChanged() -> void override
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
        open_menu_.setFill(
            menu_button.findColour((int)DirectoryViewColorIDs::ItemText));
        closed_menu_.setFill(
            menu_button.findColour((int)DirectoryViewColorIDs::ItemText));
        menu_button.setImages(is_menu_open_ ? &open_menu_ : &closed_menu_);
    }

  private:
    [[nodiscard]] static auto build_title_typeface() -> juce::Typeface::Ptr
    {
        auto const font_data = embed_fonts::OswaldRegular_ttf;
        auto const font_data_size = embed_fonts::OswaldRegular_ttfSize;
        return juce::Typeface::createSystemTypefaceFor(
            font_data, static_cast<size_t>(font_data_size));
    }

  private:
    juce::DrawablePath closed_menu_;
    juce::DrawablePath open_menu_;
    bool is_menu_open_ = false;
};

} // namespace xen::gui