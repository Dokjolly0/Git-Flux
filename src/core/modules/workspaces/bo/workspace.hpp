#pragma once

#include "core/modules/groups/bo/group_id.hpp"
#include "core/modules/repositories/bo/repository_id.hpp"
#include "core/modules/workspaces/bo/workspace_id.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace gf::workspaces::bo
{
    struct Workspace
    {
        WorkspaceId id;
        std::string name;
        std::vector<groups::bo::GroupId> groupIds; // ordered list of included groups
        std::vector<repositories::bo::RepositoryId> directRepositoryIds; // ordered list of direct repos
        std::int64_t createdAtUnixEpochMs = 0;
        std::int64_t updatedAtUnixEpochMs = 0;

        [[nodiscard]] bool isValid() const noexcept
        {
            return !id.empty() && !name.empty();
        }

        [[nodiscard]] bool containsDirectRepository(const repositories::bo::RepositoryId& repoId) const noexcept
        {
            return std::ranges::find(directRepositoryIds, repoId) != directRepositoryIds.end();
        }

        [[nodiscard]] bool containsGroup(const groups::bo::GroupId& groupId) const noexcept
        {
            return std::ranges::find(groupIds, groupId) != groupIds.end();
        }

        auto operator<=>(const Workspace&) const = default;
    };
} // namespace gf::workspaces::bo
