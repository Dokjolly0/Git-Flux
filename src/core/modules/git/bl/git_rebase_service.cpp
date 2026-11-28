#include "core/modules/git/bl/git_rebase_service.hpp"
#include "core/modules/git/bl/git_validation.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

namespace gf::git::bl
{
    GitRebaseService::GitRebaseService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    std::expected<bo::RebaseResult, ports::GitOpsError>
    GitRebaseService::startRebase(const std::filesystem::path& repoPath,
                                  const bo::RebaseOptions& options) const
    {
        if (auto valid = validateRepoPath(repoPath, "rebase"); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (options.upstream.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il branch o commit upstream di destinazione non può essere vuoto",
                .command = "git rebase"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.startRebase(repoPath, options);
    }

    std::expected<bo::RebaseResult, ports::GitOpsError>
    GitRebaseService::continueRebase(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "rebase"); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.continueRebase(repoPath);
    }

    std::expected<bo::RebaseResult, ports::GitOpsError>
    GitRebaseService::skipRebase(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "rebase"); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.skipRebase(repoPath);
    }

    std::expected<void, ports::GitOpsError>
    GitRebaseService::abortRebase(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "rebase"); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.abortRebase(repoPath);
    }

    std::expected<bool, ports::GitOpsError>
    GitRebaseService::isRebaseInProgress(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "rebase"); !valid)
        {
            return std::unexpected(valid.error());
        }

        return m_gitOps.isRebaseInProgress(repoPath);
    }

    std::expected<bo::CherryPickResult, ports::GitOpsError>
    GitRebaseService::cherryPick(const std::filesystem::path& repoPath,
                                 const bo::CherryPickOptions& options) const
    {
        if (auto valid = validateRepoPath(repoPath, "rebase"); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (options.commitSha.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il commit SHA per il cherry-pick non può essere vuoto",
                .command = "git cherry-pick"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.cherryPick(repoPath, options);
    }

    std::expected<void, ports::GitOpsError>
    GitRebaseService::abortCherryPick(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "rebase"); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.abortCherryPick(repoPath);
    }
} // namespace gf::git::bl
