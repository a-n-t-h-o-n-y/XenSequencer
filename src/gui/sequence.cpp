#include <xen/gui/sequence.hpp>

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
    auto const width = static_cast<float>(getWidth());
    auto const corner_radius = juce::jlimit(
        min_radius, max_radius, juce::jmap(width, 30.f, 200.f, min_radius, max_radius));

    g.setColour(bg_color_);
    g.fillRoundedRectangle(bounds, corner_radius);

    // define text and line characteristics
    auto const font = juce::Font{16.f}.boldened();
    g.setFont(font);

    auto const text_color = juce::Colours::black;
    auto const line_thickness = 2.f;
    auto const padding = 10;

    auto const [adjusted_interval, octave] =
        NoteInterval::get_interval_and_octave(interval_, tuning_length_);

    auto const interval_text = juce::String(adjusted_interval);
    auto const octave_text = (octave >= 0 ? "+" : "") + juce::String(octave) + " oct";

    // calculate text and line positions
    auto const text_width_1 = font.getStringWidth(interval_text);
    auto const text_width_2 = font.getStringWidth(octave_text);
    auto const text_height = font.getHeight();

    // total height of drawn content
    auto const total_height = 2 * text_height + 2 * padding;

    // starting y position to center the content
    auto const start_y_position = (static_cast<float>(getHeight()) - total_height) / 2;
    auto const interval_text_y_position = start_y_position;
    auto const line_y_position = interval_text_y_position + text_height + padding;
    auto const tuning_length_text_y_position = line_y_position + padding;
    auto const line_start_x = padding;
    auto const line_end_x = getWidth() - padding;

    // draw the interval text
    g.setColour(text_color);
    g.drawText(interval_text, (getWidth() - text_width_1) / 2,
               (int)interval_text_y_position, text_width_1, (int)text_height,
               juce::Justification::centred);

    // draw the horizontal line
    g.setColour(juce::Colours::grey);
    g.drawLine(line_start_x, line_y_position, (float)line_end_x, line_y_position,
               line_thickness);

    // draw the tuning length text below the line
    g.setColour(text_color);
    g.drawText(octave_text, (getWidth() - text_width_2) / 2,
               (int)tuning_length_text_y_position, text_width_2, (int)text_height,
               juce::Justification::centred);
}

} // namespace xen::gui