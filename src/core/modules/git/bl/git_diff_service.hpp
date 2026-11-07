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

    /// Estrae il diff per un singolo file nello scope specificato.
    [[nodiscard]] std::expected<bo::FileDiff, ports::GitOpsError>
    getFileDiff(const std::filesystem::path& repoPath,
                const std::string& filePath,
                bo::DiffScope scope);

    /// Shortcut per diff di un file staged.
    [[nodiscard]] std::expected<bo::FileDiff, ports::GitOpsError>
    getStagedFileDiff(const std::filesystem::path& repoPath, const std::string& filePath) {
        return getFileDiff(repoPath, filePath, bo::DiffScope::Staged);
    }

    /// Shortcut per diff di un file unstaged tracciato.
    [[nodiscard]] std::expected<bo::FileDiff, ports::GitOpsError>
    getUnstagedFileDiff(const std::filesystem::path& repoPath, const std::string& filePath) {
        return getFileDiff(repoPath, filePath, bo::DiffScope::Unstaged);
    }

    /// Shortcut per diff di un file untracked.
    [[nodiscard]] std::expected<bo::FileDiff, ports::GitOpsError>
    getUntrackedFileDiff(const std::filesystem::path& repoPath, const std::string& filePath) {
        return getFileDiff(repoPath, filePath, bo::DiffScope::Untracked);
    }

    /// Estrae tutti i diff per lo scope specificato (staged o unstaged).
    [[nodiscard]] std::expected<std::vector<bo::FileDiff>, ports::GitOpsError>
    getAllDiffs(const std::filesystem::path& repoPath, bo::DiffScope scope);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRelativeFilePath(const std::string& filePath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
