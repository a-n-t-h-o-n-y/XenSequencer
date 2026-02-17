#pragma once

#include <vector>

#include "command_catalog_metadata_internal.hpp"

namespace xen::catalog_detail
{

void append_bootstrap_specs(std::vector<CommandSpec> &specs);
void append_edit_specs(std::vector<CommandSpec> &specs);
void append_set_and_shift_specs(std::vector<CommandSpec> &specs);
void append_transform_specs(std::vector<CommandSpec> &specs);

} // namespace xen::catalog_detail
