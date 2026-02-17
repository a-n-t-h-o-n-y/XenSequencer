#pragma once

#include <functional>
#include <string>
#include <vector>

#include <xen/command.hpp>
#include <xen/command_action.hpp>
#include <xen/command_catalog_types.hpp>

namespace xen
{

class CatalogBindException final : public std::exception
{
  public:
    CatalogBindException(CatalogBindErrorKind kind, std::string argument,
                         std::string detail);

    [[nodiscard]] auto kind() const noexcept -> CatalogBindErrorKind;
    [[nodiscard]] auto argument() const -> std::string const &;
    [[nodiscard]] auto detail() const -> std::string const &;
    [[nodiscard]] auto what() const noexcept -> char const * override;

  private:
    CatalogBindErrorKind kind_;
    std::string argument_;
    std::string detail_;
    std::string message_;
};

struct CommandSpec
{
    CatalogCommandMetadata metadata{};
    std::function<CommandAction(CommandInvocation const &, std::size_t)> bind{};
};

[[nodiscard]] auto command_specs_storage() -> std::vector<CommandSpec> const &;

[[nodiscard]] auto find_command_spec(CommandInvocation const &invocation)
    -> CommandSpec const *;

[[nodiscard]] auto command_catalog_metadata_storage()
    -> std::vector<CatalogCommandMetadata> const &;

[[nodiscard]] auto format_metadata_argument(
    CatalogArgumentMetadata const &argument) -> std::string;

[[nodiscard]] auto format_metadata_path(CatalogCommandMetadata const &metadata)
    -> std::string;

} // namespace xen
