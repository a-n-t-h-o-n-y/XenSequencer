#include <xen/gui/sequence.hpp>

#include <cmath>
#include <cstddef>
#include <variant>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>

#include <xen/state.hpp>

namespace xen::gui
{

Sequence::Sequence(sequence::Sequence const &seq, State const &state)
    : cells_{juce::FlexItem{}.withFlex(1.f), false}
{
    this->addAndMakeVisible(top_indicator_);
    this->addAndMakeVisible(cells_);
    this->addAndMakeVisible(bottom_indicator_);

    auto const build_and_allocate_cell = BuildAndAllocateCell{state};

    // for each sequence::Cell, construct it as a pointer and add it to cells_
    for (auto const &cell : seq.cells)
    {
        cells_.push_back(std::visit(build_and_allocate_cell, cell));
    }
}

auto NoteInterval::paint(juce::Graphics &g) -> void
{
    constexpr auto max_radius = 25.f;
    constexpr auto min_radius = 10.f;

    auto const bounds = getLocalBounds().toFloat().reduced(1.f, 3.f);
    auto const width = static_cast<float>(bounds.getWidth());
    auto const corner_radius = juce::jlimit(
        min_radius, max_radius, juce::jmap(width, 30.f, 200.f, min_radius, max_radius));

    g.setColour(bg_color_);
    g.fillRoundedRectangle(bounds, corner_radius);

    // -------------------------------------------------------------------------

    // Draw the horizontal interval line

    auto computeXMargin = [corner_radius, bounds](float y) -> float {
        // Calculate the distance from the top or bottom, whichever is closer
        auto const distance_to_edge =
            std::min(y - bounds.getY(), bounds.getBottom() - y);

        // If the distance to the edge is greater than the corner_radius, it's outside
        // the rounded part
        if (distance_to_edge >= corner_radius)
        {
            return bounds.getTopLeft().getX();
        }

        // If the distance to the edge is inside the rounded part, use the Pythagorean
        // theorem to compute x margin
        auto const triangle_opposite = corner_radius - distance_to_edge;
        auto const triangle_adjacent = std::sqrt(corner_radius * corner_radius -
                                                 triangle_opposite * triangle_opposite);

        return corner_radius - triangle_adjacent + bounds.getTopLeft().getX();
    };

    constexpr auto interval_distance = 3.f;

    auto const centerY = static_cast<float>(this->getHeight()) / 2.f;
    auto const offsetY = static_cast<float>(-interval_) * interval_distance;
    auto const lineWidth = static_cast<float>(this->getWidth());
    auto const lineHeight = 1.f; // One pixel high

    // Draw Horizontal Interval Line
    auto const intervalY = centerY + offsetY;
    auto const xMarginInterval = computeXMargin(intervalY);
    if (interval_ != 0 && (std::size_t)std::abs(interval_) % tuning_length_ == 0)
    {
        g.setColour(juce::Colours::khaki);
    }
    else
    {
        g.setColour(juce::Colours::black);
    }
    g.fillRect(xMarginInterval, intervalY, lineWidth - 2 * xMarginInterval, lineHeight);

    // -------------------------------------------------------------------------

    // define text and line characteristics
    auto const font = juce::Font{16.f};
    g.setFont(font);

    auto const text_color = juce::Colours::black;
    // auto const line_thickness = 2.f;
    // auto const padding = 10;

    auto const [adjusted_interval, octave] =
        NoteInterval::get_interval_and_octave(interval_, tuning_length_);

    auto const interval_text = juce::String(adjusted_interval);
    auto const octave_text = (octave >= 0 ? "+" : "") + juce::String(octave) + " oct";
    auto complete_text = juce::String{};
    if (interval_ != 0)
    {
        complete_text += interval_text;
    }
    if (octave != 0)
    {
        complete_text += " " + octave_text;
    }

    auto text_width = font.getStringWidth(complete_text);
    auto const text_height = font.getHeight();

    if (text_width > this->getWidth())
    {
        complete_text = juce::String(interval_);
        text_width = font.getStringWidth(complete_text);
    }
    if (text_width > this->getWidth())
    {
        complete_text = juce::String{};
        text_width = font.getStringWidth(complete_text);
    }

    auto const x = (this->getWidth() - text_width) / 2.f;
    auto const y = (this->getHeight() - text_height) / 2.f;

    // Draw center line with gap
    if (interval_ != 0)
    {
        g.setColour(juce::Colours::white);
        auto const center_line_bounds = juce::Rectangle<float>{
            xMarginInterval, centerY, (float)getWidth() - (2 * xMarginInterval),
            (float)lineHeight};
        auto const text_bounds =
            juce::Rectangle<float>{x, y, (float)text_width, (float)text_height};
        auto const left_line_bounds = juce::Rectangle<float>{
            center_line_bounds.getX(), centerY,
            text_bounds.getX() - center_line_bounds.getX(), lineHeight};
        auto const right_line_bounds = juce::Rectangle<float>{
            text_bounds.getRight(), centerY,
            center_line_bounds.getRight() - text_bounds.getRight(), lineHeight};
        g.fillRect(left_line_bounds);
        g.fillRect(right_line_bounds);
    }

    g.setColour(text_color);
    g.drawText(complete_text, x, y, text_width, text_height,
               juce::Justification::centred);
}

} // namespace xen::gui