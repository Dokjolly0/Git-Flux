#pragma once

#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::git::bl
{
    class GitStagingService
    {
    public:
        explicit GitStagingService(ports::IRepositoryGitOps& gitOps) noexcept;
        ~GitStagingService() = default;

        /// Stages specific files with validation and mutual exclusion.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        stageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) const;

        /// Stages all working tree modifications into the staging area.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        stageAll(const std::filesystem::path& repoPath) const;

        /// Unstages specific files from the staging area.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        unstageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) const;

        /// Unstages all files from the staging area.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        unstageAll(const std::filesystem::path& repoPath) const;

        /// Discards unstaged local modifications of tracked files or removes untracked files.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        discardFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) const;

        /// Creates a git commit after validating the message and staging prerequisites.
        [[nodiscard]] std::expected<bo::CommitResult, ports::GitOpsError>
        createCommit(const std::filesystem::path& repoPath, const bo::CommitOptions& options) const;

    private:
        [[nodiscard]] static std::expected<void, ports::GitOpsError>
        validateRepoPath(const std::filesystem::path& repoPath);

        [[nodiscard]] static std::expected<void, ports::GitOpsError>
        validateRelativeFilePaths(const std::vector<std::string>& filePaths);

        ports::IRepositoryGitOps& m_gitOps;
    };
} // namespace gf::git::bl
