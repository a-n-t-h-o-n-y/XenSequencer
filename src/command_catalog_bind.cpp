#include <xen/command_catalog.hpp>

#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include <xen/string_manip.hpp>

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
        return std::unexpected{to_unknown_command_error(invocation)};
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
        for (auto const &[path_token, input_token] :
             std::views::zip(path, invocation.input.words))
        {
            if (to_lower(path_token) != to_lower(input_token))
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
        return std::unexpected{to_unknown_command_error(invocation)};
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

        return std::unexpected{CatalogBindError{
            .kind = e.kind(),
            .message = e.what(),
            .token = std::move(token),
        }};
    }
}

auto CommandCatalog::bind_chain(std::vector<CommandInvocation> const &invocations) const
    -> BindChainResult
{
    auto bound = std::vector<BoundStep>{};
    bound.reserve(invocations.size());

    for (auto const &invocation : invocations)
    {
        auto result = bind_invocation(invocation);
        if (!result.has_value())
        {
            return std::unexpected{std::move(result.error())};
        }
        bound.push_back(std::move(result.value()));
    }

    return bound;
}

} // namespace xen
