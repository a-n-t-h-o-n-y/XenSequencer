#include <xen/modulation.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>

namespace xen
{
namespace
{

constexpr auto TABLE_SIZE = std::size_t{1'024};
constexpr auto TWO_PI = 2.f * std::numbers::pi_v<float>;

auto const sine_table = [] {
    auto table = std::array<float, TABLE_SIZE>{};
    for (auto i = std::size_t{}; i < table.size(); ++i)
    {
        table[i] =
            std::sin(TWO_PI * static_cast<float>(i) / static_cast<float>(table.size()));
    }
    return table;
}();

auto const triangle_table = [] {
    auto table = std::array<float, TABLE_SIZE>{};
    for (auto i = std::size_t{}; i < table.size(); ++i)
    {
        auto const x = static_cast<float>(i) / static_cast<float>(table.size());
        table[i] = 4.f * std::abs(x - 0.5f) - 1.f;
    }
    return table;
}();

auto const sawtooth_up_table = [] {
    auto table = std::array<float, TABLE_SIZE>{};
    for (auto i = std::size_t{}; i < table.size(); ++i)
    {
        auto const x = static_cast<float>(i) / static_cast<float>(table.size());
        table[i] = 2.f * x - 1.f;
    }
    return table;
}();

auto const sawtooth_down_table = [] {
    auto table = std::array<float, TABLE_SIZE>{};
    for (auto i = std::size_t{}; i < table.size(); ++i)
    {
        auto const x = static_cast<float>(i) / static_cast<float>(table.size());
        table[i] = 1.f - 2.f * x;
    }
    return table;
}();

[[nodiscard]] auto normalized_phase(double phase) -> double
{
    if (!std::isfinite(phase))
    {
        throw std::overflow_error{"Modulation phase must be finite."};
    }
    return phase - std::floor(phase);
}

[[nodiscard]] auto sample_table(std::array<float, TABLE_SIZE> const &table,
                                double phase) -> float
{
    auto const scaled = normalized_phase(phase) * static_cast<double>(TABLE_SIZE);
    auto const index = static_cast<std::size_t>(scaled);
    auto const fraction = static_cast<float>(scaled - static_cast<double>(index));
    auto const next = (index + 1) % TABLE_SIZE;
    return table[index] + fraction * (table[next] - table[index]);
}

[[nodiscard]] auto sample_shape(WaveformShape shape, double phase) -> float
{
    switch (shape)
    {
    case WaveformShape::Sine:
        return sample_table(sine_table, phase);
    case WaveformShape::Triangle:
        return sample_table(triangle_table, phase);
    case WaveformShape::SawtoothUp:
        return sample_table(sawtooth_up_table, phase);
    case WaveformShape::SawtoothDown:
        return sample_table(sawtooth_down_table, phase);
    case WaveformShape::Square:
        return normalized_phase(phase) < 0.5 ? 1.f : -1.f;
    }
    throw std::invalid_argument{"Unknown waveform shape."};
}

[[nodiscard]] auto sample_waveform(ModulationWaveform const &waveform, double x)
    -> float
{
    auto const phase = static_cast<double>(waveform.frequency) * x +
                       static_cast<double>(waveform.phase);
    auto const value = static_cast<double>(waveform.amplitude_offset) +
                       static_cast<double>(waveform.amplitude) *
                           static_cast<double>(sample_shape(waveform.shape, phase));
    if (!std::isfinite(value))
    {
        throw std::overflow_error{"Modulation waveform output must be finite."};
    }
    return static_cast<float>(value);
}

[[nodiscard]] auto enabled_waveforms(ModulationDefinition const &modulation)
    -> std::vector<ModulationWaveform const *>
{
    auto enabled = std::vector<ModulationWaveform const *>{};
    enabled.reserve(modulation.waveforms.size());
    for (auto const &waveform : modulation.waveforms)
    {
        if (waveform.enabled)
        {
            enabled.push_back(&waveform);
        }
    }
    return enabled;
}

[[nodiscard]] auto is_binary(ModulationOperation operation) -> bool
{
    return operation == ModulationOperation::AmplitudeModulation ||
           operation == ModulationOperation::RingModulation ||
           operation == ModulationOperation::FrequencyModulation ||
           operation == ModulationOperation::PhaseModulation;
}

void validate_parameter(float value, float minimum, float maximum, char const *name)
{
    if (!std::isfinite(value) || value < minimum || value > maximum)
    {
        throw std::invalid_argument{std::string{name} + " must be in [" +
                                    std::to_string(minimum) + ", " +
                                    std::to_string(maximum) + "]."};
    }
}

[[nodiscard]] auto normalize(double output) -> float
{
    if (!std::isfinite(output))
    {
        throw std::overflow_error{"Combined modulation output must be finite."};
    }
    return static_cast<float>(std::clamp((output + 1.0) / 2.0, 0.0, 1.0));
}

[[nodiscard]] auto evaluate_reducer(
    ModulationOperation operation, std::span<ModulationWaveform const *const> waveforms,
    double x) -> double
{
    auto output = operation == ModulationOperation::Product ? 1.0 : 0.0;
    for (auto const *waveform : waveforms)
    {
        auto const value = static_cast<double>(sample_waveform(*waveform, x));
        if (operation == ModulationOperation::Product)
        {
            output *= value;
        }
        else
        {
            output += value;
        }
    }
    if (operation == ModulationOperation::Average)
    {
        output /= static_cast<double>(waveforms.size());
    }
    return output;
}

} // namespace

void validate(ModulationDefinition const &modulation)
{
    if (modulation.waveforms.empty())
    {
        throw std::invalid_argument{"Modulation must contain at least one waveform."};
    }
    if (modulation.waveforms.size() > MAX_MODULATION_WAVEFORMS)
    {
        throw std::invalid_argument{"Modulation contains too many waveforms."};
    }
    for (auto const &waveform : modulation.waveforms)
    {
        validate_parameter(waveform.frequency, 0.f, MAX_MODULATION_FREQUENCY,
                           "Waveform frequency");
        validate_parameter(waveform.phase, 0.f, 1.f, "Waveform phase");
        validate_parameter(waveform.amplitude, -1.f, 1.f, "Waveform amplitude");
        validate_parameter(waveform.amplitude_offset, -1.f, 1.f,
                           "Waveform amplitude offset");
    }
    auto const enabled =
        std::ranges::count(modulation.waveforms, true, &ModulationWaveform::enabled);
    if (enabled == 0)
    {
        throw std::invalid_argument{"Modulation must enable at least one waveform."};
    }
    if (is_binary(modulation.operation) && enabled != 2)
    {
        throw std::invalid_argument{
            "Binary modulation operations require exactly two enabled waveforms."};
    }
}

void validate(ModulationDestination destination, ModulationOutputRange const &range)
{
    if (!std::isfinite(range.minimum) || !std::isfinite(range.maximum) ||
        range.minimum > range.maximum)
    {
        throw std::invalid_argument{"Modulation output range is invalid."};
    }
    if (destination == ModulationDestination::Pitch)
    {
        auto const int_min = static_cast<double>(std::numeric_limits<int>::min());
        auto const int_max = static_cast<double>(std::numeric_limits<int>::max());
        if (range.minimum < int_min || range.maximum > int_max ||
            std::trunc(range.minimum) != range.minimum ||
            std::trunc(range.maximum) != range.maximum)
        {
            throw std::invalid_argument{
                "Pitch modulation range must contain integer endpoints."};
        }
        return;
    }
    if (destination == ModulationDestination::Weight)
    {
        if (range.minimum <
                static_cast<double>(std::numeric_limits<float>::denorm_min()) ||
            range.maximum > static_cast<double>(std::numeric_limits<float>::max()))
        {
            throw std::invalid_argument{
                "Weight modulation range must be positive and fit in float."};
        }
        return;
    }
    if (range.minimum < 0.0 || range.maximum > 1.0)
    {
        throw std::invalid_argument{
            "Velocity, delay, and gate modulation ranges must be in [0, 1]."};
    }
}

auto evaluate(ModulationDefinition const &modulation, std::size_t sample_count)
    -> std::vector<float>
{
    validate(modulation);
    auto output = std::vector<float>(sample_count);
    if (sample_count == 0)
    {
        return output;
    }

    auto const waveforms = enabled_waveforms(modulation);
    auto const dx = 1.0 / static_cast<double>(sample_count);
    auto fm_phase = static_cast<double>(waveforms.front()->phase);

    for (auto i = std::size_t{}; i < sample_count; ++i)
    {
        auto const x = static_cast<double>(i) / static_cast<double>(sample_count);
        auto combined = 0.0;
        switch (modulation.operation)
        {
        case ModulationOperation::Average:
        case ModulationOperation::Sum:
        case ModulationOperation::Product:
            combined = evaluate_reducer(modulation.operation, waveforms, x);
            break;
        case ModulationOperation::AmplitudeModulation: {
            auto const carrier = sample_waveform(*waveforms[0], x);
            auto const modulator = sample_waveform(*waveforms[1], x);
            combined =
                static_cast<double>(carrier) *
                ((std::clamp(static_cast<double>(modulator), -1.0, 1.0) + 1.0) / 2.0);
            break;
        }
        case ModulationOperation::RingModulation:
            combined = static_cast<double>(sample_waveform(*waveforms[0], x)) *
                       static_cast<double>(sample_waveform(*waveforms[1], x));
            break;
        case ModulationOperation::PhaseModulation: {
            auto const &carrier = *waveforms[0];
            auto const modulator = sample_waveform(*waveforms[1], x);
            auto const carrier_phase = static_cast<double>(carrier.frequency) * x +
                                       static_cast<double>(carrier.phase) +
                                       static_cast<double>(modulator);
            combined = static_cast<double>(carrier.amplitude_offset) +
                       static_cast<double>(carrier.amplitude) *
                           sample_shape(carrier.shape, carrier_phase);
            break;
        }
        case ModulationOperation::FrequencyModulation: {
            auto const &carrier = *waveforms[0];
            auto const modulator = sample_waveform(*waveforms[1], x);
            combined = static_cast<double>(carrier.amplitude_offset) +
                       static_cast<double>(carrier.amplitude) *
                           sample_shape(carrier.shape, fm_phase);
            fm_phase += (static_cast<double>(carrier.frequency) +
                         static_cast<double>(modulator)) *
                        dx;
            break;
        }
        }
        output[i] = normalize(combined);
    }
    return output;
}

} // namespace xen
