#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <utility>

#include <signals_light/signal.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>

namespace xen::gui
{

template <typename... Children>
class TabGroup : public juce::Component
{
  private:
    class Tab : public juce::Component
    {
      public:
        sl::Signal<void()> on_select;

      public:
        explicit Tab(juce::String label) : label_{std::move(label)}
        {
        }

      public:
        void set_selected(bool selected)
        {
            is_selected_ = selected;
            this->repaint();
        }

        [[nodiscard]]
        auto get_preferred_width() const -> int
        {
            auto const font = juce::Font{juce::FontOptions{15.f}};
            juce::GlyphArrangement ga;
            ga.addLineOfText(font, label_, 0.0f, 0.0f);
            auto const text_width =
                (int)ga.getBoundingBox(0, label_.length(), true).getWidth();
            auto const margin = 16;
            return text_width + margin;
        }

      protected:
        void paint(juce::Graphics &g) override
        {
            auto const background_color_id =
                is_selected_ ? ColorID::Background : ColorID::BackgroundLow;
            g.fillAll(this->findColour(background_color_id));

            g.setColour(this->findColour(ColorID::ForegroundHigh));
            g.setFont(juce::Font{juce::FontOptions{15.f}});
            g.drawText(label_, this->getLocalBounds(), juce::Justification::centred);
        }

        void mouseUp(juce::MouseEvent const &event) override
        {
            if (event.mods.isLeftButtonDown())
            {
                this->on_select();
            }
        }

      private:
        juce::String label_;
        bool is_selected_ = false;
    };

  public:
    std::tuple<Children...> children;

  public:
    /// Default constructs children, juce::Components are non-movable.
    template <typename... Labels>
    explicit TabGroup(Labels &&...labels)
        requires(sizeof...(Labels) == sizeof...(Children))
        : children{}, tabs_{Tab{labels}...}
    {
        this->setWantsKeyboardFocus(false);

        this->add_children_and_tabs();
        this->connect_tab_signals(std::index_sequence_for<Children...>{});

        if constexpr (sizeof...(Children) > 0)
        {
            this->set_selected(std::get<0>(children));
        }
    }

  protected:
    void resized() override
    {
        auto const tab_height = 24;
        auto const margin = 4;

        auto tab_bar_bounds = this->getLocalBounds().removeFromTop(tab_height);
        auto content_bounds =
            this->getLocalBounds().withTrimmedTop(tab_height + margin);

        auto tab_flexbox = juce::FlexBox{};
        tab_flexbox.flexDirection = juce::FlexBox::Direction::row;
        tab_flexbox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

        for (auto &tab : tabs_)
        {
            auto const width = static_cast<float>(tab.get_preferred_width());
            tab_flexbox.items.add(juce::FlexItem{tab}.withWidth(width).withHeight(
                static_cast<float>(tab_height)));
        }

        tab_flexbox.performLayout(tab_bar_bounds);

        std::apply([&content_bounds](
                       auto &...child) { (child.setBounds(content_bounds), ...); },
                   children);
    }

  private:
    void add_children_and_tabs()
    {
        std::apply([this](auto &...child) { (this->addAndMakeVisible(child), ...); },
                   children);

        for (auto &tab : tabs_)
        {
            this->addAndMakeVisible(tab);
        }
    }

    template <std::size_t... Is>
    void connect_tab_signals(std::index_sequence<Is...>)
    {
        (tabs_[Is].on_select.connect(
             [this] { this->set_selected(std::get<Is>(children)); }),
         ...);
    }

    void set_selected(juce::Component &child)
    {
        std::apply(
            [&child](auto &...c) {
                ((c.setVisible(&c == &child), c.setEnabled(&c == &child)), ...);
            },
            children);

        auto index = std::size_t{0};
        std::apply(
            [&child, &index, this](auto &...c) {
                ((tabs_[index++].set_selected(&c == &child)), ...);
            },
            children);

        this->resized();
    }

  private:
    std::array<Tab, sizeof...(Children)> tabs_;
};

} // namespace xen::gui