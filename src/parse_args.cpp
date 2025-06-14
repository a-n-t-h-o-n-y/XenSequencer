#include <xen/parse_args.hpp>

#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <xen/modulator.hpp>

namespace
{
using namespace xen;

auto parse_modulator_json(nlohmann::json const &j) -> Modulator
{
    auto const type = j.value("type", std::string{});
    if (type == "constant")
    {
        auto const value = j.at("value").get<float>();
        return modulator::constant(value);
    }
    else if (type == "sine")
    {
        auto const frequency = j.at("frequency").get<float>();
        auto const amplitude = j.value("amplitude", 1.f);
        auto const phase = j.value("phase", 0.f);
        return modulator::sine(frequency, amplitude, phase);
    }
    else if (type == "triangle")
    {
        auto const frequency = j.at("frequency").get<float>();
        auto const amplitude = j.value("amplitude", 1.f);
        auto const phase = j.value("phase", 0.f);
        return modulator::triangle(frequency, amplitude, phase);
    }
    else if (type == "sawtooth_up")
    {
        auto const frequency = j.at("frequency").get<float>();
        auto const amplitude = j.value("amplitude", 1.f);
        auto const phase = j.value("phase", 0.f);
        return modulator::sawtooth_up(frequency, amplitude, phase);
    }
    else if (type == "sawtooth_down")
    {
        auto const frequency = j.at("frequency").get<float>();
        auto const amplitude = j.value("amplitude", 1.f);
        auto const phase = j.value("phase", 0.f);
        return modulator::sawtooth_down(frequency, amplitude, phase);
    }
    else if (type == "square")
    {
        auto const frequency = j.at("frequency").get<float>();
        auto const amplitude = j.value("amplitude", 1.f);
        auto const phase = j.value("phase", 0.f);
        auto const pulse_width = j.value("pulse_width", 0.5f);
        return modulator::square(frequency, amplitude, phase, pulse_width);
    }
    else if (type == "noise")
    {
        auto const amplitude = j.value("amplitude", 1.f);
        return modulator::noise(amplitude);
    }
    else if (type == "scale")
    {
        auto const factor = j.at("factor").get<float>();
        return modulator::scale(factor);
    }
    else if (type == "bias")
    {
        auto const amount = j.at("amount").get<float>();
        return modulator::bias(amount);
    }
    else if (type == "absolute_value")
    {
        return modulator::absolute_value();
    }
    else if (type == "clamp")
    {
        auto const min = j.at("min").get<float>();
        auto const max = j.at("max").get<float>();
        return modulator::clamp(min, max);
    }
    else if (type == "invert")
    {
        return modulator::invert();
    }
    else if (type == "power")
    {
        auto const amount = j.at("amount").get<float>();
        return modulator::power(amount);
    }
    else if (type == "chain")
    {
        auto children = std::vector<Modulator>{};

        auto const children_json = j.at("children");
        for (auto const &child : children_json)
        {
            children.push_back(parse_modulator_json(child));
        }
        return modulator::chain(std::move(children));
    }
    else if (type == "blend")
    {
        auto children = std::vector<Modulator>{};

        auto const children_json = j.at("children");
        for (auto const &child : children_json)
        {
            children.push_back(parse_modulator_json(child));
        }
        return modulator::blend(std::move(children));
    }
    else
    {
        throw std::invalid_argument{"Modulator type cannot be parsed: \"" + type + '"'};
    }
}

} // namespace

namespace xen
{

auto parse_int(std::string const &x) -> std::optional<int>
{
    try
    {
        auto pos = std::size_t{0};

        // Pass 0 as the base to auto-detect from the prefix
        auto const result = std::stoi(x, &pos, 0);

        // This verifies the entire string was parsed.
        if (pos != x.size())
        {
            return std::nullopt;
        }

        return result;
    }
    catch (std::invalid_argument const &)
    {
        return std::nullopt;
    }
    catch (std::out_of_range const &)
    {
        return std::nullopt;
    }
}

auto parse_bool(std::string const &x) -> std::optional<bool>
{
    auto const lower = to_lower(x);
    if (lower == "true")
    {
        return true;
    }
    else if (lower == "false")
    {
        return false;
    }
    else
    {
        return std::nullopt;
    }
}

auto parse_time_signature(std::string const &x) -> sequence::TimeSignature
{
    auto ss = std::istringstream{x};
    auto ts = sequence::TimeSignature{};
    ss >> ts;
    if (!ss.eof())
    {
        throw std::invalid_argument{"Invalid time signature format: " + x};
    }
    return ts;
}

auto parse_modulator(std::string const &mod_json) -> Modulator
{
    // Split into a separate function because it is a recursive structure and I don't
    // want to go from string to json to string to json etc... each time a layer is
    // encountered.
    return parse_modulator_json(nlohmann::json::parse(mod_json));
}

} // namespace xen
