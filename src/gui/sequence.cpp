#include "sequence.hpp"

#include <memory>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "focusable_component.hpp"

namespace xen::gui
{

auto Cell::mouseDown(juce::MouseEvent const &event) -> void
{
    this->FocusableComponent::mouseDown(event);

    drag_start_position_ = event.position;

    if (event.mods.isRightButtonDown())
    {
        split_preview_ = 0;
    }
}

auto Cell::mouseDrag(juce::MouseEvent const &event) -> void
{
    dragging_ = true;

    if (event.mods.isRightButtonDown())
    {
        { // Split Preview
            auto const mod = event.mods.isShiftDown()  ? 2.f
                             : event.mods.isCtrlDown() ? 0.5f
                                                       : 1.f;
            auto const inc = Cell::get_increment(
                50, (int)(std::abs(drag_start_position_.y - event.position.y)), mod,
                25);
            this->set_split_preview(inc);
        }
    }
}

auto Cell::mouseUp(juce::MouseEvent const &event) -> void
{
    if (this->is_dragging() && event.mods.isRightButtonDown() && this->on_split_request)
    {
        this->on_split_request(this->get_cell_data(),
                               (std::size_t)(split_preview_ + 1));
        return;
    }

    dragging_ = false;
}

// -----------------------------------------------------------------------------

auto Rest::mouseUp(const juce::MouseEvent &event) -> void
{
    if (event.mods.isLeftButtonDown() && !this->is_dragging())
    {
        // Warning: This call will delete *this, do not access any data, or do anything
        // after this call!
        this->flip_cell();
        return;
    }

    this->Cell::mouseUp(event);
}

auto Rest::flip_cell() -> void
{
    if (this->on_cell_swap_request)
    {
        // Warning: This call will delete *this, do not access any data, or do
        // anything after this call!
        // TODO you need the tuning length here... you have it in subsequence..
        this->on_cell_swap_request(std::make_unique<Note>(sequence::Note{}, 12));
        return;
    }
}

// -----------------------------------------------------------------------------

auto NoteInterval::paint(juce::Graphics &g) -> void
{
    g.fillAll(bg_color_);

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
    auto const start_y_position = (getHeight() - total_height) / 2;
    auto const interval_text_y_position = start_y_position;
    auto const line_y_position = interval_text_y_position + text_height + padding;
    auto const tuning_length_text_y_position = line_y_position + padding;
    auto const line_start_x = padding;
    auto const line_end_x = getWidth() - padding;

    // draw the interval text
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

// -----------------------------------------------------------------------------

auto Note::mouseDown(juce::MouseEvent const &event) -> void
{
    this->Cell::mouseDown(event);

    if (event.mods.isLeftButtonDown())
    {
        initial_interval_ = note_.interval;
        initial_delay_ = note_.delay;
    }
    else if (event.mods.isRightButtonDown())
    {
        initial_gate_ = note_.gate;
    }
}

auto Note::mouseDrag(juce::MouseEvent const &event) -> void
{
    this->Cell::mouseDrag(event);

    if (event.mods.isLeftButtonDown())
    {
        { // Interval
            auto const mod = event.mods.isShiftDown()  ? 2.f
                             : event.mods.isCtrlDown() ? 0.5f
                                                       : 1.f;
            auto const inc = Cell::get_increment(
                18, (int)(this->drag_start_position().y - event.position.y), mod, 25);
            this->set_interval(initial_interval_ + inc);
        }
        { // Delay
            auto const mod = event.mods.isShiftDown()  ? 2.f
                             : event.mods.isCtrlDown() ? 0.5f
                                                       : 1.f;
            auto const inc = Cell::get_increment(
                18, (int)(event.position.x - this->drag_start_position().x), mod, 25);
            this->set_delay(initial_delay_ + (inc * 0.03f));
        }
    }
    else if (event.mods.isRightButtonDown())
    {
        { // Gate
            auto const mod = event.mods.isShiftDown()  ? 2.f
                             : event.mods.isCtrlDown() ? 0.5f
                                                       : 1.f;
            auto const inc = Cell::get_increment(
                18, (int)(event.position.x - this->drag_start_position().x), mod, 25);
            this->set_gate(initial_gate_ + (inc * 0.03f));
        }
    }
}

auto Note::mouseUp(juce::MouseEvent const &event) -> void
{
    if (!this->is_dragging() && event.mods.isRightButtonDown())
    {
        // Warning: This call will delete *this, do not access any data, or do
        // anything after this call!
        this->flip_cell();
        return;
    }

    this->Cell::mouseUp(event);
}

auto Note::mouseWheelMove(juce::MouseEvent const &event,
                          juce::MouseWheelDetails const &wheel) -> void
{
    auto const mod = event.mods.isShiftDown()  ? 5.f
                     : event.mods.isCtrlDown() ? 0.2f
                                               : 1.f;
    this->increment_velocity(wheel.deltaY * mod * 0.08f);
}

} // namespace xen::gui