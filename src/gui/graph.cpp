#include <xen/gui/graph.hpp>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/gui/themes.hpp>

namespace
{

[[nodiscard]]
auto calculate_path(xen::Modulator const &mod, xen::gui::Graph::Bounds const &bounds,
                    juce::Rectangle<int> const &component_bounds) -> juce::Path
{
    return {};
}

} // namespace

namespace xen::gui
{

Graph::Graph(Modulator modulator, Bounds bounds)
    : modulator_{std::move(modulator)}, bounds_{bounds}
{
}

void Graph::update(Modulator modulator, Bounds bounds)
{
    modulator_ = std::move(modulator);
    bounds_ = bounds;

    path_ = calculate_path(modulator_, bounds_, this->getLocalBounds());
    this->repaint();
}

void Graph::paint(juce::Graphics &g)
{
    g.fillAll(this->findColour(ColorID::BackgroundHigh));
    g.setColour(juce::Colours::cyan);
    g.strokePath(path_, juce::PathStrokeType(2.0f));
}

} // namespace xen::gui