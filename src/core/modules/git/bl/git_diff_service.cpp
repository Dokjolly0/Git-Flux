#include "core/modules/git/bl/git_diff_service.hpp"

namespace gf::git::bl
{
    GitDiffService::GitDiffService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    std::expected<void, ports::GitOpsError>
    GitDiffService::validateRepoPath(const std::filesystem::path& repoPath)
    {
        if (repoPath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del repository non può essere vuoto",
                .command = "diff"
            });
        }

        if (std::error_code ec; !std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::PathNotFound,
                .exitCode = -1,
                .message = "La directory del repository non esiste: " + repoPath.string(),
                .command = "diff"
            });
        }

        return {};
    }

    std::expected<void, ports::GitOpsError>
    GitDiffService::validateRelativeFilePath(const std::string& filePath)
    {
        if (filePath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del file non può essere vuoto",
                .command = "diff"
            });
        }

        std::filesystem::path p(filePath);
        if (p.is_absolute() || p.has_root_path())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del file deve essere relativo al repository: " + filePath,
                .command = "diff"
            });
        }

        for (const auto& part : p)
        {
            if (part == "..")
            {
                return std::unexpected(ports::GitOpsError{
                    .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                    .exitCode = -1,
                    .message = "Rilevato tentativo di path traversal illegale: " + filePath,
                    .command = "diff"
                });
            }
        }

        return {};
    }

    std::expected<bo::FileDiff, ports::GitOpsError>
    GitDiffService::getFileDiff(const std::filesystem::path& repoPath,
                                const std::string& filePath,
                                const bo::DiffScope scope) const
    {
        if (auto validRepo = validateRepoPath(repoPath); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath); !validPath)
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
        if (auto validRepo = validateRepoPath(repoPath); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getFileDiffWithContext(repoPath, filePath, scope, contextLines);
    }

    std::expected<std::vector<bo::FileDiff>, ports::GitOpsError>
    GitDiffService::getAllDiffs(const std::filesystem::path& repoPath,
                                const bo::DiffScope scope) const
    {
        if (auto validRepo = validateRepoPath(repoPath); !validRepo)
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
        if (auto validRepo = validateRepoPath(repoPath); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath); !validPath)
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
        if (auto validRepo = validateRepoPath(repoPath); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getCommitFileDiffWithContext(repoPath, commitSha, filePath, contextLines);
    }

    std::expected<std::string, ports::GitOpsError>
    GitDiffService::getFileContent(const std::filesystem::path& repoPath,
                                   const std::string& revision,
                                   const std::string& filePath) const
    {
        if (auto validRepo = validateRepoPath(repoPath); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath); !validPath)
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
        if (auto validRepo = validateRepoPath(repoPath); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath); !validPath)
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
        if (auto validRepo = validateRepoPath(repoPath); !validRepo)
        {
            return std::unexpected(validRepo.error());
        }

        if (auto validPath = validateRelativeFilePath(filePath); !validPath)
        {
            return std::unexpected(validPath.error());
        }

        return m_gitOps.getFileBlame(repoPath, revision, filePath);
    }
} // namespace gf::git::bl
