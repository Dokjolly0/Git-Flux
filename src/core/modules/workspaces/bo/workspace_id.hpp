#pragma once

#include "core/shared/strong_id.hpp"

namespace gf::workspaces::bo {

struct WorkspaceIdTag {};
using WorkspaceId = gf::core::StrongId<WorkspaceIdTag>;

} // namespace gf::workspaces::bo
