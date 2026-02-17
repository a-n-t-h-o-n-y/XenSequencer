#pragma once

#include <string>
#include <vector>

#include <xen/command.hpp>
#include <xen/command_catalog_types.hpp>

namespace xen
{

class CommandCatalog
{
  public:
    [[nodiscard]] auto metadata() const
        -> std::vector<CatalogCommandMetadata> const &;

    [[nodiscard]] auto bind_invocation(CommandInvocation const &invocation) const
        -> BindInvocationResult;

    [[nodiscard]] auto bind_chain(
        std::vector<CommandInvocation> const &invocations) const -> BindChainResult;

    [[nodiscard]] auto complete_text(std::string const &partial_command) const
        -> std::string;

    [[nodiscard]] auto complete_id(std::string const &partial_command) const
        -> std::string;

    [[nodiscard]] auto generate_docs() const -> std::vector<Documentation>;
};

[[nodiscard]] auto create_command_catalog() -> CommandCatalog;

[[nodiscard]] auto default_command_catalog() -> CommandCatalog const &;

[[nodiscard]] auto bind_invocation(CommandInvocation const &invocation)
    -> BindInvocationResult;

[[nodiscard]] auto bind_chain(std::vector<CommandInvocation> const &invocations)
    -> BindChainResult;

[[nodiscard]] auto command_metadata()
    -> std::vector<CatalogCommandMetadata> const &;

[[nodiscard]] auto catalog_complete_text(std::string const &partial_command)
    -> std::string;

[[nodiscard]] auto catalog_complete_id(std::string const &partial_command)
    -> std::string;

[[nodiscard]] auto catalog_docs() -> std::vector<Documentation>;

} // namespace xen
