#include <xen/gui/tile.hpp>

#include <string>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/fonts.hpp>

namespace xen::gui
{

Tile::Tile(std::string display, int margin) : display_{display}, margin_{margin}
{
}

void Tile::set(std::string display)
{
    display_ = display;
    this->repaint();
}

auto Tile::get() const -> std::string
{
    return display_;
}

void Tile::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().reduced(margin_);
    auto const font_size = (float)bounds.getHeight(); // Assuming square

    g.fillAll(this->findColour(background_color_id));
    g.setColour(this->findColour(text_color_id));
    g.setFont(this->font.withHeight(font_size));
    g.drawText(juce::String(display_), bounds, juce::Justification::centred, false);
}

// -------------------------------------------------------------------------------------

void ClickableTile::mouseUp(juce::MouseEvent const &event)
{
    if (event.mods.isLeftButtonDown())
    {
        this->clicked();
    }
}
} // namespace xen::gui