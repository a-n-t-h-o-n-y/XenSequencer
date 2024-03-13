#pragma once

#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

/**
 * Collapsible component with a title and a child component.
 */
template <typename ChildComponentType>
class Accordion : public juce::Component
{
  private:
    class Top : public juce::Component
    {
      public:
        juce::Label title;
        juce::DrawableButton toggle_button;

      public:
        explicit Top(juce::String const &title_)
            : title{"title", title_},
              toggle_button{"toggle_button",
                            juce::DrawableButton::ButtonStyle::ImageFitted}
        {
            toggle_button.setWantsKeyboardFocus(false);

            this->addAndMakeVisible(title);

            open_triangle_.setPath(create_triangle_path(true));
            closed_triangle_.setPath(create_triangle_path(false));
            open_triangle_.setFill(juce::Colours::white);
            closed_triangle_.setFill(juce::Colours::white);

            toggle_button.setImages(&closed_triangle_);
            this->addAndMakeVisible(toggle_button);

            toggle_button.setColour(juce::DrawableButton::ColourIds::backgroundColourId,
                                    juce::Colours::darkgrey);
            title.setColour(juce::Label::ColourIds::backgroundColourId,
                            juce::Colours::darkgrey);
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
            flexbox.flexDirection = juce::FlexBox::Direction::row;

            flexbox.items.add(juce::FlexItem{toggle_button}.withWidth(20.f));
            flexbox.items.add(juce::FlexItem{title}.withFlex(1.f));

            flexbox.performLayout(this->getLocalBounds());
        }

      private:
        /**
         * Helper to create a triangle path.
         */
        [[nodiscard]] static auto create_triangle_path(bool pointingDown) -> juce::Path
        {
            auto path = juce::Path{};
            if (pointingDown)
            {
                path.addTriangle(0.0f, 0.0f, 10.0f, 0.0f, 5.0f, 10.0f);
            }
            else
            {
                path.addTriangle(0.0f, 0.0f, 10.0f, 5.0f, 0.0f, 10.0f);
            }
            return path;
        }

      private:
        juce::DrawablePath open_triangle_;
        juce::DrawablePath closed_triangle_;
        bool is_expanded_ = false;
    };

  private:
    Top top_;

  public:
    ChildComponentType child;

  public:
    template <typename... Args>
    explicit Accordion(juce::String const &title, Args &&...args)
        : top_{title}, child{std::forward<Args>(args)...}
    {
        this->setWantsKeyboardFocus(false);

        this->addAndMakeVisible(top_);
        this->addAndMakeVisible(child);

        top_.toggle_button.onClick = [this] { this->toggle_child_component(); };

        this->toggle_child_component();
    }

  public:
    [[nodiscard]] auto get_flexitem() -> juce::FlexItem
    {
        if (is_expanded_)
        {
            return juce::FlexItem{*this}.withFlex(0.5f);
        }
        else
        {
            return juce::FlexItem{*this}.withHeight(20.f);
        }
    }

  protected:
    auto resized() -> void override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::column;

        flexbox.items.add(juce::FlexItem{top_}.withHeight(20.f));
        flexbox.items.add(juce::FlexItem{child}.withFlex(1.f));

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
    bool is_expanded_ = true;
};

} // namespace xen::gui