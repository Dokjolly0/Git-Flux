#pragma once

#include "core/modules/repositories/bo/repository_id.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace gf::repositories::bo {

enum class RepositoryPathStatus {
    Valid,
    Missing,
    NotAGitRepository
};

struct Repository {
    RepositoryId id;
    std::string displayName;
    std::filesystem::path localPath;
    RepositoryPathStatus pathStatus = RepositoryPathStatus::Valid;
    std::int64_t registeredAtUnixEpochMs = 0;
    std::int64_t lastAccessedAtUnixEpochMs = 0;

    [[nodiscard]] bool isValid() const noexcept {
        return !id.empty() && !displayName.empty() && !localPath.empty();
    }

    auto operator<=>(const Repository&) const = default;
};

} // namespace gf::repositories::bo
