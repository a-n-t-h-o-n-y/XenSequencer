#include <xen/gui/cell.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
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

auto const CORNER_RADIUS = 10.f;

void draw_note_border(juce::Graphics &g, juce::Rectangle<int> bounds,
                      sequence::Note const &note)
{
    auto const thickness = 1;

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

[[nodiscard]]
auto generate_octave_display(int octave) -> juce::String
{
    auto const sign = octave > 0 ? "+" : (octave < 0 ? "-" : "");
    auto const digits = std::abs(octave) < 1 ? "" : std::to_string(std::abs(octave));
    return sign + digits;
}

/**
 * Paints a rounded rectangle around the cell, mostly just here for the rounded corners.
 */
void paint_cell_border(juce::Graphics &g, juce::Rectangle<int> bounds,
                       juce::Colour color)
{
    // inverted rounded rectangle
    auto const hole_bounds = bounds.reduced(2, 7);
    auto clip_path = juce::Path{};
    clip_path.addRectangle(bounds);
    clip_path.addRoundedRectangle(hole_bounds, CORNER_RADIUS);
    clip_path.setUsingNonZeroWinding(false); // use even-odd fill rule for inversion

    g.saveState();
    g.reduceClipRegion(clip_path);
    g.fillAll(color);
    g.restoreState();
}

/**
 * Generates background color for Note display.
 */
[[nodiscard]]
auto generate_note_color(juce::Colour const &base_color, sequence::Note const &note)
    -> juce::Colour
{
    if (utility::compare_within_tolerance(note.velocity, 0.f, 0.0001f))
    {
        return base_color.withAlpha(note.velocity);
    }

    auto const default_velocity = 100.f / 127.f;
    if (auto const ratio = note.velocity / default_velocity; ratio <= 1.f)
    {
        return base_color.withAlpha(std::lerp(0.2f, 1.f, ratio));
    }

    auto const ratio = (note.velocity - default_velocity) / (1.f - default_velocity);
    return base_color.withMultipliedSaturation(std::lerp(1.f, 0.9f, ratio))
        .withMultipliedBrightness(std::lerp(1.f, 1.1f, ratio));
}

/**
 * Creates a list of gui::Cell components from a sequence::Sequence.
 * @param seq The sequence to create cells from.
 * @param build_and_allocate_cell The function to create a Cell from a sequence::Cell.
 */
[[nodiscard]]
auto create_cells_components(
    sequence::Sequence const &seq,
    xen::gui::BuildAndAllocateCell const &build_and_allocate_cell)
    -> std::vector<std::unique_ptr<xen::gui::Cell>>
{
    auto cells = std::vector<std::unique_ptr<xen::gui::Cell>>{};
    cells.reserve(seq.cells.size());

    std::ranges::transform(seq.cells, std::back_inserter(cells), [&](auto const &cell) {
        auto ui = std::visit(build_and_allocate_cell, cell.element);
        ui->weight = cell.weight;
        return ui;
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
        g.setColour(color);

        auto const bounds = this->getLocalBounds().reduced(2, 7);
        g.fillRect(bounds.withHeight(1).withY(bounds.getY() - 4));
        g.fillRect(bounds.withHeight(1).withY(bounds.getBottom() + 3));
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
    paint_cell_border(g, this->getLocalBounds(),
                      this->findColour(ColorID::BackgroundHigh));
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
    auto const bounds = this->getLocalBounds().reduced(2, 7);

    // Draw Note
    auto const note_color =
        generate_note_color(this->findColour(ColorID::ForegroundMedium), note_);
    g.setColour(note_color);

    auto pitch_bounds = compute_note_bounds(bounds, note_, tuning_.intervals.size());

    g.fillRect(pitch_bounds);

    // Note Border
    g.setColour(this->findColour(ColorID::BackgroundHigh));
    draw_note_border(g, pitch_bounds, note_);

    // Octave Text
    g.setColour(this->findColour(ColorID::BackgroundLow));
    g.setFont(
        fonts::monospaced().bold.withHeight(std::max(pitch_bounds.getHeight() - 2, 1)));
    auto const octave =
        ::xen::utility::get_octave(note_.pitch, tuning_.intervals.size());
    g.drawText(
        generate_octave_display(octave),
        pitch_bounds.translated(0.f, 1.f + (float)pitch_bounds.getHeight() / 25.f),
        juce::Justification::centred, false);

    // Cell Border
    paint_cell_border(g, this->getLocalBounds(),
                      this->findColour(ColorID::BackgroundHigh));
}

// -------------------------------------------------------------------------------------

Sequence::Sequence(sequence::Sequence const &seq, std::optional<Scale> const &scale,
                   sequence::Tuning const &tuning,
                   TranslateDirection scale_translate_direction)
    : cells_{create_cells_components(seq, BuildAndAllocateCell{
                                              scale,
                                              tuning,
                                              scale_translate_direction,
                                          })}
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

auto compute_note_bounds(juce::Rectangle<int> const &bounds, sequence::Note note,
                         std::size_t pitch_count) -> juce::Rectangle<int>
{
    if (pitch_count == 0)
    {
        throw std::invalid_argument("Tuning length must not be zero.");
    }

    auto const normalized = xen::utility::normalize_pitch(note.pitch, pitch_count);
    assert(normalized < pitch_count);

    auto const total_height = bounds.getHeight();
    auto const int_height = total_height / static_cast<int>(pitch_count);
    auto const remainder = total_height % static_cast<int>(pitch_count);

    auto const row = static_cast<int>(pitch_count - 1 - normalized);
    auto const y = bounds.getY() + row * int_height + std::min(row, remainder);
    auto const h = int_height + (row < remainder ? 1 : 0);

    auto const x =
        bounds.getX() + static_cast<int>((bounds.getWidth() - 1) * note.delay);
    auto const remaining = bounds.getWidth() - (x - bounds.getX());
    auto const w = std::max(static_cast<int>(remaining * note.gate), 4);

    // Leave room for staff lines (1 pixel at top)
    return juce::Rectangle<int>{x, y + 1, w, h - 1};
}

} // namespace xen::gui