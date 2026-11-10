#include "core/modules/git/bl/git_working_tree_service.hpp"

#include <system_error>

namespace gf::git::bl
{
    GitWorkingTreeService::GitWorkingTreeService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    std::expected<bo::RepoWorkingTreeStatus, ports::GitOpsError>
    GitWorkingTreeService::getWorkingTreeStatus(const std::filesystem::path& repoPath)
    {
        if (repoPath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del repository non può essere vuoto",
                .command = {}
            });
        }

        std::error_code ec;
        if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::PathNotFound,
                .exitCode = -1,
                .message = "La directory del repository non esiste su filesystem: " + repoPath.string(),
                .command = {}
            });
        }

        return m_gitOps.getWorkingTreeStatus(repoPath);
    }

    std::expected<bool, ports::GitOpsError>
    GitWorkingTreeService::isRepositoryClean(const std::filesystem::path& repoPath)
    {
        auto statusResult = getWorkingTreeStatus(repoPath);
        if (!statusResult)
        {
            return std::unexpected(statusResult.error());
        }
        return statusResult->isClean();
    }

    std::expected<bo::GitBranchHead, ports::GitOpsError>
    GitWorkingTreeService::getCurrentBranch(const std::filesystem::path& repoPath)
    {
        auto statusResult = getWorkingTreeStatus(repoPath);
        if (!statusResult)
        {
            return std::unexpected(statusResult.error());
        }
        return statusResult->branch;
    }
} // namespace gf::git::bl
