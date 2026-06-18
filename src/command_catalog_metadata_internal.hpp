#pragma once

#include <functional>
#include <string>
#include <vector>

#include <xen/command.hpp>
#include <xen/command_catalog_types.hpp>

namespace xen
{

using CommandSpec = CommandDefinition;

[[nodiscard]] auto format_metadata_argument(CatalogArgumentMetadata const &argument)
    -> std::string;

[[nodiscard]] auto format_metadata_path(CatalogCommandMetadata const &metadata)
    -> std::string;

} // namespace xen
