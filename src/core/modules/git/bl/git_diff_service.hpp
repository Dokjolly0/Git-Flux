#pragma once

#include "core/modules/git/bo/diff_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::git::bl {

class GitDiffService {
public:
    explicit GitDiffService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitDiffService() = default;

    /// Extracts the diff for a single file within the specified scope.
    [[nodiscard]] std::expected<bo::FileDiff, ports::GitOpsError>
    getFileDiff(const std::filesystem::path& repoPath,
                const std::string& filePath,
                bo::DiffScope scope);

    /// Shortcut for diff of a staged file.
    [[nodiscard]] std::expected<bo::FileDiff, ports::GitOpsError>
    getStagedFileDiff(const std::filesystem::path& repoPath, const std::string& filePath) {
        return getFileDiff(repoPath, filePath, bo::DiffScope::Staged);
    }

    /// Shortcut for diff of a tracked unstaged file.
    [[nodiscard]] std::expected<bo::FileDiff, ports::GitOpsError>
    getUnstagedFileDiff(const std::filesystem::path& repoPath, const std::string& filePath) {
        return getFileDiff(repoPath, filePath, bo::DiffScope::Unstaged);
    }

    /// Shortcut for diff of an untracked file.
    [[nodiscard]] std::expected<bo::FileDiff, ports::GitOpsError>
    getUntrackedFileDiff(const std::filesystem::path& repoPath, const std::string& filePath) {
        return getFileDiff(repoPath, filePath, bo::DiffScope::Untracked);
    }

    /// Extracts all diffs for the specified scope (staged or unstaged).
    [[nodiscard]] std::expected<std::vector<bo::FileDiff>, ports::GitOpsError>
    getAllDiffs(const std::filesystem::path& repoPath, bo::DiffScope scope);

    /// Extracts the diff for a single file in a specific commit.
    [[nodiscard]] std::expected<bo::FileDiff, ports::GitOpsError>
    getCommitFileDiff(const std::filesystem::path& repoPath,
                      const std::string& commitSha,
                      const std::string& filePath);

    /// Extracts blame line annotations for a file at a specific revision.
    [[nodiscard]] std::expected<std::vector<bo::BlameLine>, ports::GitOpsError>
    getFileBlame(const std::filesystem::path& repoPath,
                 const std::string& revision,
                 const std::string& filePath);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRelativeFilePath(const std::string& filePath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
