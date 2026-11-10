#pragma once

#include "core/modules/git/bo/conflict_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace gf::git::bl
{
    class GitConflictService
    {
    public:
        explicit GitConflictService(ports::IRepositoryGitOps& gitOps) noexcept;
        ~GitConflictService() = default;

        /// Returns the detailed structure of a conflicted file (markers, hunks, stages).
        [[nodiscard]] std::expected<bo::ConflictFile, ports::GitOpsError>
        getConflictFile(const std::filesystem::path& repoPath, const std::string& filePath);

        /// Extracts the raw content of a git stage (1=base, 2=ours, 3=theirs).
        [[nodiscard]] std::expected<std::string, ports::GitOpsError>
        getFileStageContent(const std::filesystem::path& repoPath, const std::string& filePath, int stage);

        /// Writes the resolved content and optionally stages the file under per-repo lock.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        applyConflictResolution(const std::filesystem::path& repoPath,
                                const bo::ApplyConflictResolutionOptions& options);

    private:
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        validateRepoPath(const std::filesystem::path& repoPath) const;

        ports::IRepositoryGitOps& m_gitOps;
    };
} // namespace gf::git::bl
