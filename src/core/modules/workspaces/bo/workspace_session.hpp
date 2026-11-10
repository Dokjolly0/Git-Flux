#pragma once

#include "core/modules/repositories/bo/repository_id.hpp"
#include "core/modules/workspaces/bo/workspace_id.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gf::workspaces::bo
{
    struct PanelLayoutState
    {
        bool leftSidebarCollapsed = false;
        bool rightSidebarCollapsed = false;
        double leftSidebarWidth = 260.0;
        double rightSidebarWidth = 360.0;
        std::string activeView = "graph"; // "graph", "changes", "branches", "history"

        auto operator<=>(const PanelLayoutState&) const = default;
    };

    struct WorkspaceSession
    {
        WorkspaceId workspaceId;
        int schemaVersion = 1;
        std::vector<repositories::bo::RepositoryId> openedRepositoryIds; // open tabs
        std::optional<repositories::bo::RepositoryId> activeRepositoryId;
        PanelLayoutState layout;
        std::string customStateJson; // opaque JSON payload for filters and auxiliary state
        std::int64_t updatedAtUnixEpochMs = 0;

        [[nodiscard]] bool isValid() const noexcept
        {
            return !workspaceId.empty() && schemaVersion > 0;
        }

        auto operator<=>(const WorkspaceSession&) const = default;
    };
} // namespace gf::workspaces::bo
