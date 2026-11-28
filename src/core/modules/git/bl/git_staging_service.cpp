#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/modules/git/bl/git_validation.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

#include <algorithm>
#include <cctype>

namespace gf::git::bl
{
    namespace
    {
        [[nodiscard]] bool isWhitespaceOnly(const std::string_view str) noexcept
        {
            return std::ranges::all_of(str, [](const unsigned char ch)
            {
                return std::isspace(ch) != 0;
            });
        }
    } // namespace

    GitStagingService::GitStagingService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    std::expected<void, ports::GitOpsError>
    GitStagingService::stageFiles(const std::filesystem::path& repoPath,
                                  const std::vector<std::string>& filePaths) const
    {
        if (auto valid = validateRepoPath(repoPath, "staging"); !valid)
        {
            return valid;
        }
        if (filePaths.empty())
        {
            return {};
        }
        if (auto validPaths = validateRelativeFilePaths(filePaths, "staging"); !validPaths)
        {
            return validPaths;
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.stageFiles(repoPath, filePaths);
    }

    std::expected<void, ports::GitOpsError>
    GitStagingService::stageAll(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "staging"); !valid)
        {
            return valid;
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.stageAll(repoPath);
    }

    std::expected<void, ports::GitOpsError>
    GitStagingService::unstageFiles(const std::filesystem::path& repoPath,
                                    const std::vector<std::string>& filePaths) const
    {
        if (auto valid = validateRepoPath(repoPath, "staging"); !valid)
        {
            return valid;
        }
        if (filePaths.empty())
        {
            return {};
        }
        if (auto validPaths = validateRelativeFilePaths(filePaths, "staging"); !validPaths)
        {
            return validPaths;
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.unstageFiles(repoPath, filePaths);
    }

    std::expected<void, ports::GitOpsError>
    GitStagingService::unstageAll(const std::filesystem::path& repoPath) const
    {
        if (auto valid = validateRepoPath(repoPath, "staging"); !valid)
        {
            return valid;
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.unstageAll(repoPath);
    }

    std::expected<void, ports::GitOpsError>
    GitStagingService::discardFiles(const std::filesystem::path& repoPath,
                                    const std::vector<std::string>& filePaths) const
    {
        if (auto valid = validateRepoPath(repoPath, "staging"); !valid)
        {
            return valid;
        }
        if (filePaths.empty())
        {
            return {};
        }
        if (auto validPaths = validateRelativeFilePaths(filePaths, "staging"); !validPaths)
        {
            return validPaths;
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.discardFiles(repoPath, filePaths);
    }

    std::expected<bo::CommitResult, ports::GitOpsError>
    GitStagingService::createCommit(const std::filesystem::path& repoPath,
                                    const bo::CommitOptions& options) const
    {
        if (auto valid = validateRepoPath(repoPath, "staging"); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (options.message.empty() || isWhitespaceOnly(options.message))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::EmptyCommitMessage,
                .exitCode = -1,
                .message = "Il messaggio di commit non può essere vuoto",
                .command = "commit"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.createCommit(repoPath, options);
    }
} // namespace gf::git::bl
