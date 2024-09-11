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

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>
#include <xen/utility.hpp>

namespace
{

using namespace xen;

auto const corner_radius = 10.f;

/**
 * Computes the Rectangle bounds for a given note pitch and tuning length.
 *
 * @param bounds  The bounds of the component in which the note will be displayed.
 * @param note The note.
 * @param tuning_length The tuning length, used for scaling.
 * @return The Rectangle that represents the position and size of the note.
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

    auto const normalized = normalize_pitch(note.pitch, tuning_length);

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
                std::size_t pitch_count, juce::Colour lighter_color,
                juce::Colour line_color)
{
    auto const line_height = (float)bounds.getHeight() / (float)pitch_count;
    for (std::size_t i = 0; i < pitch_count; ++i)
    {
        auto const y = bounds.getY() + (float)i * line_height;

        // Alternate between lighter and darker colors
        auto const color = (i % 2 == 0) ? lighter_color : lighter_color.darker(0.2f);
        g.setColour(color);

        // Draw filled rectangle
        g.fillRect(bounds.getX(), y, bounds.getWidth(), line_height);

        if (i != 0)
        {
            g.setColour(line_color);
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
    return laf.findColour(gui::ColorID::ForegroundMedium).brighter(1.f - velocity);
}

} // namespace

namespace xen::gui
{

void Cell::make_selected()
{
    selected = true;
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
    if (selected)
    {
        auto const line_thickness = 2.f;
        auto const bounds = this->getLocalBounds().reduced(2, 4).toFloat();

        g.setColour(this->findColour(ColorID::ForegroundHigh));
        g.drawRoundedRectangle(bounds, corner_radius, line_thickness);
    }
}

// -------------------------------------------------------------------------------------

Rest::Rest(sequence::Rest, std::size_t pitch_count) : pitch_count_{pitch_count}
{
}

void Rest::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().reduced(2, 4).toFloat();

    draw_button(g, bounds, this->findColour(ColorID::ForegroundLow));

    draw_staff(g, bounds, pitch_count_, this->findColour(ColorID::BackgroundLow),
               this->findColour(ColorID::ForegroundInverse));
}

// -------------------------------------------------------------------------------------

Note::Note(sequence::Note note, std::size_t tuning_length)
    : note_{note}, tuning_length_{tuning_length}
{
}

void Note::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().reduced(2, 4).toFloat();

    draw_button(g, bounds, this->findColour(ColorID::ForegroundLow));

    draw_staff(g, bounds, tuning_length_, this->findColour(ColorID::ForegroundLow),
               this->findColour(ColorID::ForegroundInverse));

    // Paint Note Pitch
    auto const pitch_bounds = compute_note_bounds(bounds, note_, tuning_length_);

    g.setColour(velocity_color(note_.velocity, this->getLookAndFeel()));

    g.fillRect(pitch_bounds);
    g.setColour(this->findColour(ColorID::ForegroundInverse));
    g.drawRect(pitch_bounds, 0.5f);

    // Paint Octave Text
    auto const octave = get_octave(note_.pitch, tuning_length_);
    auto const octave_display =
        juce::String::repeatedString((octave > 0 ? "● " : "🞆 "), std::abs(octave))
            .dropLastCharacters(1);

    g.setColour(this->findColour(ColorID::BackgroundLow));
    g.setFont(
        fonts::symbols().withHeight(std::max(pitch_bounds.getHeight() - 2.f, 1.f)));
    g.drawText(octave_display,
               pitch_bounds.translated(0.f, 1.f + pitch_bounds.getHeight() / 25.f),
               juce::Justification::centred, false);
}

// -------------------------------------------------------------------------------------

Sequence::Sequence(sequence::Sequence const &seq, std::size_t tuning_size)
    : cells_{juce::FlexItem{}.withFlex(1.f)}
{
    this->addAndMakeVisible(cells_);

    auto const build_and_allocate_cell = BuildAndAllocateCell{tuning_size};

    // for each sequence::Cell, construct it as a pointer and add it to cells_
    for (auto const &cell : seq.cells)
    {
        cells_.push_back(std::visit(build_and_allocate_cell, cell));
    }
}

void Sequence::make_selected()
{
    for (auto &cell_ptr : cells_.get_children())
    {
        cell_ptr->selected = true;
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