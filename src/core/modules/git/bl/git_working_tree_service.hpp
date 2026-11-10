#pragma once

#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace gf::git::bl
{
    class GitWorkingTreeService
    {
    public:
        explicit GitWorkingTreeService(ports::IRepositoryGitOps& gitOps) noexcept;

        [[nodiscard]] std::expected<bo::RepoWorkingTreeStatus, ports::GitOpsError>
        getWorkingTreeStatus(const std::filesystem::path& repoPath);

        [[nodiscard]] std::expected<bool, ports::GitOpsError>
        isRepositoryClean(const std::filesystem::path& repoPath);

        [[nodiscard]] std::expected<bo::GitBranchHead, ports::GitOpsError>
        getCurrentBranch(const std::filesystem::path& repoPath);

    private:
        ports::IRepositoryGitOps& m_gitOps;
    };
} // namespace gf::git::bl
