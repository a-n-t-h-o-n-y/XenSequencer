#pragma once
#include <xen/command.hpp>

namespace xen
{

using XenCommandTree = CommandGroup;

[[nodiscard]] auto create_engine_command_tree() -> XenCommandTree;

[[nodiscard]] inline auto create_command_tree() -> XenCommandTree
{
    return create_engine_command_tree();
}

} // namespace xen
