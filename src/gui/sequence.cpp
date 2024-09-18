#include <xen/gui/sequence.hpp>

#include <algorithm>
#include <cassert>
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
    std::optional<Scale> const &scale, juce::Colour light,
    std::size_t pitch_count) -> std::vector<juce::Colour>
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
                map_pitch_to_scale(i, pitches, pitch_count, TranslateDirection::Up);

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
[[nodiscard]] auto compute_note_bounds(
    juce::Rectangle<float> const &bounds, sequence::Note note,
    sequence::Tuning const &tuning) -> juce::Rectangle<float>
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
    auto const left_x = bounds.getX() + bounds.getWidth() * note.delay;
    auto const note_width =
        (bounds.getWidth() * note.gate) - (bounds.getWidth() * note.delay);

    return juce::Rectangle<float>{
        left_x,
        y,
        note_width,
        height,
    };
}

void draw_staff(juce::Graphics &g, juce::Rectangle<float> bounds,
                juce::Colour lighter_color, juce::Colour line_color,
                std::optional<Scale> const &scale, sequence::Tuning const &tuning)
{
    auto const colors =
        generate_staff_line_colors(scale, lighter_color, tuning.intervals.size());

    assert(tuning.intervals.size() == colors.size());

    auto const height = bounds.getHeight() / (float)tuning.intervals.size();
    for (auto i = std::size_t{0}; i < colors.size(); ++i)
    {
        auto const y = bounds.getHeight() + bounds.getY() - ((float)i + 1.f) * height;

        g.setColour(colors[i]);
        g.fillRect(bounds.getX(), y, bounds.getWidth(), height);

        if (i + 1 != colors.size())
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

// void draw_pitch_line(juce::Graphics &g, juce::Rectangle<float> bounds,
//                      sequence::Tuning const &tuning, int pitch, juce::Colour fg)
// {
//     // auto const normalized = normalize_pitch(pitch, tuning.intervals.size());
//     //    (tuning.intervals[normalized] / tuning.octave) * bounds.getHeight();
//     g.setColour(fg);
//     for (auto i = std::size_t{0}; i < tuning.intervals.size(); ++i)
//     {
//         auto const y = bounds.getHeight() + bounds.getY() -
//                        (tuning.intervals[i] / tuning.octave) * bounds.getHeight();
//         g.drawLine(bounds.getX(), y, bounds.getX() + bounds.getWidth(), y, 1.f);
//     }
// }

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

Rest::Rest(sequence::Rest, std::optional<Scale> const &scale,
           sequence::Tuning const &tuning)
    : scale_{scale}, tuning_{tuning}
{
}

void Rest::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().reduced(2, 4).toFloat();

    draw_button(g, bounds, this->findColour(ColorID::ForegroundLow));

    draw_staff(g, bounds, this->findColour(ColorID::BackgroundLow),
               this->findColour(ColorID::ForegroundInverse), scale_, tuning_);
}

// -------------------------------------------------------------------------------------

Note::Note(sequence::Note note, std::optional<Scale> const &scale,
           sequence::Tuning const &tuning)
    : note_{note}, scale_{scale}, tuning_{tuning}
{
}

void Note::paint(juce::Graphics &g)
{
    auto const bounds = this->getLocalBounds().reduced(2, 4).toFloat();

    draw_button(g, bounds, this->findColour(ColorID::ForegroundLow));

    draw_staff(g, bounds, this->findColour(ColorID::ForegroundLow),
               this->findColour(ColorID::ForegroundInverse), scale_, tuning_);

    // Draw Note Box
    auto const pitch_bounds = compute_note_bounds(bounds, note_, tuning_);
    g.setColour(velocity_color(note_.velocity, this->getLookAndFeel()));
    g.fillRect(pitch_bounds);
    g.setColour(this->findColour(ColorID::ForegroundInverse));
    g.drawRect(pitch_bounds, 0.5f);

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
                   sequence::Tuning const &tuning)
    : cells_{juce::FlexItem{}.withFlex(1.f)}
{
    this->addAndMakeVisible(cells_);

    auto const build_and_allocate_cell = BuildAndAllocateCell{
        scale,
        tuning,
    };

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

BuildAndAllocateCell::BuildAndAllocateCell(std::optional<Scale> const &scale,
                                           sequence::Tuning const &tuning)
    : scale_{scale}, tuning_{tuning}
{
}

auto BuildAndAllocateCell::operator()(sequence::Rest r) const -> std::unique_ptr<Cell>
{
    return std::make_unique<Rest>(r, scale_, tuning_);
}

auto BuildAndAllocateCell::operator()(sequence::Note n) const -> std::unique_ptr<Cell>
{
    return std::make_unique<Note>(n, scale_, tuning_);
}

auto BuildAndAllocateCell::operator()(sequence::Sequence s) const
    -> std::unique_ptr<Cell>
{
    return std::make_unique<Sequence>(s, scale_, tuning_);
}

} // namespace xen::gui