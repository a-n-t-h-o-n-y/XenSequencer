#include <xen/command_catalog.hpp>

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <xen/string_manip.hpp>

#include "command_catalog_metadata_internal.hpp"
#include "command_catalog_specs_internal.hpp"

namespace xen
{
namespace
{

auto path_key(std::vector<std::string> const &path) -> std::string
{
    auto key = std::string{};
    auto separator = std::string{};
    for (auto const &token : path)
    {
        key += separator;
        key += to_lower(token);
        separator = " ";
    }
    return key;
}

auto build_specs() -> std::vector<CommandSpec>
{
    auto specs = std::vector<CommandSpec>{};
    catalog_detail::append_bootstrap_specs(specs);
    catalog_detail::append_edit_specs(specs);
    catalog_detail::append_set_and_shift_specs(specs);
    catalog_detail::append_transform_specs(specs);

    auto seen_paths = std::unordered_set<std::string>{};
    seen_paths.reserve(specs.size());
    for (auto const &spec : specs)
    {
        auto key = path_key(spec.metadata.path);
        if (!seen_paths.insert(key).second)
        {
            throw std::runtime_error("Duplicate command path in catalog: " + key);
        }
    }

    return specs;
}

} // namespace

CatalogBindException::CatalogBindException(CatalogBindErrorKind kind,
                                           std::string argument,
                                           std::string detail)
    : kind_{kind}
    , argument_{std::move(argument)}
    , detail_{std::move(detail)}
{
    if (kind_ == CatalogBindErrorKind::MissingArgument)
    {
        message_ = "Missing argument: " + argument_;
    }
    else if (kind_ == CatalogBindErrorKind::InvalidArgument)
    {
        message_ = "Invalid argument '" + argument_ + "'";
        if (!detail_.empty())
        {
            message_ += ": " + detail_;
        }
    }
    else
    {
        message_ = "Catalog bind error";
    }
}

auto CatalogBindException::kind() const noexcept -> CatalogBindErrorKind
{
    return kind_;
}

auto CatalogBindException::argument() const -> std::string const &
{
    return argument_;
}

auto CatalogBindException::detail() const -> std::string const &
{
    return detail_;
}

auto CatalogBindException::what() const noexcept -> char const *
{
    return message_.c_str();
}

auto command_specs_storage() -> std::vector<CommandSpec> const &
{
    static auto const specs = build_specs();
    return specs;
}

auto find_command_spec(CommandInvocation const &invocation) -> CommandSpec const *
{
    auto const &words = invocation.input.words;
    if (words.empty())
    {
        return nullptr;
    }

    auto const &specs = command_specs_storage();
    auto best = static_cast<CommandSpec const *>(nullptr);
    auto best_len = std::size_t{0};

    for (auto const &spec : specs)
    {
        auto const &path = spec.metadata.path;
        if (path.empty() || path.size() > words.size())
        {
            continue;
        }

        auto matched = true;
        for (auto i = std::size_t{0}; i < path.size(); ++i)
        {
            if (to_lower(words[i]) != to_lower(path[i]))
            {
                matched = false;
                break;
            }
        }

        if (!matched)
        {
            continue;
        }

        if (path.size() > best_len)
        {
            best = &spec;
            best_len = path.size();
        }
    }

    return best;
}

} // namespace xen
