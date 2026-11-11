#include "core/modules/git/bl/git_branch_service.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

#include <algorithm>

namespace gf::git::bl
{
    GitBranchService::GitBranchService(ports::IRepositoryGitOps& gitOps) noexcept
        : m_gitOps(gitOps)
    {
    }

    bool GitBranchService::isValidBranchName(std::string_view name) noexcept
    {
        if (name.empty() || name == "@")
        {
            return false;
        }
        if (name.front() == '.' || name.front() == '-' || name.front() == '/')
        {
            return false;
        }
        if (name.back() == '.' || name.back() == '/')
        {
            return false;
        }
        if (name.ends_with(".lock"))
        {
            return false;
        }

        for (char ch : name)
        {
            unsigned char c = static_cast<unsigned char>(ch);
            if (c < 0x20 || c == 0x7f)
            {
                return false;
            }
            if (c == ' ' || c == '~' || c == '^' || c == ':' || c == '?' || c == '*' || c == '[' || c == '\\')
            {
                return false;
            }
        }

        if (name.find("..") != std::string_view::npos)
        {
            return false;
        }
        if (name.find("//") != std::string_view::npos)
        {
            return false;
        }
        if (name.find("@{") != std::string_view::npos)
        {
            return false;
        }

        return true;
    }

    std::expected<void, ports::GitOpsError>
    GitBranchService::validateRepoPath(const std::filesystem::path& repoPath) const
    {
        if (repoPath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del repository non può essere vuoto",
                .command = "branch"
            });
        }

        std::error_code ec;
        if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::PathNotFound,
                .exitCode = -1,
                .message = "La directory del repository non esiste: " + repoPath.string(),
                .command = "branch"
            });
        }

        return {};
    }

    std::expected<std::vector<bo::GitBranch>, ports::GitOpsError>
    GitBranchService::listBranches(const std::filesystem::path& repoPath)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }

        return m_gitOps.listBranches(repoPath);
    }

    std::expected<std::vector<bo::GitBranch>, ports::GitOpsError>
    GitBranchService::getLocalBranches(const std::filesystem::path& repoPath)
    {
        auto all = listBranches(repoPath);
        if (!all)
        {
            return all;
        }

        std::vector<bo::GitBranch> localBranches;
        for (auto& b : *all)
        {
            if (b.type == bo::BranchType::Local)
            {
                localBranches.push_back(std::move(b));
            }
        }
        return localBranches;
    }

    std::expected<std::vector<bo::GitBranch>, ports::GitOpsError>
    GitBranchService::getRemoteBranches(const std::filesystem::path& repoPath)
    {
        auto all = listBranches(repoPath);
        if (!all)
        {
            return all;
        }

        std::vector<bo::GitBranch> remoteBranches;
        for (auto& b : *all)
        {
            if (b.type == bo::BranchType::Remote)
            {
                remoteBranches.push_back(std::move(b));
            }
        }
        return remoteBranches;
    }

    std::expected<std::optional<bo::GitBranch>, ports::GitOpsError>
    GitBranchService::getCurrentBranch(const std::filesystem::path& repoPath)
    {
        auto all = listBranches(repoPath);
        if (!all)
        {
            return std::unexpected(all.error());
        }

        for (const auto& b : *all)
        {
            if (b.isCurrent)
            {
                return b;
            }
        }
        return std::nullopt;
    }

    std::expected<void, ports::GitOpsError>
    GitBranchService::createBranch(const std::filesystem::path& repoPath,
                                   const bo::CreateBranchOptions& options)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return valid;
        }

        if (!isValidBranchName(options.name))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidBranchName,
                .exitCode = -1,
                .message = "Nome del branch non valido: " + options.name,
                .command = "branch"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.createBranch(repoPath, options);
    }

    std::expected<void, ports::GitOpsError>
    GitBranchService::checkoutBranch(const std::filesystem::path& repoPath,
                                     const bo::CheckoutOptions& options)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return valid;
        }

        if (options.branchOrCommit.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidBranchName,
                .exitCode = -1,
                .message = "Specificatore di checkout non può essere vuoto",
                .command = "checkout"
            });
        }

        if (options.createNewBranch && !isValidBranchName(options.branchOrCommit))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidBranchName,
                .exitCode = -1,
                .message = "Nome del nuovo branch non valido: " + options.branchOrCommit,
                .command = "checkout"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.checkoutBranch(repoPath, options);
    }

    std::expected<void, ports::GitOpsError>
    GitBranchService::deleteBranch(const std::filesystem::path& repoPath,
                                   const std::string& branchName,
                                   bool force)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return valid;
        }

        if (branchName.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidBranchName,
                .exitCode = -1,
                .message = "Nome del branch da eliminare non può essere vuoto",
                .command = "branch -d"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.deleteBranch(repoPath, branchName, force);
    }

    std::expected<void, ports::GitOpsError>
    GitBranchService::renameBranch(const std::filesystem::path& repoPath,
                                   const std::string& oldName,
                                   const std::string& newName)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return valid;
        }

        if (oldName.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidBranchName,
                .exitCode = -1,
                .message = "Nome del branch originario non può essere vuoto",
                .command = "branch -m"
            });
        }

        if (!isValidBranchName(newName))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidBranchName,
                .exitCode = -1,
                .message = "Nuovo nome del branch non valido: " + newName,
                .command = "branch -m"
            });
        }

        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.renameBranch(repoPath, oldName, newName);
    }

    std::expected<void, ports::GitOpsError>
    GitBranchService::resetCommit(const std::filesystem::path& repoPath,
                                  const std::string& commitSha,
                                  const std::string& mode)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return valid;
        }
        if (commitSha.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "SHA del commit non può essere vuoto",
                .command = "reset"
            });
        }
        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.resetCommit(repoPath, commitSha, mode);
    }

    std::expected<void, ports::GitOpsError>
    GitBranchService::revertCommit(const std::filesystem::path& repoPath,
                                   const std::string& commitSha)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return valid;
        }
        if (commitSha.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "SHA del commit non può essere vuoto",
                .command = "revert"
            });
        }
        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.revertCommit(repoPath, commitSha);
    }

    std::expected<std::string, ports::GitOpsError>
    GitBranchService::rewordCommit(const std::filesystem::path& repoPath,
                                   const std::string& commitSha,
                                   const std::string& newMessage)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }
        if (commitSha.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "SHA del commit non può essere vuoto",
                .command = "reword"
            });
        }
        if (newMessage.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::EmptyCommitMessage,
                .exitCode = -1,
                .message = "Il messaggio del commit non può essere vuoto",
                .command = "reword"
            });
        }
        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.rewordCommit(repoPath, commitSha, newMessage);
    }

    std::expected<bool, ports::GitOpsError>
    GitBranchService::canFastForwardBranch(const std::filesystem::path& repoPath,
                                           const std::string& targetBranch,
                                           const std::string& sourceRef)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return std::unexpected(valid.error());
        }
        if (targetBranch.empty() || sourceRef.empty())
        {
            return false;
        }
        return m_gitOps.canFastForwardBranch(repoPath, targetBranch, sourceRef);
    }

    std::expected<void, ports::GitOpsError>
    GitBranchService::fastForwardBranch(const std::filesystem::path& repoPath,
                                        const std::string& targetBranch,
                                        const std::string& sourceRef)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return valid;
        }
        if (targetBranch.empty() || sourceRef.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidBranchName,
                .exitCode = -1,
                .message = "Branch di origine o destinazione non specificato",
                .command = "fast-forward"
            });
        }
        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.fastForwardBranch(repoPath, targetBranch, sourceRef);
    }

    std::expected<void, ports::GitOpsError>
    GitBranchService::createWorktree(const std::filesystem::path& repoPath,
                                     const std::filesystem::path& worktreePath,
                                     const std::string& commitOrBranch)
    {
        if (auto valid = validateRepoPath(repoPath); !valid)
        {
            return valid;
        }
        if (worktreePath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Percorso della directory del worktree non specificato",
                .command = "worktree"
            });
        }
        auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
        return m_gitOps.createWorktree(repoPath, worktreePath, commitOrBranch);
    }
} // namespace gf::git::bl
