#pragma once

#include "core/modules/git/bo/history_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gf::git::bl {

class GitHistoryService {
public:
    explicit GitHistoryService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitHistoryService() = default;

    /// Checks if a tag name conforms to git-check-ref-format specifications.
    [[nodiscard]] static bool isValidTagName(std::string_view name) noexcept;

    /// Returns commit history with pagination and filtering options.
    [[nodiscard]] std::expected<std::vector<bo::GitCommitLogItem>, ports::GitOpsError>
    getCommitHistory(const std::filesystem::path& repoPath, const bo::CommitLogOptions& options = {});

    /// Returns details of a single commit given its SHA hash or reference.
    [[nodiscard]] std::expected<std::optional<bo::GitCommitLogItem>, ports::GitOpsError>
    getCommitDetails(const std::filesystem::path& repoPath, const std::string& commitSha);

    /// Returns the list of files modified, added, deleted, or renamed by a commit.
    [[nodiscard]] std::expected<std::vector<bo::CommitFileChange>, ports::GitOpsError>
    getCommitFiles(const std::filesystem::path& repoPath, const std::string& commitSha);

    /// Returns all repository tags (lightweight and annotated).
    [[nodiscard]] std::expected<std::vector<bo::GitTag>, ports::GitOpsError>
    listTags(const std::filesystem::path& repoPath);

    /// Creates a new tag after validating the name and acquiring per-repo lock.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    createTag(const std::filesystem::path& repoPath, const bo::CreateTagOptions& options);

    /// Deletes a local tag acquiring per-repo lock.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    deleteTag(const std::filesystem::path& repoPath, const std::string& tagName);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
