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
                                       int note_interval, std::size_t tuning_length)
    -> juce::Rectangle<float>
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
            g.drawHorizontalLine(y, bounds.getX(), bounds.getX() + bounds.getWidth());
        }
    }
}

// TODO remove background_color param?
void draw_button(juce::Graphics &g, juce::Rectangle<float> bounds,
                 juce::Colour background_color, juce::Colour border_color)
{
    auto const min_radius = 10.f;
    auto const max_radius = 25.f;
    auto const line_thickness = 2.f;
    auto const corner_radius = compute_corner_radius(bounds, min_radius, max_radius);

    { // Reduce Paint Region
        auto path = juce::Path{};
        path.addRoundedRectangle(bounds, corner_radius);
        g.reduceClipRegion(path);
    }

    // g.setColour(background_color);
    // g.fillRoundedRectangle(bounds, corner_radius);

    g.setColour(border_color);
    g.drawRoundedRectangle(bounds, corner_radius, line_thickness);
}

} // namespace

namespace xen::gui
{

auto Rest::paint(juce::Graphics &g) -> void
{
    auto const bounds = this->getLocalBounds().toFloat().reduced(2.f, 4.f);

    draw_button(g, bounds, this->findColour((int)RestColorIDs::Background),
                this->findColour((int)RestColorIDs::Outline));

    // g.setColour(juce::Colours::dimgrey);
    draw_staff(g, bounds, interval_count_, juce::Colours::dimgrey.darker(0.6f));

    // auto const font = juce::Font{"Arial", "Normal", 16.f}.boldened();
    // g.setFont(font);
    // g.setColour(this->findColour((int)RestColorIDs::Text));
    // g.drawText("R", bounds, juce::Justification::centred);
}

// -------------------------------------------------------------------------------------

auto NoteInterval::paint(juce::Graphics &g) -> void
{
    auto const bounds = this->getLocalBounds().toFloat().reduced(2.f, 4.f);

    draw_button(g, bounds, this->findColour((int)NoteColorIDs::Foreground),
                this->findColour((int)RestColorIDs::Outline));

    // g.setColour(juce::Colours::dimgrey);
    // TODO use NoteColorIDs?
    // TODO Update NoteColorIDs probably not using the low mid high anymore
    draw_staff(g, bounds, tuning_length_, juce::Colours::dimgrey);

    // Paint Note Interval ------------------------------------------------------
    // TODO account for y offset for note placement
    auto const interval_bounds = compute_note_bounds(bounds, interval_, tuning_length_);
    auto const note_color = from_gradient((float)get_octave(interval_, tuning_length_),
                                          -4.f, 4.f, this->getLookAndFeel());

    g.setColour(note_color);
    g.fillRect(interval_bounds);
    // Paint Interval Text ------------------------------------------------------
    // {
    // auto const interval_text =
    //     juce::String(normalize_interval(interval_, tuning_length_));

    // auto font_size = std::min(16.f, interval_bounds.getHeight());
    // auto font = juce::Font{"Arial", "Normal", font_size}.boldened();
    // g.setFont(font);
    // g.setColour(this->findColour((int)NoteColorIDs::IntervalText));

    // // auto const margin = std::max(
    // //     0.f, corner_radius - ((float)font.getStringWidth(interval_text) / 2.f));
    // auto const margin = 4.f;

    // // Draw the interval text aligned to the far left and vertically centered.
    // g.drawText(interval_text, (int)(interval_bounds.getX() + margin),
    //            (int)interval_bounds.getY(),
    //            (int)(interval_bounds.getWidth() - margin),
    //            (int)interval_bounds.getHeight(), juce::Justification::centredLeft);
    // }
    // Paint Octave Text --------------------------------------------------------
    // {
    //     g.setFont(juce::Font{"Arial", "Normal", 16.f}.boldened());

    //     auto const octave = get_octave(interval_, tuning_length_);

    //     auto octave_text = (octave >= 0 ? "+" : "") + juce::String(octave) + " oct";
    //     if ((float)g.getCurrentFont().getStringWidth(octave_text) >
    //     bounds.getWidth())
    //     {
    //         octave_text = (octave >= 0 ? "+" : "") + juce::String(octave);
    //     }

    //     g.setColour(this->findColour((int)NoteColorIDs::OctaveText));
    //     g.drawText(octave_text, this->getLocalBounds(),
    //     juce::Justification::centred);
    // }
}

// -------------------------------------------------------------------------------------

auto IntervalColumn::paint(juce::Graphics &g) -> void
{
    // TODO color ID
    g.fillAll(this->findColour((int)MeasureColorIDs::Background));

    auto const bounds = this->getLocalBounds().toFloat().reduced(0.f, vertical_offset_);

    // TODO add color ID
    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font{
        juce::Font::getDefaultMonospacedFontName(),
        14.f,
        juce::Font::plain,
    });

    auto const item_height = bounds.getHeight() / static_cast<float>(size_);

    for (std::size_t i = 0; i < size_; ++i)
    {
        float y = bounds.getBottom() - (static_cast<float>(i) + 1.f) * item_height;
        auto const text = juce::String(i).paddedLeft('0', 2);

        g.drawText(text, bounds.withY(y).withHeight(item_height),
                   juce::Justification::centred, true);
    }
}

// -------------------------------------------------------------------------------------

auto SequenceIndicator::paint(juce::Graphics &g) -> void
{
    constexpr auto margin = 4;
    constexpr auto thickness = 1;

    float const y_offset = static_cast<float>(this->getHeight() - thickness) / 2.f;
    float const x_start = margin;
    float const x_end = static_cast<float>(this->getWidth() - margin);

    g.setColour(this->findColour((int)MeasureColorIDs::Outline));
    g.drawLine(x_start, y_offset, x_end, y_offset, thickness);
}

// -------------------------------------------------------------------------------------

Sequence::Sequence(sequence::Sequence const &seq, std::size_t tuning_size)
    : interval_column_{tuning_size, 4.f}, cells_{juce::FlexItem{}.withFlex(1.f), false}
{
    this->addAndMakeVisible(top_indicator_);
    this->addAndMakeVisible(interval_column_);
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
    auto outer_flexbox = juce::FlexBox{};
    outer_flexbox.flexDirection = juce::FlexBox::Direction::column;

    auto inner_flexbox = juce::FlexBox{};
    inner_flexbox.flexDirection = juce::FlexBox::Direction::row;

    inner_flexbox.items.add(juce::FlexItem(interval_column_).withWidth(23.f));
    inner_flexbox.items.add(juce::FlexItem(cells_).withFlex(1.f));

    outer_flexbox.items.add(juce::FlexItem(top_indicator_).withHeight(8.f));
    outer_flexbox.items.add(juce::FlexItem(inner_flexbox).withFlex(1.f));

    outer_flexbox.performLayout(this->getLocalBounds());
}

} // namespace xen::gui