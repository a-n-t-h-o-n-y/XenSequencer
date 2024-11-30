#include <xen/modulators.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <stdexcept>

namespace xen
{

ConstantModulator::ConstantModulator(float value) : value_{value}
{
}

auto ConstantModulator::operator()(float) -> float
{
    return value_;
}

// -------------------------------------------------------------------------------------

SineModulator::SineModulator(float amplitude, float frequency, float phase)
    : amplitude_{amplitude}, frequency_{frequency}, phase_{phase}
{
}

auto SineModulator::operator()(float t) -> float
{
    auto const two_pi = 2.f * std::numbers::pi_v<float>;
    auto const value = amplitude_ * std::sin(two_pi * frequency_ * t) + phase_;
    return 0.5f * value + 0.5f; // Map from [-1, 1] to [0, 1]
}

// -------------------------------------------------------------------------------------

TriangleModulator::TriangleModulator(float amplitude, float frequency, float phase)
    : amplitude_{amplitude}, frequency_{frequency}, phase_{phase}
{
}

auto TriangleModulator::operator()(float t) -> float
{
    auto const phase = std::fmod(t * frequency_ + phase_, 1.f);
    auto const value = 4.f * std::abs(phase - 0.5f) - 1.f;
    return amplitude_ * (0.5f * value + 0.5f); // Map from [-1, 1] to [0, 1]
}

// -------------------------------------------------------------------------------------

SawtoothUpModulator::SawtoothUpModulator(float amplitude, float frequency, float phase)
    : amplitude_{amplitude}, frequency_{frequency}, phase_{phase}
{
}

auto SawtoothUpModulator::operator()(float t) -> float
{
    auto const phase = std::fmod(t * frequency_ + phase_, 1.f);
    auto const value = 2.f * (phase - 0.5f);
    return amplitude_ * (0.5f * value + 0.5f); // Map from [-1, 1] to [0, 1]
}

// -------------------------------------------------------------------------------------

SawtoothDownModulator::SawtoothDownModulator(float amplitude, float frequency,
                                             float phase)
    : amplitude_{amplitude}, frequency_{frequency}, phase_{phase}
{
}

auto SawtoothDownModulator::operator()(float t) -> float
{
    auto const phase = std::fmod(t * frequency_ + phase_, 1.f);
    auto const value = 2.f * (0.5f - phase);
    return amplitude_ * (0.5f * value + 0.5f); // Map from [-1, 1] to [0, 1]
}

// -------------------------------------------------------------------------------------

SquareModulator::SquareModulator(float amplitude, float frequency, float phase,
                                 float pulse_width)
    : amplitude_{amplitude}, frequency_{frequency}, phase_{phase},
      pulse_width_{pulse_width}
{
    if (pulse_width_ < 0.f || pulse_width_ > 1.f)
    {
        throw std::invalid_argument{"Pulse width must be in the range [0, 1]"};
    }
}

auto SquareModulator::operator()(float t) -> float
{
    auto const phase = std::fmod(t * frequency_ + phase_, 1.f);
    auto const value = phase < pulse_width_ ? 1.f : -1.f;
    return amplitude_ * (0.5f * value + 0.5f); // Map from [-1, 1] to [0, 1]
}

// -------------------------------------------------------------------------------------

NoiseModulator::NoiseModulator(float amplitude) : amplitude_{amplitude}
{
}

auto NoiseModulator::operator()(float) -> float
{
    auto gen = std::mt19937{std::random_device{}()};
    auto dist = std::uniform_real_distribution<float>{-1.f, 1.f};
    return 0.5f * amplitude_ * dist(gen) + 0.5f; // Map from [-1, 1] to [0, 1]
}

} // namespace xen
