#include "sequence.hpp"

#include <memory>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::gui
{

auto Cell::mouseDown(juce::MouseEvent const &event) -> void
{
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
            if (inc != split_preview_)
            {
                split_preview_ = inc;
                this->repaint();
            }
        }
    }
}

auto Cell::mouseUp(juce::MouseEvent const &event) -> void
{
    if (this->is_dragging() && event.mods.isRightButtonDown() && this->on_split_request)
    {
        this->on_split_request(this->get_cell_data(), split_preview_ + 1);
        return;
    }

    dragging_ = false;
}

// -----------------------------------------------------------------------------

auto Rest::mouseUp(const juce::MouseEvent &event) -> void
{
    if (event.mods.isLeftButtonDown() && !this->is_dragging() &&
        this->on_cell_swap_request)
    {
        // Warning: This call will delete *this, do not access any data, or do anything
        // after this call!
        this->on_cell_swap_request(std::make_unique<Note>(sequence::Note{}));
        return;
    }

    this->Cell::mouseUp(event);
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
    if (!this->is_dragging() && event.mods.isRightButtonDown() &&
        this->on_cell_swap_request)
    {
        // Warning: This call will delete *this, do not access any data, or do
        // anything after this call!
        this->on_cell_swap_request(std::make_unique<Rest>(sequence::Rest{}));
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