#pragma once

#include <cstddef>
#include <vector>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <sequence/sequence.hpp>

#include <xen/clock.hpp>

namespace xen::gui
{

/**
 * Intermediate Representation of background sequence drawing.
 */
struct IR
{
    /// Each Rectangle has dimensions [0..1]
    std::vector<juce::Rectangle<float>> rectangles;
};

struct Window
{
    float offset; // How far into bg to begin [0..1]
    float length; // How many lengths of bg to display [0..inf]
};

[[nodiscard]]
auto generate_ir(sequence::Cell const &cell, std::size_t tuning_length) -> IR;

/**
 * Create a Window over a background active sequence to determine where to start in the
 * background sequence and how many times to repeat.
 */
[[nodiscard]]
auto generate_window(Clock::time_point fg_start, Clock::duration fg_duration,
                     Clock::time_point bg_start, Clock::duration bg_duration,
                     Clock::time_point now) -> Window;

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
 * @return
 *   New IR whose rectangles lie within [0..1], representing the windowed slice.
 */
[[nodiscard]]
auto apply_window(IR const &ir, Window const &window) -> IR;

[[nodiscard]]
auto get_bg_offset_from_fg(Clock::time_point fg_start, Clock::duration fg_duration,
                           Clock::time_point bg_start, Clock::time_point now) -> float;

void paint_bg_active_sequence(IR const &ir, float bg_offset, juce::Graphics &g);

} // namespace xen::gui