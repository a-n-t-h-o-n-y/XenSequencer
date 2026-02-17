#include <xen/command_catalog.hpp>

namespace xen
{

auto create_command_catalog() -> CommandCatalog
{
    return CommandCatalog{};
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

auto bind_chain(std::vector<CommandInvocation> const &invocations)
    -> BindChainResult
{
    return default_command_catalog().bind_chain(invocations);
}

} // namespace xen

