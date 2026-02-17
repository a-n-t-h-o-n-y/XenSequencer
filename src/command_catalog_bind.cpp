#include <xen/command_catalog.hpp>

#include <string>
#include <utility>
#include <vector>

#include "command_catalog_metadata_internal.hpp"

namespace xen
{

namespace
{

auto to_unknown_command_error(CommandInvocation const &invocation) -> CatalogBindError
{
    auto token = std::string{};
    if (!invocation.input.words.empty())
    {
        token = invocation.input.words.front();
    }

    auto message = std::string{"Command not found"};
    if (!token.empty())
    {
        message += ": " + token;
    }

    return CatalogBindError{
        .kind = CatalogBindErrorKind::UnknownCommand,
        .message = std::move(message),
        .token = std::move(token),
    };
}

} // namespace

auto CommandCatalog::bind_invocation(CommandInvocation const &invocation) const
    -> BindInvocationResult
{
    if (invocation.input.words.empty())
    {
        return to_unknown_command_error(invocation);
    }

    auto const *spec = find_command_spec(invocation);
    if (spec == nullptr)
    {
        return to_unknown_command_error(invocation);
    }

    try
    {
        return BoundCommand{
            .action = spec->bind(invocation, spec->metadata.path.size()),
            .canonical = invocation.canonical_segment,
        };
    }
    catch (CatalogBindException const &e)
    {
        auto token = std::string{};
        if (!invocation.input.words.empty())
        {
            token = invocation.input.words.front();
        }

        return CatalogBindError{
            .kind = e.kind(),
            .message = e.what(),
            .token = std::move(token),
        };
    }
}

auto CommandCatalog::bind_chain(
    std::vector<CommandInvocation> const &invocations) const -> BindChainResult
{
    auto bound = std::vector<BoundCommand>{};
    bound.reserve(invocations.size());

    for (auto const &invocation : invocations)
    {
        auto const result = bind_invocation(invocation);
        if (std::holds_alternative<CatalogBindError>(result))
        {
            return std::get<CatalogBindError>(result);
        }
        bound.push_back(std::get<BoundCommand>(result));
    }

    return bound;
}

} // namespace xen
