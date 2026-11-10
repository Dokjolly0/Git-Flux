#include "core/modules/git/bl/git_conflict_service.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

namespace gf::git::bl
{
    GitConflictService::GitConflictService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    std::expected<void, ports::GitOpsError>
    GitConflictService::validateRepoPath(const std::filesystem::path& repoPath) const
    {
        if (repoPath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del repository non può essere vuoto",
                .command = "conflict"
            });
        }

        std::error_code ec;
        if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::PathNotFound,
                .exitCode = -1,
                .message = "La directory del repository non esiste: " + repoPath.string(),
                .command = "conflict"
            });
        }

        return {};
    }

    std::expected<bo::ConflictFile, ports::GitOpsError>
    GitConflictService::getConflictFile(const std::filesystem::path& repoPath,
                                        const std::string& filePath)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (filePath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il percorso del file in conflitto non può essere vuoto",
                .command = "conflict"
            });
        }

        return m_gitOps.getConflictFile(repoPath, filePath);
    }

    std::expected<std::string, ports::GitOpsError>
    GitConflictService::getFileStageContent(const std::filesystem::path& repoPath,
                                            const std::string& filePath,
                                            int stage)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (filePath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il percorso del file non può essere vuoto",
                .command = "show"
            });
        }

        if (stage < 1 || stage > 3)
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Lo stage deve essere 1 (base), 2 (ours) o 3 (theirs)",
                .command = "show"
            });
        }

        return m_gitOps.getFileStageContent(repoPath, filePath, stage);
    }

    std::expected<void, ports::GitOpsError>
    GitConflictService::applyConflictResolution(const std::filesystem::path& repoPath,
                                                const bo::ApplyConflictResolutionOptions& options)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (options.filePath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il percorso del file da risolvere non può essere vuoto",
                .command = "conflict-resolve"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.applyConflictResolution(repoPath, options);
    }
} // namespace gf::git::bl
