#include <xen/scale.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace xen
{

void to_json(nlohmann::json &j, Scale const &scale)
{
    j = nlohmann::json{{"name", scale.name},
                       {"intended_tuning_length", scale.intended_tuning_length},
                       {"intervals", scale.intervals}};
}

void from_json(nlohmann::json const &j, Scale &scale)
{
    j.at("name").get_to(scale.name);
    j.at("intended_tuning_length").get_to(scale.intended_tuning_length);
    j.at("intervals").get_to(scale.intervals);
}

} // namespace xen