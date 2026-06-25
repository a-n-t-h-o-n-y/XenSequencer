#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include <xen/command.hpp>
#include <xen/command_catalog_types.hpp>
#include <xen/keymap.hpp>
#include <xen/message_level.hpp>
#include <xen/state.hpp>

namespace xen::bridge
{

inline constexpr std::string_view protocol = "xen.bridge.v1";
inline constexpr int project_schema_version = 2;
inline constexpr int library_schema_version = 1;
inline constexpr int catalog_schema_version = 2;

[[nodiscard]] auto to_string(MessageLevel level) -> std::string;

[[nodiscard]] auto make_project_snapshot(ProjectSnapshot const &snapshot)
    -> nlohmann::json;

[[nodiscard]] auto make_instance_binding(InstanceBinding const &binding)
    -> nlohmann::json;

[[nodiscard]] auto make_catalog_payload(
    std::vector<CatalogCommandMetadata> const &commands) -> nlohmann::json;

[[nodiscard]] auto make_keymap_payload(KeymapSnapshot const &snapshot)
    -> nlohmann::json;

} // namespace xen::bridge
