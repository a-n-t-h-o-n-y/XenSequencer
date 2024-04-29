#include <xen/gui/sequence.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <variant>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>

#include <xen/gui/color_ids.hpp>
#include <xen/state.hpp>
#include <xen/utility.hpp>

namespace
{

using namespace xen;

/**
 * Computes the corner radius for a rectangle based on its width.
 *
 * @param bounds The bounds of the rectangle.
 * @param min_radius The minimum corner radius.
 * @param max_radius The maximum corner radius.
 * @return The computed corner radius for the rectangle.
 */
[[nodiscard]] auto compute_corner_radius(juce::Rectangle<float> const &bounds,
                                         float const min_radius,
                                         float const max_radius) -> float
{
    auto const width = bounds.getWidth();
    return juce::jlimit(min_radius, max_radius,
                        juce::jmap(width, 30.f, 200.f, min_radius, max_radius));
}

/**
 * Computes the Rectangle bounds for a given note interval and tuning length.
 *
 * @param component_bounds  The bounds of the component in which the note will
 * be displayed.
 * @param note_interval The note interval, to be used in modulo calculation.
 * @param tuning_length The tuning length, used for scaling.
 * @return The Rectangle that represents the position and size of the note.
 *
 * @exception std::invalid_argument If tuning_length is zero, to prevent
 * division by zero.
 */
[[nodiscard]] auto compute_note_bounds(juce::Rectangle<float> const &component_bounds,
                                       int note_interval,
                                       size_t tuning_length) -> juce::Rectangle<float>
{
    if (tuning_length == 0)
    {
        throw std::invalid_argument("Tuning length must not be zero.");
    }

    auto const normalized = normalize_interval(note_interval, tuning_length);

    // Calculate note height
    auto const note_height = component_bounds.getHeight() / (float)tuning_length;

    // Calculate note y-position from the bottom
    auto const y_position =
        component_bounds.getBottom() - ((float)normalized * note_height);

    return juce::Rectangle<float>{
        component_bounds.getX(),
        y_position - note_height,
        component_bounds.getWidth(),
        note_height,
    };
}

[[nodiscard]] auto from_gradient(float value, float min, float max,
                                 juce::LookAndFeel const &laf) -> juce::Colour
{
    juce::Colour startColor = laf.findColour((int)gui::NoteColorIDs::IntervalLow);
    juce::Colour middleColor = laf.findColour((int)gui::NoteColorIDs::IntervalMid);
    juce::Colour endColor = laf.findColour((int)gui::NoteColorIDs::IntervalHigh);

    juce::ColourGradient gradient;
    gradient.isRadial = false;
    gradient.point1 = {0, 0};
    gradient.point2 = {0, 100};

    gradient.addColour(0.0, startColor);
    gradient.addColour(0.43, middleColor);
    gradient.addColour(1.0, endColor);
    value = std::clamp(value, min, max);

    auto normalized_position = (value - min) / (max - min);

    return gradient.getColourAtPosition(normalized_position);
}

} // namespace

namespace xen::gui
{

Sequence::Sequence(sequence::Sequence const &seq, SequencerState const &state)
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
    g.setColour(this->findColour((int)MeasureColorIDs::Background));
    g.fillAll();

    // Paint Background Rectangle ------------------------------------------------
    constexpr auto max_radius = 25.f;
    constexpr auto min_radius = 10.f;

    auto const bounds = getLocalBounds().toFloat().reduced(1.f, 3.f);
    auto const corner_radius = compute_corner_radius(bounds, min_radius, max_radius);

    auto base_path = juce::Path{};
    base_path.addRoundedRectangle(bounds, corner_radius);

    g.setColour(bg_color_);
    g.fillPath(base_path);

    // Reduce Paint Region to base_path -----------------------------------------
    g.reduceClipRegion(base_path);

    // Paint Note Interval ------------------------------------------------------
    auto const interval_bounds = compute_note_bounds(bounds, interval_, tuning_length_);
    auto const note_color = from_gradient((float)get_octave(interval_, tuning_length_),
                                          -4.f, 4.f, this->getLookAndFeel());

    g.setColour(note_color);
    g.fillRect(interval_bounds);
    // Paint Interval Text ------------------------------------------------------
    {
        auto const interval_text =
            juce::String(normalize_interval(interval_, tuning_length_));

        auto font_size = std::min(16.f, interval_bounds.getHeight());
        auto font = juce::Font{"Arial", "Normal", font_size}.boldened();
        g.setFont(font);
        g.setColour(this->findColour((int)NoteColorIDs::IntervalText));

        auto const margin = std::max(
            0.f, corner_radius - ((float)font.getStringWidth(interval_text) / 2.f));

        // Draw the interval text aligned to the far left and vertically centered.
        g.drawText(interval_text, (int)(interval_bounds.getX() + margin),
                   (int)interval_bounds.getY(),
                   (int)(interval_bounds.getWidth() - margin),
                   (int)interval_bounds.getHeight(), juce::Justification::centredLeft);
    }
    // Paint Octave Text --------------------------------------------------------
    {
        g.setFont(juce::Font{"Arial", "Normal", 16.f}.boldened());

        auto const octave = get_octave(interval_, tuning_length_);

        auto octave_text = (octave >= 0 ? "+" : "") + juce::String(octave) + " oct";
        if ((float)g.getCurrentFont().getStringWidth(octave_text) > bounds.getWidth())
        {
            octave_text = (octave >= 0 ? "+" : "") + juce::String(octave);
        }

        g.setColour(this->findColour((int)NoteColorIDs::OctaveText));
        g.drawText(octave_text, this->getLocalBounds(), juce::Justification::centred);
    }
}

} // namespace xen::gui