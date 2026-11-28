#include "core/modules/git/bl/git_history_service.hpp"
#include "core/modules/git/bl/git_validation.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

namespace gf::git::bl
{
    GitHistoryService::GitHistoryService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    bool GitHistoryService::isValidTagName(const std::string_view name) noexcept
    {
        return isValidRefName(name);
    }

    std::expected<std::vector<bo::GitCommitLogItem>, ports::GitOpsError>
    GitHistoryService::getCommitHistory(const std::filesystem::path& repoPath,
                                        const bo::CommitLogOptions& options) const
    {
        if (auto valid = validateRepoPath(repoPath, "history"); !valid)
        {
            return std::unexpected(valid.error());
        }

        return m_gitOps.getCommitLog(repoPath, options);
    }

    std::expected<std::optional<bo::GitCommitLogItem>, ports::GitOpsError>
    GitHistoryService::getCommitDetails(const std::filesystem::path& repoPath,
                                        const std::string& commitSha) const
    {
        if (auto valid = validateRepoPath(repoPath, "history"); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (commitSha.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "L'hash del commit non può essere vuoto",
                .command = "log"
            });
        }

        bo::CommitLogOptions options;
        options.revisionRange = commitSha;
        options.maxCount = 1;

        auto logResult = m_gitOps.getCommitLog(repoPath, options);
        if (!logResult)
        {
            return std::unexpected(logResult.error());
        }

        if (logResult->empty())
        {
            return std::nullopt;
        }

        return logResult->front();
    }

    std::expected<std::vector<bo::CommitFileChange>, ports::GitOpsError>
    GitHistoryService::getCommitFiles(const std::filesystem::path& repoPath,
                                      const std::string& commitSha) const
    {
        if (auto valid = validateRepoPath(repoPath, "history"); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (commitSha.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "L'hash del commit non può essere vuoto",
                .command = "diff-tree"
            });
        }

        return m_gitOps.getCommitFiles(repoPath, commitSha);
    }

    std::expected<bo::MultiCommitChangesSummary, ports::GitOpsError>
    GitHistoryService::getMultiCommitChanges(const std::filesystem::path& repoPath,
                                             const std::vector<std::string>& commitShas,
                                             const bool isContiguousRange,
                                             const std::string& baseRevision,
                                             const std::string& headSha) const
    {
        if (auto valid = validateRepoPath(repoPath, "history"); !valid)
        {
            return std::unexpected(valid.error());
        }

        std::vector<std::string> cleanShas;
        cleanShas.reserve(commitShas.size());
        for (const auto& sha : commitShas)
        {
            if (!sha.empty() && sha != "WIP")
            {
                cleanShas.push_back(sha);
            }
        }
        if (cleanShas.empty())
        {
            return bo::MultiCommitChangesSummary{};
        }

        return m_gitOps.getMultiCommitChanges(repoPath, cleanShas, isContiguousRange, baseRevision, headSha);
    }

    std::expected<std::vector<std::string>, ports::GitOpsError>
    GitHistoryService::listTrackedFiles(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "history"); !valid)
        {
            return std::unexpected(valid.error());
        }

        return m_gitOps.listTrackedFiles(repoPath);
    }

    std::expected<std::vector<bo::GitTag>, ports::GitOpsError>
    GitHistoryService::listTags(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "history"); !valid)
        {
            return std::unexpected(valid.error());
        }

        return m_gitOps.listTags(repoPath);
    }

    std::expected<void, ports::GitOpsError>
    GitHistoryService::createTag(const std::filesystem::path& repoPath,
                                 const bo::CreateTagOptions& options) const
    {
        if (auto valid = validateRepoPath(repoPath, "history"); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (!isValidTagName(options.name))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidTagName,
                .exitCode = -1,
                .message = "Nome del tag non valido secondo le specifiche git: '" + options.name + "'",
                .command = "tag"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.createTag(repoPath, options);
    }

    std::expected<void, ports::GitOpsError>
    GitHistoryService::deleteTag(const std::filesystem::path& repoPath,
                                 const std::string& tagName) const
    {
        if (auto valid = validateRepoPath(repoPath, "history"); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (tagName.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidTagName,
                .exitCode = -1,
                .message = "Il nome del tag da eliminare non può essere vuoto",
                .command = "tag -d"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.deleteTag(repoPath, tagName);
    }
} // namespace gf::git::bl
