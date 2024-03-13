#pragma once

#include <xen/state.hpp>
#include <xen/timeline.hpp>

namespace xen
{

/**
 * The specific Timeline type for the Xen plugin.
 */
using XenTimeline = Timeline<State, AuxState>;

} // namespace xen