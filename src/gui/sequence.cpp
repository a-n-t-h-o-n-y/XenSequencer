#include <xen/gui/sequence.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>

#include <xen/gui/color_ids.hpp>
#include <xen/gui/fonts.hpp>
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
    auto const note_width =
        (bounds.getWidth() * note.gate) - (bounds.getWidth() * note.delay);

    return juce::Rectangle<float>{
        left_x,
        y_position - note_height,
        note_width,
        note_height,
    };
}

void draw_staff(juce::Graphics &g, juce::Rectangle<float> bounds,
                std::size_t interval_count, juce::Colour lighter_color)
{
    auto const line_height = (float)bounds.getHeight() / (float)interval_count;
    for (std::size_t i = 0; i < interval_count; ++i)
    {
        auto const y = bounds.getY() + (float)i * line_height;

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

void Cell::make_selected()
{
    selected_ = true;
}

void Cell::select_child(std::vector<std::size_t> const &indices)
{
    if (indices.empty())
    {
        this->make_selected();
    }
    else
    {
        throw std::runtime_error(
            "Invalid index or unexpected type encountered in traversal.");
    }
}

void Cell::paintOverChildren(juce::Graphics &g)
{
    if (selected_)
    {
        auto const line_thickness = 1.f;
        auto const bounds = this->getLocalBounds().toFloat().reduced(2.f, 4.f);

        g.setColour(this->findColour((int)MeasureColorIDs::SelectionHighlight));
        g.drawRoundedRectangle(bounds, corner_radius, line_thickness);
    }
}

// -------------------------------------------------------------------------------------

Rest::Rest(sequence::Rest, std::size_t interval_count) : interval_count_{interval_count}
{
}

void Rest::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().toFloat().reduced(2.f, 4.f);

    draw_button(g, bounds, this->findColour((int)RestColorIDs::Outline));

    // g.setColour(juce::Colours::dimgrey);
    draw_staff(g, bounds, interval_count_, juce::Colours::dimgrey.darker(1.f));
}

// -------------------------------------------------------------------------------------

Note::Note(sequence::Note note, std::size_t tuning_length)
    : note_{note}, tuning_length_{tuning_length}
{
}

void Note::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().toFloat().reduced(2.f, 4.f);

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
    auto const octave = get_octave(note_.interval, tuning_length_);
    auto const octave_display =
        juce::String::repeatedString((octave > 0 ? "● " : "🞆 "), std::abs(octave))
            .dropLastCharacters(1);

    // TODO color ids
    g.setColour(this->findColour((int)NoteColorIDs::Foreground));
    g.setFont(
        fonts::symbols().withHeight(std::max(interval_bounds.getHeight() - 2.f, 1.f)));
    g.drawText(
        octave_display,
        interval_bounds.translated(0.f, 1.f + interval_bounds.getHeight() / 25.f),
        juce::Justification::centred, false);
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

void Sequence::select_child(std::vector<std::size_t> const &indices)
{
    if (indices.empty())
    {
        this->make_selected();
        return;
    }

    cells_.at(indices[0])
        .select_child(std::vector(std::next(indices.cbegin()), indices.cend()));
}

void Sequence::paint(juce::Graphics &g)
{
    g.setColour(this->findColour((int)MeasureColorIDs::Background));
    g.fillAll();
}

void Sequence::resized()
{
    cells_.setBounds(this->getLocalBounds());
}

// -------------------------------------------------------------------------------------

BuildAndAllocateCell::BuildAndAllocateCell(std::size_t tuning_octave_size)
    : tos_{tuning_octave_size}
{
}

auto BuildAndAllocateCell::operator()(sequence::Rest r) const -> std::unique_ptr<Cell>
{
    return std::make_unique<Rest>(r, tos_);
}

auto BuildAndAllocateCell::operator()(sequence::Note n) const -> std::unique_ptr<Cell>
{
    return std::make_unique<Note>(n, tos_);
}

auto BuildAndAllocateCell::operator()(sequence::Sequence s) const
    -> std::unique_ptr<Cell>
{
    return std::make_unique<Sequence>(s, tos_);
}

} // namespace xen::gui