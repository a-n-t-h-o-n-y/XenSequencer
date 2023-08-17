#pragma once

#include "state.hpp"
#include "timeline.hpp"

namespace xen
{

/**
 * @brief The specific Timeline type for the Xen plugin.
 */
using XenTimeline = Timeline<State, SelectedState>;

} // namespace xen