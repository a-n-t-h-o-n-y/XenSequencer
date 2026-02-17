#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <xen/command_action.hpp>

namespace xen
{

enum class CatalogBindErrorKind : std::uint8_t
{
    UnknownCommand,
    MissingArgument,
    InvalidArgument,
};

struct CatalogBindError
{
    CatalogBindErrorKind kind{CatalogBindErrorKind::UnknownCommand};
    std::string message{};
    std::string token{};
};

struct BoundCommand
{
    CommandAction action{};
    std::string canonical{};
};

struct CatalogArgumentMetadata
{
    std::string type{};
    std::string name{};
    std::optional<std::string> default_value{};
};

struct CatalogCommandMetadata
{
    std::vector<std::string> path{};
    bool accepts_pattern_prefix{false};
    std::vector<CatalogArgumentMetadata> arguments{};
    std::string description{};
};

using BindInvocationResult = std::variant<BoundCommand, CatalogBindError>;
using BindChainResult = std::variant<std::vector<BoundCommand>, CatalogBindError>;

} // namespace xen
