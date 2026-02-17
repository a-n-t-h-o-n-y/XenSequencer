#pragma once

#include <optional>

#include <xen/command.hpp>
#include <xen/command_action.hpp>

namespace xen
{

[[nodiscard]] auto try_to_command_action(CommandInvocation const &invocation)
    -> std::optional<CommandAction>;

} // namespace xen
