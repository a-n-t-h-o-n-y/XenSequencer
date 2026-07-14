#include <xen/scale.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <xen/constants.hpp>
#include <xen/string_manip.hpp>
#include <xen/utility.hpp>

#include "numeric.hpp"

namespace YAML
{
template <>
struct convert<::xen::LibraryScale>
{
    static auto decode(Node const &node, ::xen::LibraryScale &entry) -> bool
    {
        if (!node["id"] || !node["name"] || !node["tuning_length"] ||
            !node["intervals"])
        {
            return false;
        }
        entry.id = node["id"].as<std::string>();
        auto &scale = entry.definition;
        scale.name = ::xen::to_lower(node["name"].as<std::string>());
        scale.tuning_length = node["tuning_length"].as<std::size_t>();
        auto const intervals = node["intervals"].as<std::vector<unsigned>>();
        scale.intervals.clear();
        scale.intervals.reserve(intervals.size());
        for (auto const interval : intervals)
        {
            if (interval == 0 || interval > std::numeric_limits<std::uint8_t>::max())
            {
                throw std::invalid_argument{
                    "Scale intervals must be in the range [1, 255]."};
            }
            scale.intervals.push_back(static_cast<std::uint8_t>(interval));
        }
        if (node["mode"])
        {
            auto const mode = node["mode"].as<unsigned>();
            if (mode > std::numeric_limits<std::uint8_t>::max())
            {
                throw std::invalid_argument{"Scale mode is out of range."};
            }
            scale.mode = static_cast<std::uint8_t>(mode);
        }
        else
        {
            scale.mode = 1;
        }
        ::xen::validate_scale(scale);
        return true;
    }
};
} // namespace YAML

namespace xen
{

auto Scale::operator==(Scale const &other) const -> bool
{
    return std::tie(this->tuning_length, this->intervals, this->mode) ==
           std::tie(other.tuning_length, other.intervals, other.mode);
}

auto Scale::operator!=(Scale const &other) const -> bool
{
    return !(*this == other);
}

void validate_scale(Scale const &scale)
{
    if (scale.tuning_length == 0 || !std::in_range<int>(scale.tuning_length))
    {
        throw std::invalid_argument{
            "Scale tuning_length must be representable as a positive int."};
    }
    if (scale.intervals.empty() || scale.intervals.size() > 255)
    {
        throw std::invalid_argument{"Scale must contain between 1 and 255 intervals."};
    }
    if (std::ranges::find(scale.intervals, std::uint8_t{0}) !=
        std::end(scale.intervals))
    {
        throw std::invalid_argument{"Scale intervals must be in the range [1, 255]."};
    }
    if (scale.mode == 0 || scale.mode > scale.intervals.size())
    {
        throw std::invalid_argument{"Scale mode must be in the interval range."};
    }
    auto const interval_sum =
        std::accumulate(scale.intervals.begin(), scale.intervals.end(), std::size_t{0});
    if (interval_sum != scale.tuning_length)
    {
        throw std::invalid_argument{"Scale intervals must sum to tuning_length."};
    }
}

auto load_scales(std::string const &system_yaml, std::string const &user_yaml)
    -> std::vector<LibraryScale>
{
    auto const system_node = YAML::Load(system_yaml);
    auto const user_node = YAML::Load(user_yaml);
    auto system_scales = system_node["scales"].as<std::vector<LibraryScale>>();
    auto user_scales = std::vector<LibraryScale>{};
    if (user_node["scales"])
    {
        user_scales = user_node["scales"].as<std::vector<LibraryScale>>();
    }
    system_scales.insert(std::end(system_scales),
                         std::make_move_iterator(std::begin(user_scales)),
                         std::make_move_iterator(std::end(user_scales)));
    auto ids = std::vector<std::string>{};
    ids.reserve(system_scales.size());
    for (auto const &entry : system_scales)
    {
        if (entry.id.empty())
        {
            throw std::invalid_argument{"Scale ID must not be empty."};
        }
        if (std::ranges::contains(ids, entry.id))
        {
            throw std::invalid_argument{"Duplicate scale ID: " + entry.id};
        }
        ids.push_back(entry.id);
    }
    return system_scales;
}

auto generate_valid_pitches(xen::Scale const &scale) -> std::vector<int>
{
    validate_scale(scale);
    auto intervals = scale.intervals;
    std::ranges::rotate(intervals, std::next(std::begin(intervals), scale.mode - 1));

    auto result = std::vector<int>{0};

    for (int interval : intervals)
    {
        result.push_back(numeric::checked_add(result.back(), interval,
                                              "Scale interval sum exceeds int."));
    }
    return result;
}

auto map_pitch_to_scale(int pitch, std::vector<int> const &valid_pitches,
                        std::size_t tuning_length, TranslateDirection direction) -> int
{
    if (valid_pitches.empty())
    {
        throw std::invalid_argument{"Valid pitches must not be empty."};
    }
    if (tuning_length == 0 || !std::in_range<int>(tuning_length))
    {
        throw std::invalid_argument{
            "Scale mapping tuning length must be representable as a positive int."};
    }

    auto const int_tuning_length = static_cast<int>(tuning_length);
    auto octave_shift = utility::get_octave(pitch, tuning_length);
    auto const normalized_pitch =
        static_cast<int>(utility::normalize_pitch(pitch, tuning_length));

    auto it = std::ranges::lower_bound(valid_pitches, normalized_pitch);

    if (it == std::end(valid_pitches) || *it != normalized_pitch)
    {
        if (direction == TranslateDirection::Down)
        {
            if (it == std::begin(valid_pitches))
            {
                it = std::prev(std::end(valid_pitches));
                octave_shift = numeric::checked_add(
                    octave_shift, -1, "Scale mapping octave shift exceeds int.");
            }
            else
            {
                --it;
            }
        }
        else
        { // TranslateDirection::Up
            if (it == std::end(valid_pitches))
            {
                it = std::begin(valid_pitches);
                octave_shift = numeric::checked_add(
                    octave_shift, 1, "Scale mapping octave shift exceeds int.");
            }
        }
    }

    auto const octave_offset = numeric::checked_mul(
        octave_shift, int_tuning_length, "Scale mapping octave offset exceeds int.");
    return numeric::checked_add(*it, octave_offset, "Mapped pitch exceeds int.");
}

} // namespace xen
