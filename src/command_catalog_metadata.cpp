#include <xen/command_catalog.hpp>

#include <string>
#include <vector>

#include "command_catalog_metadata_internal.hpp"

namespace xen
{

auto format_metadata_argument(CatalogArgumentMetadata const &argument) -> std::string
{
    auto result = std::string{"["};
    result += argument.type + ": " + argument.name;
    if (argument.default_value.has_value())
    {
        result += "=" + *argument.default_value;
    }
    result += "]";
    return result;
}

auto format_metadata_path(CatalogCommandMetadata const &metadata) -> std::string
{
    auto path = std::string{};
    auto separator = std::string{};
    for (auto const &token : metadata.path)
    {
        path += separator;
        path += token;
        separator = " ";
    }
    return path;
}

auto CommandCatalog::metadata() const -> std::vector<CatalogCommandMetadata> const &
{
    return metadata_;
}

auto command_metadata() -> std::vector<CatalogCommandMetadata> const &
{
    return default_command_catalog().metadata();
}

} // namespace xen
