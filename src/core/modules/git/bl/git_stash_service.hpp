#pragma once

#include "core/modules/git/bo/stash_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <vector>

namespace gf::git::bl
{
    class GitStashService
    {
    public:
        explicit GitStashService(ports::IRepositoryGitOps& gitOps) noexcept;
        ~GitStashService() = default;

        /// Returns the list of stashes saved in the repository.
        [[nodiscard]] std::expected<std::vector<bo::GitStashItem>, ports::GitOpsError>
        listStashes(const std::filesystem::path& repoPath);

        /// Saves current changes into a new stash under per-repo lock.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        stashPush(const std::filesystem::path& repoPath, const bo::StashPushOptions& options = {});

        /// Applies and drops a stash under per-repo lock.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        stashPop(const std::filesystem::path& repoPath, const bo::StashApplyOptions& options = {});

        /// Applies a stash keeping it saved under per-repo lock.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        stashApply(const std::filesystem::path& repoPath, const bo::StashApplyOptions& options = {});

        /// Drops a specific stash under per-repo lock.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        stashDrop(const std::filesystem::path& repoPath, std::size_t stashIndex = 0);

        /// Clears all stashes under per-repo lock.
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        stashClear(const std::filesystem::path& repoPath);

    private:
        [[nodiscard]] std::expected<void, ports::GitOpsError>
        validateRepoPath(const std::filesystem::path& repoPath) const;

        ports::IRepositoryGitOps& m_gitOps;
    };
} // namespace gf::git::bl
