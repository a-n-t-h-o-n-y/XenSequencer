#pragma once

#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/tile.hpp>

namespace xen::gui
{

/**
 * Vertical label for use with HAccordion.
 */
class VLabel : public juce::Component
{
  public:
    explicit VLabel(juce::String text);

  public:
    void set_text(juce::String text);

    /**
     * Set the number of pixels between letters.
     */
    void set_letter_spacing(float spacing);

  public:
    void paint(juce::Graphics &g);

  private:
    juce::String text_;
    float letter_spacing_;
    juce::Font font_;
};

// -------------------------------------------------------------------------------------

class AccordionTop : public juce::Component
{
  public:
    ClickableTile toggle_button;
    VLabel title;

  public:
    explicit AccordionTop(juce::String title_);

  public:
    /**
     * Toggle the button imagery between opened and closed.
     */
    void toggle();

  protected:
    void resized() override;

    void paintOverChildren(juce::Graphics &g) override;

  private:
    bool is_expanded_ = true; // Opposite, because we toggle on construction
};

// -------------------------------------------------------------------------------------

/**
 * Collapsible component with a title and a child component.
 */
template <typename ChildComponentType>
class HAccordion : public juce::Component
{
  private:
    AccordionTop top_;

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
        top_.toggle_button.clicked.connect([this] { this->toggle_child_component(); });

        this->toggle_child_component();
    }

  public:
    /**
     * Set the flex item for the accordion used when expanded, for use by parent.
     */
    void set_flexitem(juce::FlexItem flexitem)
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
    void resized() override
    {
        auto flexbox = juce::FlexBox{};
        flexbox.flexDirection = juce::FlexBox::Direction::row;

        flexbox.items.add(juce::FlexItem{child}.withFlex(1.f));
        flexbox.items.add(juce::FlexItem{top_}.withWidth(23.f));

        flexbox.performLayout(this->getLocalBounds());
    }

  private:
    void toggle_child_component()
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