#include <xen/modulator.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <stdexcept>
#include <vector>

namespace xen::modulator
{

// GENERATORS --------------------------------------------------------------------------

auto constant(float value) -> Modulator
{
    return [value](float) -> float { return value; };
}

auto sine(float frequency, float amplitude, float phase) -> Modulator
{
    if (frequency < 0.f)
    {
        throw std::invalid_argument{"Frequency must be non-negative."};
    }
    return [frequency, amplitude, phase](float t) -> float {
        auto const two_pi = 2.f * std::numbers::pi_v<float>;
        return amplitude * std::sin(two_pi * frequency * (t + phase));
    };
}

auto triangle(float frequency, float amplitude, float phase) -> Modulator
{
    if (frequency < 0.f)
    {
        throw std::invalid_argument{"Frequency must be non-negative."};
    }

    return [frequency, amplitude, phase](float t) -> float {
        auto const wrapped_t = std::fmod(t + phase, 1.0f);
        auto const x = std::fmod(wrapped_t * frequency, 1.0f); // map to [0, 1)
        auto const tri = 4.f * std::abs(x - 0.5f) - 1.f; // triangle wave in [-1, 1]
        return amplitude * tri;
    };
}

auto sawtooth_up(float frequency, float amplitude, float phase) -> Modulator
{
    if (frequency < 0.f)
    {
        throw std::invalid_argument{"Frequency must be non-negative."};
    }

    return [frequency, amplitude, phase](float t) -> float {
        auto const wrapped_t = std::fmod(t + phase, 1.0f);
        auto const x = std::fmod(wrapped_t * frequency, 1.0f); // map to [0, 1)
        auto const saw = 2.f * x - 1.f; // sawtooth wave in [-1, 1]
        return amplitude * saw;
    };
}

auto sawtooth_down(float frequency, float amplitude, float phase) -> Modulator
{
    if (frequency < 0.f)
    {
        throw std::invalid_argument{"Frequency must be non-negative."};
    }

    return [frequency, amplitude, phase](float t) -> float {
        auto const wrapped_t = std::fmod(t + phase, 1.0f);
        auto const x = std::fmod(wrapped_t * frequency, 1.0f); // map to [0, 1)
        auto const saw = 1.f - 2.f * x; // inverted sawtooth wave in [-1, 1]
        return amplitude * saw;
    };
}

auto square(float frequency, float amplitude, float phase, float pulse_width)
    -> Modulator
{
    if (frequency < 0.f)
    {
        throw std::invalid_argument{"Frequency must be non-negative."};
    }
    if (pulse_width < 0.f || pulse_width > 1.f)
    {
        throw std::invalid_argument{"Pulse width must be in the range [0, 1]"};
    }

    return [frequency, amplitude, phase, pulse_width](float t) -> float {
        auto const wrapped_t = std::fmod(t + phase, 1.0f);
        auto const x = std::fmod(wrapped_t * frequency, 1.0f); // map to [0, 1)
        auto const square = (x < pulse_width) ? 1.f : -1.f;    // square wave in [-1, 1]
        return amplitude * square;
    };
}

auto noise(float amplitude) -> Modulator
{
    return [amplitude](float) -> float {
        static std::mt19937 gen{std::random_device{}()};
        static std::uniform_real_distribution<float> dist{-1.f, 1.f};
        return amplitude * dist(gen);
    };
}

// MODIFIERS ---------------------------------------------------------------------------

auto scale(float factor) -> Modulator
{
    return [factor](float input) -> float { return input * factor; };
}

auto bias(float amount) -> Modulator
{
    return [amount](float input) -> float { return input + amount; };
}

auto absolute_value() -> Modulator
{
    return [](float input) -> float { return std::abs(input); };
}

auto clamp(float min, float max) -> Modulator
{
    return [min, max](float input) -> float { return std::clamp(input, min, max); };
}

auto invert() -> Modulator
{
    return modulator::scale(-1.f);
}

/**
 * Creates a modulator that raises the input to the specified power.
 *
 * @param exponent The power to which the input will be raised. Must be non-negative.
 * @return A Modulator function that maps x to x^exponent.
 * @throws std::invalid_argument if exponent < 0.
 */
auto power(float exponent) -> Modulator
{
    if (exponent < 0.f)
    {
        throw std::invalid_argument{"Exponent must be non-negative."};
    }
    return [exponent](float x) -> float { return std::pow(x, exponent); };
}

// META / ROUTING ----------------------------------------------------------------------

auto chain(std::vector<Modulator> mods) -> Modulator
{
    return [modulators = std::move(mods)](float input) -> float {
        auto output = input;
        for (auto const &mod : modulators)
        {
            output = mod(output);
        }
        return output;
    };
}

auto blend(std::vector<Modulator> mods) -> Modulator
{
    return [modulators = std::move(mods)](float const input) -> float {
        auto output = 0.f;
        for (auto const &mod : modulators)
        {
            output += mod(input);
        }
        return output;
    };
}

} // namespace xen::modulator