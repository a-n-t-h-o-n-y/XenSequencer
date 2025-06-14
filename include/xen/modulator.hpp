#pragma once

#include <functional>
#include <utility>
#include <vector>

// NOTE: If you add or update a modulator, go to parse_args.cpp and update the
// parse_modulator function.

namespace xen
{

using Modulator = std::function<float(float)>;

} // namespace xen

namespace xen::modulator
{

// GENERATORS --------------------------------------------------------------------------

[[nodiscard]]
auto constant(float value) -> Modulator;

/**
 * Create a Modulator object to generate sine values.
 *
 * @param frequency The frequency of the sine wave in cycles per unit input; for
 * example, a frequency of 5 produces 5 cycles over the input range [0, 1].
 * @param amplitude The amplitude of the sine wave. Defaults to 1. Output will be up to
 * this value and down to the negative of it.
 * @param phase The offset to apply to the `t` input; phase.
 * @return A Modulator object that generates sine values.
 * @throws std::invalid_argument if frequency is not positive.
 */
[[nodiscard]]
auto sine(float frequency, float amplitude = 1.f, float phase = 0.f) -> Modulator;

[[nodiscard]]
auto triangle(float frequency, float amplitude = 1.f, float phase = 0.f) -> Modulator;

[[nodiscard]]
auto sawtooth_up(float frequency, float amplitude = 1.f, float phase = 0.f)
    -> Modulator;

[[nodiscard]]
auto sawtooth_down(float frequency, float amplitude = 1.f, float phase = 0.f)
    -> Modulator;

[[nodiscard]]
auto square(float frequency, float amplitude = 1.f, float phase = 0.f,
            float pulse_width = 0.5f) -> Modulator;

[[nodiscard]]
auto noise(float amplitude = 1.f) -> Modulator;

// MODIFIERS ---------------------------------------------------------------------------

/**
 * Scale the input by a constant amount.
 *
 * @param factor Scale factor to apply (output = input * factor).
 */
[[nodiscard]]
auto scale(float factor) -> Modulator;

/**
 * Offset the input by a contant amount.
 *
 * @param amount Amount to offset the input by (output = input + amount).
 */
[[nodiscard]]
auto bias(float amount) -> Modulator;

/**
 * Returns the absolute value of the input.
 */
[[nodiscard]]
auto absolute_value() -> Modulator;

/**
 * Clamps the input to the range [min, max].
 */
[[nodiscard]]
auto clamp(float min, float max) -> Modulator;

/**
 * Inverts the input by multiplying by -1.
 */
[[nodiscard]]
auto invert() -> Modulator;

[[nodiscard]]
auto power(float amount) -> Modulator;

// META / ROUTING ----------------------------------------------------------------------

/**
 * Process each modulator in series in the order given.
 *
 * @details The input to this Modulator is passed to the first modulator in the vector,
 * and its results are passed to the next, etc... and the last is returned by this
 * Modulator.
 * @param mods The Modulators to process in series. If this is empty, the input is
 * passed directly to the output.
 */
[[nodiscard]]
auto chain(std::vector<Modulator> mods) -> Modulator;

/**
 * Runs the input through each Modulator, then sums the results into a single result.
 *
 * @param mods The Modulators to process in parallel. If this is empty, the result will
 * always be zero.
 */
[[nodiscard]]
auto blend(std::vector<Modulator> mods) -> Modulator;

} // namespace xen::modulator