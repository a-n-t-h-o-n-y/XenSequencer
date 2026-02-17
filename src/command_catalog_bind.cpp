#include <xen/command_catalog.hpp>
#include "command_catalog_parse.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

auto to_parse_error(CommandInvocation const &invocation, std::string const &reason)
    -> CatalogBindError
{
    auto token = std::string{};
    if (!invocation.input.words.empty())
    {
        token = invocation.input.words.front();
    }

    if (reason == "Missing argument and no default value")
    {
        return CatalogBindError{
            .kind = CatalogBindErrorKind::MissingArgument,
            .message = "Missing argument: argument",
            .token = std::move(token),
        };
    }

    return CatalogBindError{
        .kind = CatalogBindErrorKind::InvalidArgument,
        .message = "Invalid argument 'argument': " + reason,
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

    try
    {
        if (auto action = try_to_command_action(invocation))
        {
            return BoundCommand{
                .action = std::move(*action),
                .canonical = invocation.canonical_segment,
            };
        }
    }
    catch (std::invalid_argument const &e)
    {
        return to_parse_error(invocation, e.what());
    }

    return to_unknown_command_error(invocation);
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
