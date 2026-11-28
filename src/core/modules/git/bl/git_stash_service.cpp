#include "core/modules/git/bl/git_stash_service.hpp"
#include "core/modules/git/bl/git_validation.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

namespace gf::git::bl
{
    GitStashService::GitStashService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    std::expected<std::vector<bo::GitStashItem>, ports::GitOpsError>
    GitStashService::listStashes(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "stash"); !valid)
        {
            return std::unexpected(valid.error());
        }

        return m_gitOps.listStashes(repoPath);
    }

    std::expected<void, ports::GitOpsError>
    GitStashService::stashPush(const std::filesystem::path& repoPath,
                               const bo::StashPushOptions& options) const
    {
        if (auto valid = validateRepoPath(repoPath, "stash"); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.stashPush(repoPath, options);
    }

    std::expected<void, ports::GitOpsError>
    GitStashService::stashPop(const std::filesystem::path& repoPath,
                              const bo::StashApplyOptions& options) const
    {
        if (auto valid = validateRepoPath(repoPath, "stash"); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.stashPop(repoPath, options);
    }

    std::expected<void, ports::GitOpsError>
    GitStashService::stashApply(const std::filesystem::path& repoPath,
                                const bo::StashApplyOptions& options) const
    {
        if (auto valid = validateRepoPath(repoPath, "stash"); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.stashApply(repoPath, options);
    }

    std::expected<void, ports::GitOpsError>
    GitStashService::stashDrop(const std::filesystem::path& repoPath,
                               const std::size_t stashIndex) const
    {
        if (auto valid = validateRepoPath(repoPath, "stash"); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.stashDrop(repoPath, stashIndex);
    }

    std::expected<void, ports::GitOpsError>
    GitStashService::stashClear(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "stash"); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.stashClear(repoPath);
    }
} // namespace gf::git::bl
