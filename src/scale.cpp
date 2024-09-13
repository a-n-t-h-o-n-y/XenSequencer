#include <xen/scale.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <xen/constants.hpp>
#include <xen/string_manip.hpp>
#include <xen/user_directory.hpp>
#include <xen/utility.hpp>

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
    return true;
}

} // namespace YAML