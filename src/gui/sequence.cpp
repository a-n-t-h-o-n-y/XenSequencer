#include <xen/gui/sequence.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <variant>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>
#include <sequence/tuning.hpp>

#include <xen/gui/fonts.hpp>
#include <xen/gui/themes.hpp>
#include <xen/scale.hpp>
#include <xen/utility.hpp>

namespace
{

using namespace xen;

auto const corner_radius = 10.f;

/**
 * Returns list of background colors for each pitch in tuning, starting with pitch 0.
 */
[[nodiscard]] auto generate_staff_line_colors(
    std::optional<Scale> const &scale, juce::Colour light, std::size_t pitch_count,
    TranslateDirection scale_translate_direction) -> std::vector<juce::Colour>
{
    auto colors = std::vector<juce::Colour>{};
    if (scale.has_value())
    {
        auto const pitches = generate_valid_pitches(*scale);
        juce::Colour current_color = light;
        int previous_pitch = 0;

        for (auto i = 0; i < (int)pitch_count; ++i)
        {
            auto const mapped_pitch =
                map_pitch_to_scale(i, pitches, pitch_count, scale_translate_direction);

            if (mapped_pitch != previous_pitch)
            {
                current_color = current_color == light ? light.darker(0.2f) : light;
            }
            colors.push_back(current_color);
            previous_pitch = mapped_pitch;
        }
    }
    else
    {
        for (std::size_t i = 0; i < pitch_count; ++i)
        {
            colors.push_back((i % 2 == 0) ? light : light.darker(0.2f));
        }
    }
    return colors;
}

/**
 * Computes the Rectangle bounds for a given note pitch and tuning length.
 *
 * @param bounds  The bounds of the component in which the note will be displayed.
 * @param note The note.
 * @return The Rectangle that represents the position and size of the note.
 * @exception std::invalid_argument If tuning_length is zero, to prevent
 * division by zero.
 */
[[nodiscard]] auto compute_note_bounds(juce::Rectangle<float> const &bounds,
                                       sequence::Note note,
                                       sequence::Tuning const &tuning)
    -> juce::Rectangle<float>
{
    auto const pitch_count = tuning.intervals.size();
    if (pitch_count == 0)
    {
        throw std::invalid_argument("Tuning length must not be zero.");
    }

    auto const normalized = normalize_pitch(note.pitch, pitch_count);

    assert(normalized < tuning.intervals.size());

    auto const height = bounds.getHeight() / (float)tuning.intervals.size();
    auto const y =
        bounds.getHeight() + bounds.getY() - ((float)normalized + 1.f) * height;

    // Calculate the note x and width
    auto const left_x = bounds.getX() + (bounds.getWidth() - 1.f) * note.delay;
    auto note_width = (bounds.getWidth() - (left_x - bounds.getX())) * note.gate;
    note_width = std::max(note_width, 4.f);

    return juce::Rectangle<float>{
        left_x,
        y,
        note_width,
        height,
    };
}

void draw_staff(juce::Graphics &g, juce::Rectangle<float> bounds,
                juce::Colour lighter_color, juce::Colour line_color,
                std::optional<Scale> const &scale, sequence::Tuning const &tuning,
                TranslateDirection scale_translate_direction)
{
    auto const colors = generate_staff_line_colors(
        scale, lighter_color, tuning.intervals.size(), scale_translate_direction);

    assert(tuning.intervals.size() == colors.size());

    auto const height = bounds.getHeight() / (float)tuning.intervals.size();

    // Rectangles - Drawn bottom to top - starting with pitch zero.
    for (auto i = std::size_t{0}; i < colors.size(); ++i)
    {
        auto const y = bounds.getY() + (float)(colors.size() - i - 1) * height;
        g.setColour(colors[i]);
        g.fillRect(bounds.getX(), y, bounds.getWidth(), height);
    }
    // Lines - Drawn on top
    for (auto i = std::size_t{0}; i + 1 < colors.size(); ++i)
    {
        auto const color = (colors[i] == colors[i + 1]) ? colors[i] : line_color;
        auto const y = bounds.getY() + (float)(colors.size() - i - 1) * height;
        g.setColour(color);
        g.fillRect(bounds.getX(), y - 0.4f, bounds.getWidth(), 0.8f);
    }
}

void draw_note_border(juce::Graphics &g, juce::Rectangle<float> bounds,
                      sequence::Note const &note, juce::Colour border_color)
{
    auto const thickness = 0.8f;
    g.setColour(border_color);
    // Top
    g.fillRect(bounds.withHeight(thickness));

    // Bottom
    g.fillRect(bounds.withHeight(thickness).withY(bounds.getY() + bounds.getHeight() -
                                                  thickness));

    // Left
    if (std::not_equal_to{}(note.delay, 0.f))
    {
        g.fillRect(bounds.withWidth(thickness));
    }

    // Right
    if (std::not_equal_to{}(note.gate, 1.f))
    {
        g.fillRect(bounds.withWidth(thickness).withX(bounds.getX() + bounds.getWidth() -
                                                     thickness));
    }
}

void reduce_region(juce::Graphics &g, juce::Rectangle<float> bounds)
{
    auto path = juce::Path{};
    path.addRoundedRectangle(bounds, corner_radius);
    g.reduceClipRegion(path);
}

/**
 * \p velocity must be [0, 1]
 */
[[nodiscard]] auto velocity_color(float velocity, juce::LookAndFeel const &laf)
    -> juce::Colour
{
    return laf.findColour(gui::ColorID::ForegroundMedium).brighter(1.f - velocity);
}

/**
 * Creates a list of gui::Cell components from a sequence::Sequence.
 * @param seq The sequence to create cells from.
 * @param build_and_allocate_cell The function to create a Cell from a sequence::Cell.
 */
[[nodiscard]] auto create_cells_components(
    sequence::Sequence const &seq,
    xen::gui::BuildAndAllocateCell const &build_and_allocate_cell)
    -> std::vector<std::unique_ptr<xen::gui::Cell>>
{
    auto cells = std::vector<std::unique_ptr<xen::gui::Cell>>{};
    cells.reserve(seq.cells.size());

    std::ranges::transform(seq.cells, std::back_inserter(cells), [&](auto const &cell) {
        return std::visit(build_and_allocate_cell, cell);
    });

    return cells;
}

} // namespace

