#pragma once

#include <array>
#include <cstddef>
#include <iomanip>
#include <sstream>

#include <juce_gui_basics/juce_gui_basics.h>

#include <signals_light/signal.hpp>

#include <xen/gui/color_ids.hpp>
#include <xen/gui/fonts.hpp>

namespace xen::gui
{

class SequenceSquare : public juce::TextButton
{
  public:
    // TODO add color ids for this class and assign them theme values.
    SequenceSquare()
    {
    }

  public:
    auto indicate() -> void
    {
        is_active_ = true;
        this->setColour(juce::TextButton::buttonColourId, this->get_color());
    }

    auto unindicate() -> void
    {
        is_active_ = false;
        this->setColour(juce::TextButton::buttonColourId, this->get_color());
    }

  public:
    auto lookAndFeelChanged() -> void override
    {
        this->setColour(juce::TextButton::buttonColourId, this->get_color());
    }

  private:
    [[nodiscard]] auto get_color() const -> juce::Colour
    {
        return is_active_ ? juce::Colours::red : juce::Colours::black;
    }

  private:
    bool is_active_{false};
};

class SequenceBankGrid : public juce::Component
{
  public:
    sl::Signal<void(std::size_t)> on_index_selected;

  public:
    SequenceBankGrid()
    {
        for (auto i = 0; i < buttons_.size(); ++i)
        {
            auto &btn = buttons_[i];

            btn.setButtonText(int_to_hex(i));
            btn.onClick = [this, i] { this->on_index_selected.emit(i); };
            this->addAndMakeVisible(btn);
        }
    }

  public:
    auto update_ui(std::size_t selected_index) -> void
    {
        for (auto &btn : buttons_)
        {
            btn.unindicate();
        }
        buttons_[selected_index].indicate();
    }

  public:
    void resized() override
    {
        constexpr auto length = 4;

        auto grid = juce::Grid{};

        // Define the track sizes
        using Track = juce::Grid::TrackInfo;
        using Fr = juce::Grid::Fr;
        for (auto i = 0; i < length; ++i)
        {
            grid.templateRows.add(Track(Fr(1)));
            grid.templateColumns.add(Track(Fr(1)));
        }

        // left to right, bottom to top
        for (auto row = 0; row < length; ++row)
        {
            for (auto col = 0; col < length; ++col)
            {
                std::size_t const index = 16 - (row * length) + (col - 4);
                grid.items.add(juce::GridItem(buttons_[index]));
            }
        }

        grid.performLayout(this->getLocalBounds());
    }

  private:
    static auto int_to_hex(int num) -> std::string
    {
        auto stream = std::stringstream{};
        stream << "0x" << std::uppercase << std::hex << num;
        return stream.str();
    }

  private:
    std::array<SequenceSquare, 16> buttons_;
};

} // namespace xen::gui
