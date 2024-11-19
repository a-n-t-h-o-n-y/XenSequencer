#pragma once
#include <xen/command.hpp>

namespace xen
{

using XenCommandTree = CommandGroup;

[[nodiscard]] auto create_command_tree() -> XenCommandTree;

} // namespace xen
