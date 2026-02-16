#pragma once

#include <optional>
#include <string>
#include <utility>

#include <xen/message_level.hpp>
#include <xen/runtime_state.hpp>

namespace xen
{

class RuntimeCommandTree
{
  public:
    [[nodiscard]] auto execute(RuntimeState &runtime_state,
                               std::string const &command_string) const
        -> std::optional<std::pair<MessageLevel, std::string>>;

    [[nodiscard]] auto guide_text(std::string const &partial_command) const
        -> std::string;

    [[nodiscard]] auto complete_id(std::string const &partial_command) const
        -> std::string;
};

} // namespace xen
