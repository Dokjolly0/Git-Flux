#pragma once

#include "core/modules/git/bo/rebase_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>

namespace gf::git::bl {

class GitRebaseService {
public:
    explicit GitRebaseService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitRebaseService() = default;

    /// Starts a rebase session under per-repo lock.
    [[nodiscard]] std::expected<bo::RebaseResult, ports::GitOpsError>
    startRebase(const std::filesystem::path& repoPath, const bo::RebaseOptions& options);

    /// Continues an in-progress rebase after resolving conflicts under per-repo lock.
    [[nodiscard]] std::expected<bo::RebaseResult, ports::GitOpsError>
    continueRebase(const std::filesystem::path& repoPath);

    /// Skips the current commit during a rebase under per-repo lock.
    [[nodiscard]] std::expected<bo::RebaseResult, ports::GitOpsError>
    skipRebase(const std::filesystem::path& repoPath);

    /// Aborts an in-progress rebase restoring the previous state under per-repo lock.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    abortRebase(const std::filesystem::path& repoPath);

    /// Checks if a rebase session is currently in progress.
    [[nodiscard]] std::expected<bool, ports::GitOpsError>
    isRebaseInProgress(const std::filesystem::path& repoPath);

    /// Executes cherry-pick of a specified commit under per-repo lock.
    [[nodiscard]] std::expected<bo::CherryPickResult, ports::GitOpsError>
    cherryPick(const std::filesystem::path& repoPath, const bo::CherryPickOptions& options);

    /// Aborts an in-progress cherry-pick under per-repo lock.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    abortCherryPick(const std::filesystem::path& repoPath);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
