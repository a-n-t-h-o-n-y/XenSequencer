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
    std::vector<std::uint8_t> intervals;

    [[nodiscard]] auto operator<=>(Scale const &) const = default;
};

/**
 * Loads in Scales from library directory's scales.yml and user_scales.yml files.
 */
[[nodiscard]] auto load_scales_from_files() -> std::vector<Scale>;

} // namespace xen

namespace YAML
{
template <>
struct convert<::xen::Scale>
{
    static auto decode(Node const &node, ::xen::Scale &scale) -> bool;
};
} // namespace YAML