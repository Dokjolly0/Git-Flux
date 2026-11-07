#include "core/modules/git/bl/git_stash_service.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

namespace gf::git::bl {

GitStashService::GitStashService(ports::IRepositoryGitOps& gitOps) noexcept
    : m_gitOps(gitOps) {}

std::expected<void, ports::GitOpsError>
GitStashService::validateRepoPath(const std::filesystem::path& repoPath) const {
    if (repoPath.empty()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
            .exitCode = -1,
            .message = "Il percorso del repository non può essere vuoto",
            .command = "stash"
        });
    }

    std::error_code ec;
    if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec)) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::PathNotFound,
            .exitCode = -1,
            .message = "La directory del repository non esiste: " + repoPath.string(),
            .command = "stash"
        });
    }

    return {};
}

std::expected<std::vector<bo::GitStashItem>, ports::GitOpsError>
GitStashService::listStashes(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    return m_gitOps.listStashes(repoPath);
}

std::expected<void, ports::GitOpsError>
GitStashService::stashPush(const std::filesystem::path& repoPath,
                           const bo::StashPushOptions& options) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.stashPush(repoPath, options);
}

std::expected<void, ports::GitOpsError>
GitStashService::stashPop(const std::filesystem::path& repoPath,
                          const bo::StashApplyOptions& options) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.stashPop(repoPath, options);
}

std::expected<void, ports::GitOpsError>
GitStashService::stashApply(const std::filesystem::path& repoPath,
                            const bo::StashApplyOptions& options) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.stashApply(repoPath, options);
}

std::expected<void, ports::GitOpsError>
GitStashService::stashDrop(const std::filesystem::path& repoPath,
                           std::size_t stashIndex) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.stashDrop(repoPath, stashIndex);
}

std::expected<void, ports::GitOpsError>
GitStashService::stashClear(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.stashClear(repoPath);
}

} // namespace gf::git::bl
