#include <xen/command_catalog.hpp>

#include <string>
#include <utility>
#include <vector>

#include <xen/string_manip.hpp>

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

    auto const *spec = static_cast<CommandDefinition const *>(nullptr);
    auto best_length = std::size_t{0};
    for (auto const &candidate : definitions_)
    {
        auto const &path = candidate.metadata.path;
        if (path.empty() || path.size() > invocation.input.words.size())
        {
            continue;
        }

        auto matches = true;
        for (auto i = std::size_t{0}; i < path.size(); ++i)
        {
            if (to_lower(path[i]) != to_lower(invocation.input.words[i]))
            {
                matches = false;
                break;
            }
        }

        if (matches && path.size() > best_length)
        {
            spec = &candidate;
            best_length = path.size();
        }
    }

    if (spec == nullptr)
    {
        return to_unknown_command_error(invocation);
    }

    try
    {
        return spec->bind(invocation, spec->metadata.path.size());
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

auto CommandCatalog::bind_chain(std::vector<CommandInvocation> const &invocations) const
    -> BindChainResult
{
    auto bound = std::vector<BoundStep>{};
    bound.reserve(invocations.size());

    for (auto const &invocation : invocations)
    {
        auto const result = bind_invocation(invocation);
        if (std::holds_alternative<CatalogBindError>(result))
        {
            return std::get<CatalogBindError>(result);
        }
        bound.push_back(std::get<BoundStep>(result));
    }

    return bound;
}

} // namespace xen
