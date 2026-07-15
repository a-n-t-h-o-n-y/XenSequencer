#pragma once

#include <nlohmann/json.hpp>

#include <xen/modulation.hpp>

namespace xen
{

inline constexpr int MODULATION_SCHEMA_VERSION = 3;

[[nodiscard]] auto modulation_definition_to_json(ModulationDefinition const &modulation)
    -> nlohmann::json;
[[nodiscard]] auto modulation_definition_from_json(nlohmann::json const &json)
    -> ModulationDefinition;

[[nodiscard]] auto modulation_destination_to_json(ModulationDestination destination)
    -> nlohmann::json;
[[nodiscard]] auto modulation_destination_from_json(nlohmann::json const &json)
    -> ModulationDestination;

[[nodiscard]] auto modulation_output_range_to_json(ModulationOutputRange const &range)
    -> nlohmann::json;
[[nodiscard]] auto modulation_output_range_from_json(nlohmann::json const &json)
    -> ModulationOutputRange;

[[nodiscard]] auto modulation_target_to_json(ModulationTarget const &target)
    -> nlohmann::json;
[[nodiscard]] auto modulation_target_from_json(nlohmann::json const &json)
    -> ModulationTarget;

[[nodiscard]] auto modulation_catalog_to_json() -> nlohmann::json;

} // namespace xen
