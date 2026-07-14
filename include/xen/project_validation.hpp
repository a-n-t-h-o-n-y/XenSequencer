#pragma once

#include <xen/state.hpp>

namespace xen
{

void validate_cell_file(sequence::Cell const &cell);

void validate(ProjectState const &project);
void validate(ContentLibrary const &library);
void validate(WorkspaceSettings const &workspace);

} // namespace xen
