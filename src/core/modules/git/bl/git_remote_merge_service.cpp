#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

#include <cctype>

namespace gf::git::bl
{
    GitRemoteMergeService::GitRemoteMergeService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    bool GitRemoteMergeService::isValidRemoteName(std::string_view name) noexcept
    {
        if (name.empty())
        {
            return false;
        }
        for (char ch : name)
        {
            unsigned char c = static_cast<unsigned char>(ch);
            if (c <= 0x20 || c == 0x7f)
            {
                return false;
            }
            if (c == '/' || c == '\\' || c == ':' || c == '~' || c == '^' || c == '?' || c == '*' || c == '[')
            {
                return false;
            }
        }
        return true;
    }

    std::expected<void, ports::GitOpsError>
    GitRemoteMergeService::validateRepoPath(const std::filesystem::path& repoPath) const
    {
        if (repoPath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del repository non può essere vuoto",
                .command = "remote"
            });
        }

        std::error_code ec;
        if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::PathNotFound,
                .exitCode = -1,
                .message = "La directory del repository non esiste: " + repoPath.string(),
                .command = "remote"
            });
        }

        return {};
    }

    std::expected<std::vector<bo::GitRemote>, ports::GitOpsError>
    GitRemoteMergeService::listRemotes(const std::filesystem::path& repoPath)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        return m_gitOps.listRemotes(repoPath);
    }

    std::expected<void, ports::GitOpsError>
    GitRemoteMergeService::addRemote(const std::filesystem::path& repoPath,
                                     const std::string& name,
                                     const std::string& url)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (!isValidRemoteName(name))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Nome del remote non valido: '" + name + "'",
                .command = "remote add"
            });
        }

        if (url.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "L'URL del remote non può essere vuoto",
                .command = "remote add"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.addRemote(repoPath, name, url);
    }

    std::expected<void, ports::GitOpsError>
    GitRemoteMergeService::removeRemote(const std::filesystem::path& repoPath,
                                        const std::string& name)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (name.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il nome del remote da rimuovere non può essere vuoto",
                .command = "remote remove"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.removeRemote(repoPath, name);
    }

    std::expected<void, ports::GitOpsError>
    GitRemoteMergeService::setRemoteUrl(const std::filesystem::path& repoPath,
                                        const std::string& name,
                                        const std::string& url)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (name.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il nome del remote non può essere vuoto",
                .command = "remote set-url"
            });
        }

        if (url.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "L'URL del remote non può essere vuoto",
                .command = "remote set-url"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.setRemoteUrl(repoPath, name, url);
    }

    std::expected<bo::RemoteOperationResult, ports::GitOpsError>
    GitRemoteMergeService::fetchRemote(const std::filesystem::path& repoPath,
                                       const bo::RemoteOperationOptions& options)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.fetchRemote(repoPath, options);
    }

    std::expected<bo::RemoteOperationResult, ports::GitOpsError>
    GitRemoteMergeService::pullRemote(const std::filesystem::path& repoPath,
                                      const bo::RemoteOperationOptions& options)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.pullRemote(repoPath, options);
    }

    std::expected<bo::RemoteOperationResult, ports::GitOpsError>
    GitRemoteMergeService::pushRemote(const std::filesystem::path& repoPath,
                                      const bo::RemoteOperationOptions& options)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.pushRemote(repoPath, options);
    }

    std::expected<bo::MergeResult, ports::GitOpsError>
    GitRemoteMergeService::mergeBranch(const std::filesystem::path& repoPath,
                                       const bo::MergeOptions& options)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        if (options.sourceBranchOrCommit.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il branch o commit sorgente per il merge non può essere vuoto",
                .command = "merge"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.mergeBranch(repoPath, options);
    }

    std::expected<void, ports::GitOpsError>
    GitRemoteMergeService::abortMerge(const std::filesystem::path& repoPath)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.abortMerge(repoPath);
    }
} // namespace gf::git::bl
