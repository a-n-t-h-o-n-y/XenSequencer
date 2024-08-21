#pragma once

#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/color_ids.hpp>
#include <xen/gui/fonts.hpp>

namespace xen::gui
{

class VLabel : public juce::Component
{
  public:
    VLabel(juce::String const &text) : text_{text}
    {
    }

  public:
    auto set_text(juce::String const &text) -> void
    {
        text_ = text;
        this->repaint();
    }

    auto set_letter_spacing(float spacing) -> void
    {
        letter_spacing_ = spacing;
        this->repaint();
    }

  public:
    auto paint(juce::Graphics &g) -> void override
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
            g.drawText(juce::String::charToString(ch), bounds.getX(), y,
                       bounds.getWidth(), (int)font_height,
                       juce::Justification::centred);
            y += (int)(font_height + letter_spacing_);
            if (y > bounds.getHeight())
            {
                break;
            }
        }
    }

  private:
    juce::String text_;
    float letter_spacing_ = 0.f;
    juce::Font font_{fonts::monospaced().regular.withHeight(15.f)};
};

/**
 * Collapsible component with a title and a child component.
 */
template <typename ChildComponentType>
class HAccordion : public juce::Component
{
  private:
    class Top : public juce::Component
    {
      public:
        VLabel title;
        juce::DrawableButton toggle_button;

      public:
        explicit Top(juce::String const &title_)
            : title{title_},
              toggle_button{"toggle_button",
                            juce::DrawableButton::ButtonStyle::ImageFitted}
        {
            toggle_button.setWantsKeyboardFocus(false);

            this->addAndMakeVisible(title);

            open_triangle_.setPath(create_triangle_path(true));
            closed_triangle_.setPath(create_triangle_path(false));

            toggle_button.setImages(&closed_triangle_);
            this->addAndMakeVisible(toggle_button);

            this->lookAndFeelChanged();
        }

      public:
        auto toggle() -> void
        {
            is_expanded_ = !is_expanded_;
            toggle_button.setImages(is_expanded_ ? &open_triangle_ : &closed_triangle_);
        }

      protected:
        auto resized() -> void override
        {
            auto flexbox = juce::FlexBox{};
            flexbox.flexDirection = juce::FlexBox::Direction::column;

            flexbox.items.add(juce::FlexItem{toggle_button}.withHeight(23.f));
            flexbox.items.add(juce::FlexItem{title}.withFlex(1.f));

            flexbox.performLayout(this->getLocalBounds());
        }

        auto lookAndFeelChanged() -> void override
        {
            auto const background =
                this->findColour((int)AccordionColorIDs::Background);
            auto const text = this->findColour((int)AccordionColorIDs::Text);
            auto const triangle = this->findColour((int)AccordionColorIDs::Triangle);

            toggle_button.setColour(juce::DrawableButton::ColourIds::backgroundColourId,
                                    background);

            open_triangle_.setFill(triangle);
            closed_triangle_.setFill(triangle);
            toggle_button.setImages(is_expanded_ ? &open_triangle_ : &closed_triangle_);
        }

        auto paintOverChildren(juce::Graphics &g) -> void override
        {
            g.setColour(this->findColour((int)AccordionColorIDs::TitleUnderline));
            g.drawRect(this->getLocalBounds(), 1);
        }

      private:
        /**
         * Helper to create a triangle path.
         */
        [[nodiscard]] static auto create_triangle_path(bool open) -> juce::Path
        {
            return open ? juce::Drawable::parseSVGPath(
                              "M504-480 320-664l56-56 240 240-240 240-56-56 184-184Z")
                        : juce::Drawable::parseSVGPath(
                              "M560-240 320-480l240-240 56 56-184 184 184 184-56 56Z");
        }

      private:
        juce::DrawablePath open_triangle_;
        juce::DrawablePath closed_triangle_;
        bool is_expanded_ = true; // opposite, because we toggle on construction
    };

  private:
    Top top_;

  public:
    ChildComponentType child;

  public:
    template <typename... Args>
    explicit HAccordion(juce::String const &title, Args &&...args)
        : top_{title}, child{std::forward<Args>(args)...}
    {
        this->setWantsKeyboardFocus(false);

        this->addAndMakeVisible(top_);
        this->addAndMakeVisible(child);

        this->set_flexitem(juce::FlexItem{}.withFlex(1.f));

        top_.title.set_letter_spacing(1.f);
        top_.toggle_button.onClick = [this] { this->toggle_child_component(); };

        this->toggle_child_component();
    }

  public:
    /**
     * Set the flex item for the accordion used when expanded, for use by parent.
     */
    auto set_flexitem(juce::FlexItem flexitem) -> void
    {
        flexitem.associatedComponent = this;
        flexitem_ = std::move(flexitem);
    }

    /**
     * Get the flex item for the current stat of the accordion, for use by parent.
     */
    [[nodiscard]] auto get_flexitem() -> juce::FlexItem
    {
        return is_expanded_ ? flexitem_ : juce::FlexItem{*this}.withWidth(23.f);
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::row;

        flexbox.items.add(juce::FlexItem{child}.withFlex(1.f));
        flexbox.items.add(juce::FlexItem{top_}.withWidth(23.f));

        flexbox.performLayout(this->getLocalBounds());
    }

  private:
    auto toggle_child_component() -> void
    {
        top_.toggle();
        is_expanded_ = !is_expanded_;
        child.setVisible(is_expanded_);
        child.setEnabled(is_expanded_);
        if (auto *parent = getParentComponent(); parent != nullptr)
        {
            parent->resized();
        }
        this->resized();
    }

  private:
    bool is_expanded_ = true; // opposite, because we toggle on construction
    juce::FlexItem flexitem_{};
};

} // namespace xen::gui