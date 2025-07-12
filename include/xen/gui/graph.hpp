#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <xen/modulator.hpp>

namespace xen::gui
{

class Graph : public juce::Component
{
  public:
    struct Bounds
    {
        float x_min;
        float x_max;
        float y_min;
        float y_max;
    };

  public:
    Graph(Modulator modulator, Bounds bounds);

  public:
    void update(Modulator modulator, Bounds bounds);

    void paint(juce::Graphics &g) override;

  private:
    Modulator modulator_;
    Bounds bounds_;
    juce::Path path_;
};

} // namespace xen::gui