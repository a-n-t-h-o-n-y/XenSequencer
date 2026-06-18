#include <xen/command_catalog.hpp>

#include <stdexcept>
#include <unordered_set>

#include <xen/string_manip.hpp>

namespace xen
{
void append_default_command_definitions(CommandCatalog &catalog);

namespace
{

auto path_key(std::vector<std::string> const &path) -> std::string
{
    auto result = std::string{};
    auto separator = std::string{};
    for (auto const &token : path)
    {
        result += separator;
        result += to_lower(token);
        separator = " ";
    }
    return result;
}

} // namespace

void CommandCatalog::add(CommandDefinition definition)
{
    if (definition.metadata.path.empty())
    {
        throw std::invalid_argument("Command path must not be empty.");
    }
    if (!definition.bind)
    {
        throw std::invalid_argument("Command definition must have a binder.");
    }

    for (auto const &token : definition.metadata.path)
    {
        if (token.empty())
        {
            throw std::invalid_argument("Command path tokens must not be empty.");
        }
    }

    auto optional_argument_seen = false;
    auto argument_names = std::unordered_set<std::string>{};
    for (auto const &argument : definition.metadata.arguments)
    {
        if (argument.name.empty())
        {
            throw std::invalid_argument("Command argument names must not be empty.");
        }
        if (!argument_names.insert(argument.name).second)
        {
            throw std::invalid_argument("Duplicate command argument name: " +
                                        argument.name);
        }
        if (argument.default_value.has_value())
        {
            optional_argument_seen = true;
        }
        else if (optional_argument_seen)
        {
            throw std::invalid_argument(
                "Required command arguments cannot follow optional arguments.");
        }
    }

    auto const key = path_key(definition.metadata.path);
    for (auto const &existing : definitions_)
    {
        if (path_key(existing.metadata.path) == key)
        {
            throw std::invalid_argument("Duplicate command path in catalog: " + key);
        }
    }

    auto metadata = definition.metadata;
    definitions_.push_back(std::move(definition));
    try
    {
        metadata_.push_back(std::move(metadata));
    }
    catch (...)
    {
        definitions_.pop_back();
        throw;
    }
}

auto create_command_catalog() -> CommandCatalog
{
    auto catalog = CommandCatalog{};
    append_default_command_definitions(catalog);
    return catalog;
}

auto default_command_catalog() -> CommandCatalog const &
{
    static auto const catalog = create_command_catalog();
    return catalog;
}

auto bind_invocation(CommandInvocation const &invocation) -> BindInvocationResult
{
    return default_command_catalog().bind_invocation(invocation);
}

auto bind_chain(std::vector<CommandInvocation> const &invocations) -> BindChainResult
{
    return default_command_catalog().bind_chain(invocations);
}

} // namespace xen
