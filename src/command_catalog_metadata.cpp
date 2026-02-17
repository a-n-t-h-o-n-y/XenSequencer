#include <xen/command_catalog.hpp>

#include <string>
#include <vector>

#include "command_catalog_metadata_internal.hpp"

namespace xen
{

auto command_catalog_metadata_storage() -> std::vector<CatalogCommandMetadata> const &
{
    static auto const metadata = [] {
        auto result = std::vector<CatalogCommandMetadata>{};
        auto const &specs = command_specs_storage();
        result.reserve(specs.size());
        for (auto const &spec : specs)
        {
            result.push_back(spec.metadata);
        }
        return result;
    }();

    return metadata;
}

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
    return command_catalog_metadata_storage();
}

auto command_metadata() -> std::vector<CatalogCommandMetadata> const &
{
    return default_command_catalog().metadata();
}

} // namespace xen
