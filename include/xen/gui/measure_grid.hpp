#pragma once

#include <array>
#include <cstddef>
#include <iomanip>
#include <sstream>

#include <signals_light/signal.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

class MeasureGrid : public juce::Component
{
  public:
    // TODO emit this signal
    sl::Signal<void(std::size_t)> on_index_selected;

  public:
    // TODO set font to monospace
    // TODO add color ids for this class and assign them theme values.
    MeasureGrid()
    {
        auto i = 0;
        for (auto &btn : buttons_)
        {
            btn.setButtonText(int_to_hex(i));
            this->addAndMakeVisible(btn);
            ++i;
        }
    }

  public:
    void resized() override
    {
        juce::Grid grid;

        // Define the track sizes
        using Track = juce::Grid::TrackInfo;
        using namespace juce;
        grid.templateRows = {Track(1_fr), Track(1_fr), Track(1_fr), Track(1_fr)};
        grid.templateColumns = {Track(1_fr), Track(1_fr), Track(1_fr), Track(1_fr)};

        // Add items to the grid
        // TODO add items left to right, top to bottom
        // you want the index order to remain valid, this is only for visuals,
        // don't mix up the order of the buttons in the array.
        for (auto &button : buttons_)
        {
            grid.items.add(juce::GridItem(button));
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
    std::array<juce::TextButton, 16> buttons_;
};

} // namespace xen::gui
