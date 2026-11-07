#pragma once

#include "core/modules/groups/bo/group_id.hpp"
#include "core/modules/repositories/bo/repository_id.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace gf::groups::bo {

struct Group {
    GroupId id;
    std::string name;
    std::vector<repositories::bo::RepositoryId> repositoryIds; // ordered list
    std::int64_t createdAtUnixEpochMs = 0;
    std::int64_t updatedAtUnixEpochMs = 0;

    [[nodiscard]] bool isValid() const noexcept {
        return !id.empty() && !name.empty();
    }

    [[nodiscard]] bool contains(const repositories::bo::RepositoryId& repoId) const noexcept {
        return std::ranges::find(repositoryIds, repoId) != repositoryIds.end();
    }

    auto operator<=>(const Group&) const = default;
};

} // namespace gf::groups::bo
