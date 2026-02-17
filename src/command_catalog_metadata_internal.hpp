#pragma once

#include <string>
#include <vector>

#include <xen/command_catalog_types.hpp>

namespace xen
{

[[nodiscard]] auto command_catalog_metadata_storage()
    -> std::vector<CatalogCommandMetadata> const &;

[[nodiscard]] auto format_metadata_argument(
    CatalogArgumentMetadata const &argument) -> std::string;

[[nodiscard]] auto format_metadata_path(CatalogCommandMetadata const &metadata)
    -> std::string;

} // namespace xen
