#pragma once

#include <map>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include <xen/command.hpp>
#include <xen/command_catalog_types.hpp>
#include <xen/message_level.hpp>
#include <xen/state.hpp>

namespace xen::bridge
{

inline constexpr std::string_view protocol = "xen.bridge.v1";
inline constexpr int snapshot_schema_version = 3;

[[nodiscard]] auto to_string(MessageLevel level) -> std::string;

[[nodiscard]] auto make_ui_state_snapshot(
    EngineSnapshot const &snapshot, ContentLibraryState const &library)
    -> nlohmann::json;

[[nodiscard]] auto make_catalog_payload(
    std::vector<CatalogCommandMetadata> const &commands) -> nlohmann::json;

[[nodiscard]] auto make_keymap_payload(
    std::map<std::string, std::map<std::string, std::string>> const &keymap)
    -> nlohmann::json;

[[nodiscard]] auto make_reference_payload(
    std::vector<Documentation> const &docs,
    std::map<std::string, std::map<std::string, std::string>> const &keymap)
    -> nlohmann::json;

} // namespace xen::bridge
