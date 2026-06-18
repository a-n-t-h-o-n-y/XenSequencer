#pragma once
#include <memory>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

// NOTE: If you add or update a modulator, remember to modify `evaluate`, `to_json` and
// `from_json` functions below.

namespace xen::modulator
{

struct Constant;
struct Sine;
struct Triangle;
struct SawtoothUp;
struct SawtoothDown;
struct Square;
struct Scale;
struct Bias;
struct AbsoluteValue;
struct Clamp;
struct Power;
struct Chain;
struct Blend;

} // namespace xen::modulator

namespace xen
{

using Modulator =
    std::variant<modulator::Constant, modulator::Sine, modulator::Triangle,
                 modulator::SawtoothUp, modulator::SawtoothDown, modulator::Square,
                 modulator::Scale, modulator::Bias, modulator::AbsoluteValue,
                 modulator::Clamp, modulator::Power, modulator::Chain,
                 modulator::Blend>;

} // namespace xen

namespace xen::modulator
{

// GENERATORS --------------------------------------------------------------------------

struct Constant
{
    float value;
};

struct Sine
{
    float frequency;
    float amplitude = 1.f;
    float phase = 0.f;
};

struct Triangle
{
    float frequency;
    float amplitude = 1.f;
    float phase = 0.f;
};

struct SawtoothUp
{
    float frequency;
    float amplitude = 1.f;
    float phase = 0.f;
};

struct SawtoothDown
{
    float frequency;
    float amplitude = 1.f;
    float phase = 0.f;
};

struct Square
{
    float frequency;
    float amplitude = 1.f;
    float phase = 0.f;
    float pulse_width = 0.5f;
};

// MODIFIERS ---------------------------------------------------------------------------

struct Scale
{
    float factor;
};

struct Bias
{
    float amount;
};

struct AbsoluteValue
{
};

struct Clamp
{
    float min;
    float max;
};

struct Power
{
    float exponent;
};

// META / ROUTING ----------------------------------------------------------------------

struct Chain
{
    std::vector<Modulator> children;
};

struct Blend
{
    std::vector<Modulator> children;
};

} // namespace xen::modulator

// OPERATIONS --------------------------------------------------------------------------

namespace xen
{

/**
 * Evaluate a modulator at time t.
 *
 * @param mod The modulator to evaluate.
 * @param t The time/input value.
 * @return The modulated output value.
 * @throws std::invalid_argument if a waveform generator receives non-finite input.
 * @throws std::overflow_error if waveform phase calculation overflows.
 */
[[nodiscard]]
auto evaluate(Modulator const &mod, float t) -> float;

/**
 * Serialize a modulator to JSON.
 *
 * @param mod The modulator to serialize.
 * @return JSON representation of the modulator.
 */
[[nodiscard]]
auto to_json(Modulator const &mod) -> nlohmann::json;

/**
 * Deserialize a modulator from JSON.
 *
 * @param j The JSON to deserialize.
 * @param out The output variable.
 * @return The deserialized modulator.
 * @throws nlohmann::json::exception if JSON is invalid.
 */
void from_json(nlohmann::json const &j, Modulator &out);

} // namespace xen
