#include "core/modules/git/bl/git_rebase_service.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

namespace gf::git::bl {

GitRebaseService::GitRebaseService(ports::IRepositoryGitOps& gitOps) noexcept
    : m_gitOps(gitOps) {}

std::expected<void, ports::GitOpsError>
GitRebaseService::validateRepoPath(const std::filesystem::path& repoPath) const {
    if (repoPath.empty()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
            .exitCode = -1,
            .message = "Il percorso del repository non può essere vuoto",
            .command = "rebase"
        });
    }

    std::error_code ec;
    if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec)) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::PathNotFound,
            .exitCode = -1,
            .message = "La directory del repository non esiste: " + repoPath.string(),
            .command = "rebase"
        });
    }

    return {};
}

std::expected<bo::RebaseResult, ports::GitOpsError>
GitRebaseService::startRebase(const std::filesystem::path& repoPath,
                              const bo::RebaseOptions& options) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    if (options.upstream.empty()) {
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
GitRebaseService::continueRebase(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.continueRebase(repoPath);
}

std::expected<bo::RebaseResult, ports::GitOpsError>
GitRebaseService::skipRebase(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.skipRebase(repoPath);
}

std::expected<void, ports::GitOpsError>
GitRebaseService::abortRebase(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.abortRebase(repoPath);
}

std::expected<bool, ports::GitOpsError>
GitRebaseService::isRebaseInProgress(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    return m_gitOps.isRebaseInProgress(repoPath);
}

std::expected<bo::CherryPickResult, ports::GitOpsError>
GitRebaseService::cherryPick(const std::filesystem::path& repoPath,
                             const bo::CherryPickOptions& options) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    if (options.commitSha.empty()) {
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
GitRebaseService::abortCherryPick(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.abortCherryPick(repoPath);
}

} // namespace gf::git::bl
