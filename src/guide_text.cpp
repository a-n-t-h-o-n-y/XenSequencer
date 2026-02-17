#include <xen/guide_text.hpp>

#include <string>

#include <xen/command_catalog.hpp>

namespace xen
{

auto generate_guide_text(std::string const &partial_command) -> std::string
{
    return catalog_complete_text(partial_command);
}

auto complete_id(std::string const &partial_command) -> std::string
{
    return catalog_complete_id(partial_command);
}

} // namespace xen
