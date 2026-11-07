#pragma once

#include "core/modules/git/bo/branch_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gf::git::bl {

class GitBranchService {
public:
    explicit GitBranchService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitBranchService() = default;

    /// Validates branch name according to git-check-ref-format rules.
    [[nodiscard]] static bool isValidBranchName(std::string_view name) noexcept;

    /// Returns the complete list of local and remote branches.
    [[nodiscard]] std::expected<std::vector<bo::GitBranch>, ports::GitOpsError>
    listBranches(const std::filesystem::path& repoPath);

    /// Returns only local branches.
    [[nodiscard]] std::expected<std::vector<bo::GitBranch>, ports::GitOpsError>
    getLocalBranches(const std::filesystem::path& repoPath);

    /// Returns only remote branches.
    [[nodiscard]] std::expected<std::vector<bo::GitBranch>, ports::GitOpsError>
    getRemoteBranches(const std::filesystem::path& repoPath);

    /// Returns the currently active branch (HEAD), or nullopt if HEAD is detached or missing.
    [[nodiscard]] std::expected<std::optional<bo::GitBranch>, ports::GitOpsError>
    getCurrentBranch(const std::filesystem::path& repoPath);

    /// Creates a new branch after validating its name.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    createBranch(const std::filesystem::path& repoPath, const bo::CreateBranchOptions& options);

    /// Checks out a branch or commit.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    checkoutBranch(const std::filesystem::path& repoPath, const bo::CheckoutOptions& options);

    /// Deletes a local branch.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    deleteBranch(const std::filesystem::path& repoPath, const std::string& branchName, bool force = false);

    /// Renames a local branch after validating the new name.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    renameBranch(const std::filesystem::path& repoPath, const std::string& oldName, const std::string& newName);

    /// Resets the current branch to a specific commit.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    resetCommit(const std::filesystem::path& repoPath, const std::string& commitSha, const std::string& mode);

    /// Reverts a specific commit on the current branch.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    revertCommit(const std::filesystem::path& repoPath, const std::string& commitSha);

    /// Creates a worktree for a commit or branch.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    createWorktree(const std::filesystem::path& repoPath, const std::filesystem::path& worktreePath, const std::string& commitOrBranch);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
