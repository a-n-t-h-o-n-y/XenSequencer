#include <xen/scale.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <xen/constants.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/utility.hpp>

namespace
{

[[nodiscard]] auto euclid_mod(int a, int b) -> int
{
    return (a % b + b) % b;
}

} // namespace

namespace xen
{

auto load_scales_from_files() -> std::vector<Scale>
{
    auto system_node =
        YAML::LoadFile(get_system_scales_file().getFullPathName().toStdString());
    auto user_node =
        YAML::LoadFile(get_user_scales_file().getFullPathName().toStdString());

    auto system_scales = system_node["scales"].as<std::vector<Scale>>();
    auto user_scales = std::vector<Scale>{};
    if (user_node["scales"])
    {
        user_scales = user_node["scales"].as<std::vector<Scale>>();
    }
    system_scales.insert(std::end(system_scales),
                         std::make_move_iterator(std::begin(user_scales)),
                         std::make_move_iterator(std::end(user_scales)));
    return system_scales;
}

auto generate_valid_pitches(xen::Scale const &scale) -> std::vector<int>
{
    auto intervals = scale.intervals;
    std::ranges::rotate(intervals, std::next(std::begin(intervals), scale.mode));

    auto result = std::vector<int>{0};

    for (int interval : intervals)
    {
        result.push_back(result.back() + interval);
    }
    return result;
}

auto map_pitch_to_scale(int pitch, std::vector<int> const &valid_pitches,
                        std::size_t tuning_length, TranslateDirection direction) -> int
{
    auto octave_shift = (int)std::floor(static_cast<double>(pitch) / tuning_length);
    auto normalized_pitch = euclid_mod(pitch, tuning_length);

    auto it = std::ranges::lower_bound(valid_pitches, normalized_pitch);

    if (it == std::end(valid_pitches) || *it != normalized_pitch)
    {
        if (direction == TranslateDirection::Down)
        {
            if (it == std::begin(valid_pitches))
            {
                it = std::prev(std::end(valid_pitches));
                octave_shift--;
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
                octave_shift++;
            }
        }
    }

    return *it + octave_shift * tuning_length;
}

} // namespace xen

namespace YAML
{

auto convert<::xen::Scale>::decode(Node const &node, ::xen::Scale &scale) -> bool
{
    if (!node["name"] || !node["tuning_length"] || !node["intervals"])
    {
        return false;
    }
    scale.name = ::xen::to_lower(node["name"].as<std::string>());
    scale.tuning_length = node["tuning_length"].as<std::size_t>();
    scale.intervals = node["intervals"].as<std::vector<std::uint8_t>>();
    if (node["mode"])
    {
        scale.mode = node["mode"].as<std::uint8_t>();
    }
    else
    {
        scale.mode = 0;
    }
    return true;
}

} // namespace YAML