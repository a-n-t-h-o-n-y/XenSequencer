#include <xen/key_core.hpp>

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>

#include <juce_core/juce_core.h>

#include <yaml-cpp/yaml.h>

namespace
{

[[nodiscard]] auto merge_yaml_files(juce::File const &base_filepath,
                                    juce::File const &overlay_filepath) -> YAML::Node
{
    if (base_filepath.getSize() > (128 * 1'024 * 1'024))
    {
        throw std::runtime_error{"System keys file size exceeds 128MB"};
    }
    if (overlay_filepath.getSize() > (128 * 1'024 * 1'024))
    {
        throw std::runtime_error{"User keys file size exceeds 128MB"};
    }

    auto base = YAML::LoadFile(base_filepath.getFullPathName().toStdString());
    auto overlay = YAML::LoadFile(overlay_filepath.getFullPathName().toStdString());
    base.remove("version");

    if (!base.IsMap() || !overlay.IsMap())
    {
        throw std::runtime_error{"Invalid YAML file structure."};
    }

    for (auto const &overlay_pair : overlay)
    {
        auto const component_name = overlay_pair.first.as<std::string>();
        auto const &bindings_node = overlay_pair.second;
        for (auto const &key : bindings_node)
        {
            auto const key_value = key.first.as<std::string>();
            auto const value_value = key.second.as<std::string>();
            base[component_name][key_value] = value_value;
        }
    }

    return base;
}

} // namespace

namespace xen
{

auto export_merged_keymap(juce::File const &default_keys, juce::File const &user_keys)
    -> std::map<std::string, std::map<std::string, std::string>>
{
    auto const keys_node = merge_yaml_files(default_keys, user_keys);
    auto result = std::map<std::string, std::map<std::string, std::string>>{};

    for (auto const &component : keys_node)
    {
        auto const component_name = component.first.as<std::string>();
        auto const &key_mappings = component.second;

        if (!key_mappings.IsMap())
        {
            continue;
        }

        auto raw_mappings = std::map<std::string, std::string>{};
        for (auto const &mapping : key_mappings)
        {
            auto const key_combo_str = mapping.first.as<std::string>();
            auto const command = mapping.second.as<std::string>();
            raw_mappings.insert_or_assign(key_combo_str, command);
        }

        result.insert_or_assign(component_name, std::move(raw_mappings));
    }

    return result;
}

} // namespace xen
