#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/measure.hpp>
#include <sequence/sequence.hpp>

#include <xen/clock.hpp>
#include <xen/state.hpp>

namespace xen::gui
{

/**
 * Intermediate Representation for Notes before they are drawn.
 * @details Calculating the integer pixel position for y coordinate and height is
 * very specific in its implementation, and I am drawing notes in two different places
 * onto the same staff, so everything needs to be calculated uniformly. This helps get
 * the data needed to the correct place to do those calculations. Basically, I am
 * deferring veritical dimension calculations until later.
 */
struct NoteIR
{
    sequence::Note note;
    float x;
    float width;
};

using IR = std::vector<NoteIR>;

struct IRWindow
{
    float offset; // How far into bg to begin [0..1]
    float length; // How many lengths of bg to display [0..inf]

    auto operator==(IRWindow const &other) const -> bool = default;
    auto operator!=(IRWindow const &other) const -> bool = default;
};

[[nodiscard]]
auto generate_ir(sequence::Cell const &cell, std::size_t tuning_length) -> IR;

/**
 * Create a IRWindow over a background active sequence to determine where to start in the
 * background sequence and how many times to repeat.
 */
[[nodiscard]]
auto generate_window(Clock::duration fg_duration, Clock::time_point bg_start,
                     Clock::duration bg_duration, Clock::time_point now) -> IRWindow;

/**
 * @brief Applies a repeating window to an IR sequence.
 * @detail
 *   - The IR’s rectangles are defined over a unit-length background [0..1] that repeats
 * infinitely.
 *   - The window “cuts out” the portion of length `window.length` starting at relative
 * * offset `window.offset` (wrapping/repeating as needed), then normalizes that slice
 * back to [0..1].
 * @param ir
 *   Source IR containing rectangles with X ∈ [0..1].
 *   Invariant: ir.rectangles[*].getX() and getWidth() ∈ [0..1], and rectangles do not
 * overlap boundaries.
 * @param window
 *   .offset ∈ [0..1) — where to begin within the repeating background
 *   .length >= 0   — how many background-lengths to include (can exceed 1 for repeats)
 * @param trigger_offset - how much to rotate the sequence to line up with trigger[0, 1]
 * @return
 *   New IR whose rectangles lie within [0..1], representing the windowed slice.
 */
[[nodiscard]]
auto apply_window(IR const &ir, IRWindow const &window, float trigger_offset) -> IR;

[[nodiscard]]
auto get_bg_trigger_offset(Clock::time_point fg_start, Clock::time_point bg_start,
                           Clock::duration bg_duration) -> float;

void paint_bg_active_sequence(IR const &ir, juce::Graphics &g,
                              juce::Rectangle<int> const &bounds,
                              std::size_t pitch_count, juce::Colour color);

void paint_trigger_line(juce::Graphics &g, float percent_location, juce::Colour color);

[[nodiscard]]
auto calculate_duration(sequence::Measure const &m, DAWState const &daw)
    -> Clock::duration;

} // namespace xen::gui