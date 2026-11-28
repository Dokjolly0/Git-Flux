#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/git/bl/git_validation.hpp"

namespace gf::git::bl
{
    GitWorkingTreeService::GitWorkingTreeService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    std::expected<bo::RepoWorkingTreeStatus, ports::GitOpsError>
    GitWorkingTreeService::getWorkingTreeStatus(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        return m_gitOps.getWorkingTreeStatus(repoPath);
    }

    std::expected<bool, ports::GitOpsError>
    GitWorkingTreeService::isRepositoryClean(const std::filesystem::path& repoPath) const
    {
        auto statusResult = getWorkingTreeStatus(repoPath);
        if (!statusResult)
        {
            return std::unexpected(statusResult.error());
        }
        return statusResult->isClean();
    }

    std::expected<bo::GitBranchHead, ports::GitOpsError>
    GitWorkingTreeService::getCurrentBranch(const std::filesystem::path& repoPath) const
    {
        auto statusResult = getWorkingTreeStatus(repoPath);
        if (!statusResult)
        {
            return std::unexpected(statusResult.error());
        }
        return statusResult->branch;
    }
} // namespace gf::git::bl
