#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace xen
{

/**
 * Struct to hold musical scale data.
 */
struct Scale
{
    std::string name;
    std::size_t intended_tuning_length;
    std::vector<std::uint8_t> intervals;

    [[nodiscard]] auto operator<=>(Scale const &) const = default;
};

/**
 * Serialize Scale to JSON.
 */
void to_json(nlohmann::json &j, Scale const &scale);

/**
 * Deserialize Scale from JSON.
 */
void from_json(nlohmann::json const &j, Scale &scale);

} // namespace xen