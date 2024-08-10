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

auto const corner_radius = 10.f;

/**
 * Computes the Rectangle bounds for a given note interval and tuning length.
 *
 * @param bounds  The bounds of the component in which the note will be displayed.
 * @param note The note.
 * @param tuning_length The tuning length, used for scaling.
 * @return The Rectangle that represents the position and size of the note.
 *
 * @exception std::invalid_argument If tuning_length is zero, to prevent
 * division by zero.
 */
[[nodiscard]] auto compute_note_bounds(juce::Rectangle<float> const &bounds,
                                       sequence::Note note, std::size_t tuning_length)
    -> juce::Rectangle<float>
{
    if (tuning_length == 0)
    {
        throw std::invalid_argument("Tuning length must not be zero.");
    }

    auto const normalized = normalize_interval(note.interval, tuning_length);

    // Calculate note height
    auto const note_height = bounds.getHeight() / (float)tuning_length;

    // Calculate note y-position from the bottom
    auto const y_position = bounds.getBottom() - ((float)normalized * note_height);

    // Calculate the note x and width
    auto const left_x = bounds.getX() + bounds.getWidth() * note.delay;
    auto const note_width = bounds.getWidth() * note.gate;

    return juce::Rectangle<float>{
        left_x,
        y_position - note_height,
        note_width,
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

void draw_staff(juce::Graphics &g, juce::Rectangle<float> bounds,
                std::size_t interval_count, juce::Colour lighter_color)
{
    auto const line_height = static_cast<float>(bounds.getHeight()) / interval_count;
    for (std::size_t i = 0; i < interval_count; ++i)
    {
        auto const y = bounds.getY() + static_cast<int>(i * line_height);

        // Alternate between lighter and darker colors
        auto const color = (i % 2 == 0) ? lighter_color : lighter_color.darker(0.2f);
        g.setColour(color);

        // Draw filled rectangle
        g.fillRect(bounds.getX(), y, bounds.getWidth(), line_height);

        // TODO pick a color?
        if (i != 0)
        {
            g.setColour(juce::Colours::black);
            g.drawLine(bounds.getX(), y, bounds.getX() + bounds.getWidth(), y, 0.5f);
        }
    }
}

void draw_button(juce::Graphics &g, juce::Rectangle<float> bounds,
                 juce::Colour border_color)
{
    auto const line_thickness = 2.f;

    { // Reduce Paint Region
        auto path = juce::Path{};
        path.addRoundedRectangle(bounds, corner_radius);
        g.reduceClipRegion(path);
    }

    g.setColour(border_color);
    g.drawRoundedRectangle(bounds, corner_radius, line_thickness);
}

/**
 * \p velocity must be [0, 1]
 */
[[nodiscard]] auto velocity_color(float velocity,
                                  juce::LookAndFeel const &laf) -> juce::Colour
{
    return laf.findColour((int)gui::NoteColorIDs::IntervalMid).brighter(1.f - velocity);
}

} // namespace

namespace xen::gui
{

auto Cell::paintOverChildren(juce::Graphics &g) -> void
{
    if (selected_)
    {
        auto const line_thickness = 1.f;
        auto const bounds = this->getLocalBounds().toFloat().reduced(2.f, 2.f);

        g.setColour(this->findColour((int)MeasureColorIDs::SelectionHighlight));
        g.drawRoundedRectangle(bounds, corner_radius, line_thickness);
    }
}

// -------------------------------------------------------------------------------------

auto Rest::paint(juce::Graphics &g) -> void
{
    auto const bounds = this->getLocalBounds().toFloat().reduced(2.f, 2.f);

    draw_button(g, bounds, this->findColour((int)RestColorIDs::Outline));

    // g.setColour(juce::Colours::dimgrey);
    draw_staff(g, bounds, interval_count_, juce::Colours::dimgrey.darker(0.6f));
}

// -------------------------------------------------------------------------------------

auto Note::paint(juce::Graphics &g) -> void
{
    auto const bounds = this->getLocalBounds().toFloat().reduced(2.f, 2.f);

    draw_button(g, bounds, this->findColour((int)RestColorIDs::Outline));

    // TODO use NoteColorIDs?
    // TODO Update NoteColorIDs probably not using the low mid high anymore
    draw_staff(g, bounds, tuning_length_, juce::Colours::dimgrey);

    // Paint Note Interval
    auto const interval_bounds = compute_note_bounds(bounds, note_, tuning_length_);

    g.setColour(velocity_color(note_.velocity, this->getLookAndFeel()));

    g.fillRect(interval_bounds);
    g.setColour(juce::Colours::black);
    g.drawRect(interval_bounds, 0.5f);

    // Paint Octave Text
    g.setFont({
        juce::Font::getDefaultMonospacedFontName(),
        14.f,
        juce::Font::plain,
    });
    auto const octave = get_octave(note_.interval, tuning_length_);
    auto const octave_text = (octave >= 0 ? "+" : "") + juce::String(octave) + " oct";
    g.drawText(octave_text, interval_bounds, juce::Justification::centred, true);
}

// -------------------------------------------------------------------------------------

Sequence::Sequence(sequence::Sequence const &seq, std::size_t tuning_size)
    : cells_{juce::FlexItem{}.withFlex(1.f), false}
{
    this->addAndMakeVisible(cells_);

    auto const build_and_allocate_cell = BuildAndAllocateCell{tuning_size};

    // for each sequence::Cell, construct it as a pointer and add it to cells_
    for (auto const &cell : seq.cells)
    {
        cells_.push_back(std::visit(build_and_allocate_cell, cell));
    }
}

auto Sequence::select_child(std::vector<std::size_t> const &indices) -> void
{
    if (indices.empty())
    {
        this->make_selected();
        return;
    }

    cells_.at(indices[0])
        .select_child(std::vector(std::next(indices.cbegin()), indices.cend()));
}

auto Sequence::paint(juce::Graphics &g) -> void
{
    g.setColour(this->findColour((int)MeasureColorIDs::Background));
    g.fillAll();
}

auto Sequence::resized() -> void
{
    cells_.setBounds(this->getLocalBounds());
}

} // namespace xen::gui