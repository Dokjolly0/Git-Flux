#include "core/modules/git/bl/git_diff_service.hpp"
#include "core/modules/git/bl/git_validation.hpp"

namespace gf::git::bl
{
    GitDiffService::GitDiffService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    std::expected<bo::FileDiff, ports::GitOpsError>
    GitDiffService::getFileDiff(const std::filesystem::path& repoPath,
                                const std::string& filePath,
                                const bo::DiffScope scope) const
    {
        if (auto validRepo = validateRepoPath(repoPath, "diff"); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath, "diff"); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getFileDiff(repoPath, filePath, scope);
    }

    std::expected<bo::FileDiff, ports::GitOpsError>
    GitDiffService::getFileDiff(const std::filesystem::path& repoPath,
                                const std::string& filePath,
                                const bo::DiffScope scope,
                                const int contextLines) const
    {
        if (auto validRepo = validateRepoPath(repoPath, "diff"); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath, "diff"); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getFileDiffWithContext(repoPath, filePath, scope, contextLines);
    }

    std::expected<std::vector<bo::FileDiff>, ports::GitOpsError>
    GitDiffService::getAllDiffs(const std::filesystem::path& repoPath,
                                const bo::DiffScope scope) const
    {
        if (auto validRepo = validateRepoPath(repoPath, "diff"); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        return m_gitOps.getAllDiffs(repoPath, scope);
    }

    std::expected<bo::FileDiff, ports::GitOpsError>
    GitDiffService::getCommitFileDiff(const std::filesystem::path& repoPath,
                                      const std::string& commitSha,
                                      const std::string& filePath) const
    {
        if (auto validRepo = validateRepoPath(repoPath, "diff"); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath, "diff"); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getCommitFileDiff(repoPath, commitSha, filePath);
    }

    std::expected<bo::FileDiff, ports::GitOpsError>
    GitDiffService::getCommitFileDiff(const std::filesystem::path& repoPath,
                                      const std::string& commitSha,
                                      const std::string& filePath,
                                      const int contextLines) const
    {
        if (auto validRepo = validateRepoPath(repoPath, "diff"); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath, "diff"); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getCommitFileDiffWithContext(repoPath, commitSha, filePath, contextLines);
    }

    std::expected<bo::FileDiff, ports::GitOpsError>
    GitDiffService::getCommitRangeFileDiff(const std::filesystem::path& repoPath,
                                           const std::string& baseRevision,
                                           const std::string& headSha,
                                           const std::string& filePath,
                                           const int contextLines) const
    {
        if (auto validRepo = validateRepoPath(repoPath, "diff"); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath, "diff"); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        if (baseRevision.empty() || headSha.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Revisioni di intervallo non valide per il diff",
                .command = "diff"
            });
        }

        return m_gitOps.getCommitRangeFileDiffWithContext(repoPath, baseRevision, headSha, filePath, contextLines);
    }

    std::expected<std::string, ports::GitOpsError>
    GitDiffService::getFileContent(const std::filesystem::path& repoPath,
                                   const std::string& revision,
                                   const std::string& filePath) const
    {
        if (auto validRepo = validateRepoPath(repoPath, "diff"); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath, "diff"); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getFileContent(repoPath, revision, filePath);
    }

    std::expected<bo::FileBlob, ports::GitOpsError>
    GitDiffService::getFileBlob(const std::filesystem::path& repoPath,
                                const std::string& revision,
                                const std::string& filePath,
                                const std::size_t maxBytes) const
    {
        if (auto validRepo = validateRepoPath(repoPath, "diff"); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath, "diff"); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getFileBlob(repoPath, revision, filePath, maxBytes);
    }

    std::expected<std::vector<bo::BlameLine>, ports::GitOpsError>
    GitDiffService::getFileBlame(const std::filesystem::path& repoPath,
                                 const std::string& revision,
                                 const std::string& filePath) const
    {
        if (auto validRepo = validateRepoPath(repoPath, "diff"); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath, "diff"); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getFileBlame(repoPath, revision, filePath);
    }
} // namespace gf::git::bl
