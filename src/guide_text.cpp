#include <xen/guide_text.hpp>

#include <cctype>
#include <sstream>
#include <string>
#include <tuple>

#include <sequence/pattern.hpp>

#include <xen/command.hpp>
#include <xen/signature.hpp>
#include <xen/string_manip.hpp>
#include <xen/utility.hpp>
#include <xen/xen_command_tree.hpp>

namespace xen
{

auto generate_guide_text(XenCommandTree const &command_tree,
                         std::string const &partial_command) -> std::string
{
    if (strip(partial_command).empty())
    {
        return "";
    }
    auto const split = split_input(partial_command);
    return command_tree.complete_text(split);
}

auto complete_id(XenCommandTree const &command_tree, std::string const &partial_command)
    -> std::string
{
    // This could have a proper implementation, but this is enough for now.
    auto const potential =
        get_first_word(generate_guide_text(command_tree, partial_command));
    if (!potential.empty() && potential.front() == '[')
    {
        return "";
    }
    else
    {
        return potential;
    }
}

} // namespace xen