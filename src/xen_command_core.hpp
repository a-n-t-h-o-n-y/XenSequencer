#pragma once

#include <optional>
#include <utility>
#include <variant>

#include <sequence/sequence.hpp>

#include "command_core.hpp"
#include "xen_timeline.hpp"

namespace xen
{

/**
 * @brief The CommandCore with all Xen-specific commands.
 */
class XenCommandCore : public CommandCore
{
  public:
    explicit XenCommandCore(XenTimeline &t, std::optional<sequence::Cell> &copy_buffer);

  private:
    std::optional<sequence::Cell> &copy_buffer_;
};

} // namespace xen