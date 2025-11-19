#include <xen/modulator.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace
{
constexpr auto TABLE_SIZE = std::size_t{1'024};
constexpr auto TWO_PI = 2.f * std::numbers::pi_v<float>;

// Pre-calculated wave tables
auto const sine_table = [] {
    auto table = std::array<float, TABLE_SIZE>{};
    for (auto i = std::size_t{0}; i < TABLE_SIZE; ++i)
    {
        auto const phase = static_cast<float>(i) / static_cast<float>(TABLE_SIZE);
        table[i] = std::sin(TWO_PI * phase);
    }
    return table;
}();

auto const triangle_table = []() {
    auto table = std::array<float, TABLE_SIZE>{};
    for (auto i = std::size_t{0}; i < TABLE_SIZE; ++i)
    {
        auto const x = static_cast<float>(i) / static_cast<float>(TABLE_SIZE);
        table[i] = 4.f * std::abs(x - 0.5f) - 1.f;
    }
    return table;
}();

auto const sawtooth_up_table = []() {
    auto table = std::array<float, TABLE_SIZE>{};
    for (auto i = std::size_t{0}; i < TABLE_SIZE; ++i)
    {
        auto const x = static_cast<float>(i) / static_cast<float>(TABLE_SIZE);
        table[i] = 2.f * x - 1.f;
    }
    return table;
}();

auto const sawtooth_down_table = []() {
    auto table = std::array<float, TABLE_SIZE>{};
    for (auto i = std::size_t{0}; i < TABLE_SIZE; ++i)
    {
        auto const x = static_cast<float>(i) / static_cast<float>(TABLE_SIZE);
        table[i] = 1.f - 2.f * x;
    }
    return table;
}();

/**
 * Sample from a wave table with linear interpolation.
 *
 * @param table The wave table to sample from.
 * @param frequency The frequency in cycles per unit input.
 * @param amplitude The amplitude multiplier.
 * @param phase The phase offset (in cycles, 0-1 range).
 * @param t The time/input value.
 * @return The interpolated sample value.
 */
[[nodiscard]]
auto sample_table(std::array<float, TABLE_SIZE> const &table, float frequency,
                  float amplitude, float phase, float t) -> float
{
    auto const position_raw = t * frequency + phase;
    auto const position = position_raw - std::floor(position_raw);
    auto const scaled_position = position * static_cast<float>(TABLE_SIZE);
    auto const index = static_cast<std::size_t>(scaled_position);
    auto const frac = scaled_position - static_cast<float>(index);
    auto const index0 = index % TABLE_SIZE;
    auto const index1 = (index + 1) % TABLE_SIZE;
    auto const value0 = table[index0];
    auto const value1 = table[index1];
    auto const interpolated = value0 + frac * (value1 - value0);
    return amplitude * interpolated;
}

/**
 * Generate square wave with pulse width.
 *
 * @param frequency The frequency in cycles per unit input.
 * @param amplitude The amplitude multiplier.
 * @param phase The phase offset (in cycles, 0-1 range).
 * @param pulse_width The duty cycle (0-1 range).
 * @param t The time/input value.
 * @return The square wave value.
 */
auto square_wave(float frequency, float amplitude, float phase, float pulse_width,
                 float t) -> float
{
    auto const normalized_phase = std::fmod(std::fmod(phase, 1.f) + 1.f, 1.f);
    auto const x = std::fmod(t * frequency + normalized_phase, 1.f);
    auto const square = (x < pulse_width) ? 1.f : -1.f;
    return amplitude * square;
}

} // namespace

namespace xen
{

// Helper for compile-time exhaustiveness checking
template <typename>
constexpr auto always_false = false;

using namespace xen::modulator;

auto evaluate(Modulator const &mod, float t) -> float
{
    return std::visit(
        [t](auto const &m) -> float {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, modulator::Constant>)
            {
                return m.value;
            }
            else if constexpr (std::is_same_v<T, Sine>)
            {
                return sample_table(sine_table, m.frequency, m.amplitude, m.phase, t);
            }
            else if constexpr (std::is_same_v<T, Triangle>)
            {
                return sample_table(triangle_table, m.frequency, m.amplitude, m.phase,
                                    t);
            }
            else if constexpr (std::is_same_v<T, SawtoothUp>)
            {
                return sample_table(sawtooth_up_table, m.frequency, m.amplitude,
                                    m.phase, t);
            }
            else if constexpr (std::is_same_v<T, SawtoothDown>)
            {
                return sample_table(sawtooth_down_table, m.frequency, m.amplitude,
                                    m.phase, t);
            }
            else if constexpr (std::is_same_v<T, Square>)
            {
                return square_wave(m.frequency, m.amplitude, m.phase, m.pulse_width, t);
            }
            else if constexpr (std::is_same_v<T, Scale>)
            {
                return t * m.factor;
            }
            else if constexpr (std::is_same_v<T, Bias>)
            {
                return t + m.amount;
            }
            else if constexpr (std::is_same_v<T, AbsoluteValue>)
            {
                return std::abs(t);
            }
            else if constexpr (std::is_same_v<T, Clamp>)
            {
                return std::clamp(t, m.min, m.max);
            }
            else if constexpr (std::is_same_v<T, Power>)
            {
                return std::pow(t, m.exponent);
            }
            else if constexpr (std::is_same_v<T, Chain>)
            {
                auto output = t;
                for (auto const &modulator : m.children)
                {
                    output = evaluate(modulator, output);
                }
                return output;
            }
            else if constexpr (std::is_same_v<T, Blend>)
            {
                auto output = 0.f;
                for (auto const &modulator : m.children)
                {
                    output += evaluate(modulator, t);
                }
                return output;
            }
            else
            {
                static_assert(always_false<T>,
                              "Missing evaluate implementation for modulator type");
            }
        },
        mod);
}

auto to_json(Modulator const &mod) -> nlohmann::json
{
    return std::visit(
        [](auto const &m) -> nlohmann::json {
            using T = std::decay_t<decltype(m)>;
            auto j = nlohmann::json{};

            if constexpr (std::is_same_v<T, Constant>)
            {
                j["type"] = "constant";
                j["value"] = m.value;
            }
            else if constexpr (std::is_same_v<T, Sine>)
            {
                j["type"] = "sine";
                j["frequency"] = m.frequency;
                j["amplitude"] = m.amplitude;
                j["phase"] = m.phase;
            }
            else if constexpr (std::is_same_v<T, Triangle>)
            {
                j["type"] = "triangle";
                j["frequency"] = m.frequency;
                j["amplitude"] = m.amplitude;
                j["phase"] = m.phase;
            }
            else if constexpr (std::is_same_v<T, SawtoothUp>)
            {
                j["type"] = "sawtooth_up";
                j["frequency"] = m.frequency;
                j["amplitude"] = m.amplitude;
                j["phase"] = m.phase;
            }
            else if constexpr (std::is_same_v<T, SawtoothDown>)
            {
                j["type"] = "sawtooth_down";
                j["frequency"] = m.frequency;
                j["amplitude"] = m.amplitude;
                j["phase"] = m.phase;
            }
            else if constexpr (std::is_same_v<T, Square>)
            {
                j["type"] = "square";
                j["frequency"] = m.frequency;
                j["amplitude"] = m.amplitude;
                j["phase"] = m.phase;
                j["pulse_width"] = m.pulse_width;
            }
            else if constexpr (std::is_same_v<T, Scale>)
            {
                j["type"] = "scale";
                j["factor"] = m.factor;
            }
            else if constexpr (std::is_same_v<T, Bias>)
            {
                j["type"] = "bias";
                j["amount"] = m.amount;
            }
            else if constexpr (std::is_same_v<T, AbsoluteValue>)
            {
                j["type"] = "absolute_value";
            }
            else if constexpr (std::is_same_v<T, Clamp>)
            {
                j["type"] = "clamp";
                j["min"] = m.min;
                j["max"] = m.max;
            }
            else if constexpr (std::is_same_v<T, Power>)
            {
                j["type"] = "power";
                j["exponent"] = m.exponent;
            }
            else if constexpr (std::is_same_v<T, Chain>)
            {
                j["type"] = "chain";
                auto modulators_json = nlohmann::json::array();
                for (auto const &modulator : m.children)
                {
                    modulators_json.push_back(to_json(modulator));
                }
                j["children"] = std::move(modulators_json);
            }
            else if constexpr (std::is_same_v<T, Blend>)
            {
                j["type"] = "blend";
                auto modulators_json = nlohmann::json::array();
                for (auto const &modulator : m.children)
                {
                    modulators_json.push_back(to_json(modulator));
                }
                j["children"] = std::move(modulators_json);
            }
            else
            {
                static_assert(always_false<T>,
                              "Missing to_json implementation for modulator type");
            }

            return j;
        },
        mod);
}

void from_json(nlohmann::json const &j, Modulator &out)
{
    auto const type = j.at("type").get<std::string>();

    if (type == "constant")
    {
        out = Constant{
            .value = j.at("value").get<float>(),
        };
        return;
    }
    else if (type == "sine")
    {
        out = Sine{
            .frequency = j.at("frequency").get<float>(),
            .amplitude = j.at("amplitude").get<float>(),
            .phase = j.at("phase").get<float>(),
        };
        return;
    }
    else if (type == "triangle")
    {
        out = Triangle{
            .frequency = j.at("frequency").get<float>(),
            .amplitude = j.at("amplitude").get<float>(),
            .phase = j.at("phase").get<float>(),
        };
        return;
    }
    else if (type == "sawtooth_up")
    {
        out = SawtoothUp{
            .frequency = j.at("frequency").get<float>(),
            .amplitude = j.at("amplitude").get<float>(),
            .phase = j.at("phase").get<float>(),
        };
        return;
    }
    else if (type == "sawtooth_down")
    {
        out = SawtoothDown{
            .frequency = j.at("frequency").get<float>(),
            .amplitude = j.at("amplitude").get<float>(),
            .phase = j.at("phase").get<float>(),
        };
        return;
    }
    else if (type == "square")
    {
        out = Square{
            .frequency = j.at("frequency").get<float>(),
            .amplitude = j.at("amplitude").get<float>(),
            .phase = j.at("phase").get<float>(),
            .pulse_width = j.at("pulse_width").get<float>(),
        };
        return;
    }
    else if (type == "scale")
    {
        out = Scale{
            .factor = j.at("factor").get<float>(),
        };
        return;
    }
    else if (type == "bias")
    {
        out = Bias{
            .amount = j.at("amount").get<float>(),
        };
        return;
    }
    else if (type == "absolute_value")
    {
        out = AbsoluteValue{};
        return;
    }
    else if (type == "clamp")
    {
        out = Clamp{
            .min = j.at("min").get<float>(),
            .max = j.at("max").get<float>(),
        };
        return;
    }
    else if (type == "power")
    {
        out = Power{
            .exponent = j.at("exponent").get<float>(),
        };
        return;
    }
    else if (type == "chain")
    {
        auto children = std::vector<Modulator>{};
        for (auto const &mod_json : j.at("children"))
        {
            auto &child = children.emplace_back();
            from_json(mod_json, child);
        }
        out = Chain{
            .children = std::move(children),
        };
        return;
    }
    else if (type == "blend")
    {
        auto children = std::vector<Modulator>{};
        for (auto const &mod_json : j.at("children"))
        {
            auto &child = children.emplace_back();
            from_json(mod_json, child);
        }
        out = Blend{
            .children = std::move(children),
        };
        return;
    }
    throw std::invalid_argument{"Unknown modulator type: " + type};
}

} // namespace xen