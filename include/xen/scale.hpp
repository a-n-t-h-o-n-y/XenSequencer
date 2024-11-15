#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace xen
{

/**
 * Struct to hold musical scale data.
 */
struct Scale
{
    std::string name; // Is stored as all lower case when read in via YAML.
    std::size_t tuning_length;
    std::vector<std::uint8_t> intervals; // 0 is implied
    std::uint8_t mode;                   // [1, intervals.size]

    [[nodiscard]] auto operator==(Scale const &other) const -> bool;
    [[nodiscard]] auto operator!=(Scale const &other) const -> bool;
};

/**
 * Loads in Scales from library directory's scales.yml and user_scales.yml files.
 */
[[nodiscard]] auto load_scales_from_files() -> std::vector<Scale>;

/**
 * Direction a Note should be shifted when mapping to a scale if equally spaced from
 * valid pitches.
 */
enum class TranslateDirection
{
    Up,
    Down,
};

[[nodiscard]] auto generate_valid_pitches(xen::Scale const &scale) -> std::vector<int>;

/**
 * Map an input pitch to a list of valid pitches, with wrapping at octaves. Maps to the
 * nearest neighbor pitch that is valid, or unchanged if already valid.
 *
 * @param direction The tiebreak direction if nearest neightbor is a tie.
 */
[[nodiscard]] auto map_pitch_to_scale(int pitch, std::vector<int> const &valid_pitches,
                                      std::size_t tuning_length,
                                      TranslateDirection direction) -> int;

} // namespace xen