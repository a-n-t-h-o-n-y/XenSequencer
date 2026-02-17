#include <xen/command_catalog.hpp>

#include <vector>

#include "command_catalog_metadata_internal.hpp"

namespace xen
{

auto CommandCatalog::generate_docs() const -> std::vector<Documentation>
{
    auto docs = std::vector<Documentation>{};
    docs.reserve(metadata().size());

    for (auto const &command : metadata())
    {
        auto display = SignatureDisplay{};
        display.id = format_metadata_path(command);
        display.pattern_arg = command.accepts_pattern_prefix;

        for (auto const &argument : command.arguments)
        {
            display.arguments.push_back(format_metadata_argument(argument));
        }

        docs.push_back(Documentation{
            .signature = std::move(display),
            .description = command.description,
        });
    }

    return docs;
}

auto catalog_docs() -> std::vector<Documentation>
{
    return default_command_catalog().generate_docs();
}

} // namespace xen