// -------------------------------------------------------------------------------------

namespace xen::gui
{

void Cell::make_selected()
{
    selected_ = true;
}

void Cell::emphasize_selection(bool emphasized)
{
    emphasized_ = emphasized;
}

void Cell::update_pattern(sequence::Pattern const &)
{
}

auto Cell::find_child(std::vector<std::size_t> const &indices) -> Cell *
{
    return indices.empty() ? this : nullptr;
}

void Cell::paintOverChildren(juce::Graphics &g)
{
    if (selected_)
    {
        auto const color = this->findColour(emphasized_ ? ColorID::ForegroundHigh
                                                        : ColorID::ForegroundLow);
        auto const line_thickness = 1.6f;
        auto const bounds = this->getLocalBounds().reduced(2, 4).toFloat();

        g.setColour(color);
        g.drawRoundedRectangle(bounds, corner_radius, line_thickness);
    }
}

// -------------------------------------------------------------------------------------

Rest::Rest(sequence::Rest, std::optional<Scale> const &scale,
           sequence::Tuning const &tuning, TranslateDirection scale_translate_direction)
    : scale_{scale}, tuning_{tuning},
      scale_translate_direction_{scale_translate_direction}
{
}

void Rest::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().reduced(2, 4).toFloat();

    reduce_region(g, bounds);
    draw_staff(g, bounds, this->findColour(ColorID::BackgroundLow),
               this->findColour(ColorID::ForegroundInverse), scale_, tuning_,
               scale_translate_direction_);
}

// -------------------------------------------------------------------------------------

Note::Note(sequence::Note note, std::optional<Scale> const &scale,
           sequence::Tuning const &tuning, TranslateDirection scale_translate_direction)
    : note_{note}, scale_{scale}, tuning_{tuning},
      scale_translate_direction_{scale_translate_direction}
{
}

void Note::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().reduced(2, 4).toFloat();

    reduce_region(g, bounds);
    draw_staff(g, bounds, this->findColour(ColorID::ForegroundLow),
               this->findColour(ColorID::ForegroundInverse), scale_, tuning_,
               scale_translate_direction_);

    // Draw Note Box
    auto const pitch_bounds = compute_note_bounds(bounds, note_, tuning_);
    g.setColour(velocity_color(note_.velocity, this->getLookAndFeel()));
    g.fillRect(pitch_bounds);
    draw_note_border(g, pitch_bounds, note_,
                     this->findColour(ColorID::ForegroundInverse));

    // Paint Octave Text
    auto const octave = get_octave(note_.pitch, tuning_.intervals.size());
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

Sequence::Sequence(sequence::Sequence const &seq, std::optional<Scale> const &scale,
                   sequence::Tuning const &tuning,
                   TranslateDirection scale_translate_direction)
    : cells_{create_cells_components(seq,
                                     BuildAndAllocateCell{
                                         scale,
                                         tuning,
                                         scale_translate_direction,
                                     }),
             juce::FlexItem{}.withFlex(1.f)}
{
    this->addAndMakeVisible(cells_);
}

void Sequence::make_selected()
{
    for (auto &cell_ptr : cells_.get_children())
    {
        cell_ptr->Cell::make_selected();
    }
}

void Sequence::update_pattern(sequence::Pattern const &pattern)
{
    for (auto &cell : cells_.get_children())
    {
        cell->emphasize_selection(false);
    }

    auto pattern_view = sequence::PatternView{cells_.get_children(), pattern};
    for (auto &cell : pattern_view)
    {
        cell->emphasize_selection();
    }
    this->repaint();
}

auto Sequence::find_child(std::vector<std::size_t> const &indices) -> Cell *
{
    if (indices.empty())
    {
        return this;
    }

    if (indices.front() >= cells_.get_children().size())
    {
        return nullptr;
    }

    return cells_.get_children()[indices.front()]->find_child(
        std::vector(std::next(indices.cbegin()), indices.cend()));
}

void Sequence::resized()
{
    cells_.setBounds(this->getLocalBounds());
}

// -------------------------------------------------------------------------------------

BuildAndAllocateCell::BuildAndAllocateCell(std::optional<Scale> const &scale,
                                           sequence::Tuning const &tuning,
                                           TranslateDirection scale_translate_direction)
    : scale_{scale}, tuning_{tuning},
      scale_translate_direction_{scale_translate_direction}
{
}

auto BuildAndAllocateCell::operator()(sequence::Rest r) const -> std::unique_ptr<Cell>
{
    return std::make_unique<Rest>(r, scale_, tuning_, scale_translate_direction_);
}

auto BuildAndAllocateCell::operator()(sequence::Note n) const -> std::unique_ptr<Cell>
{
    return std::make_unique<Note>(n, scale_, tuning_, scale_translate_direction_);
}

auto BuildAndAllocateCell::operator()(sequence::Sequence s) const
    -> std::unique_ptr<Cell>
{
    return std::make_unique<Sequence>(s, scale_, tuning_, scale_translate_direction_);
}

} // namespace xen::gui