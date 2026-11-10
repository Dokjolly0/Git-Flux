#pragma once

#include "core/modules/git/bo/remote_merge_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gf::git::bl
{
    class GitRemoteMergeService
    {
    public:
        explicit GitRemoteMergeService(ports::IRepositoryGitOps& gitOps) noexcept;
        ~GitRemoteMergeService() = default;

        /// Checks if a remote identifier is valid.
        [[nodiscard]] static bool isValidRemoteName(std::string_view name) noexcept;

        /// Returns the list of configured remotes.
        [[nodiscard]] std::expected<std::vector<bo::GitRemote>, ports::GitOpsError>
        listRemotes(const std::filesystem::path& repoPath);

        /// Adds a new remote after validating name and URL.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        addRemote(const std::filesystem::path& repoPath, const std::string& name, const std::string& url);

        /// Removes a configured remote.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        removeRemote(const std::filesystem::path& repoPath, const std::string& name);

        /// Updates the URL of a configured remote.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        setRemoteUrl(const std::filesystem::path& repoPath, const std::string& name, const std::string& url);

        /// Fetches from a remote acquiring the per-repository lock.
        [[nodiscard]] std::expected<bo::RemoteOperationResult, ports::GitOpsError>
        fetchRemote(const std::filesystem::path& repoPath, const bo::RemoteOperationOptions& options = {});

        /// Pulls from a remote acquiring the per-repository lock.
        [[nodiscard]] std::expected<bo::RemoteOperationResult, ports::GitOpsError>
        pullRemote(const std::filesystem::path& repoPath, const bo::RemoteOperationOptions& options = {});

        /// Pushes to a remote acquiring the per-repository lock.
        [[nodiscard]] std::expected<bo::RemoteOperationResult, ports::GitOpsError>
        pushRemote(const std::filesystem::path& repoPath, const bo::RemoteOperationOptions& options = {});

        /// Merges into active branch acquiring the per-repository lock.
        [[nodiscard]] std::expected<bo::MergeResult, ports::GitOpsError>
        mergeBranch(const std::filesystem::path& repoPath, const bo::MergeOptions& options);

        /// Aborts an in-progress merge (`git merge --abort`).
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        abortMerge(const std::filesystem::path& repoPath);

    private:
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        validateRepoPath(const std::filesystem::path& repoPath) const;

        ports::IRepositoryGitOps& m_gitOps;
    };
} // namespace gf::git::bl
