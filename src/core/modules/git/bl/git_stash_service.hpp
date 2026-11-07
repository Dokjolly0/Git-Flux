#pragma once

#include "core/modules/git/bo/stash_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <vector>

namespace gf::git::bl {

class GitStashService {
public:
    explicit GitStashService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitStashService() = default;

    /// Ritorna la lista degli stash salvati nel repository.
    [[nodiscard]] std::expected<std::vector<bo::GitStashItem>, ports::GitOpsError>
    listStashes(const std::filesystem::path& repoPath);

    /// Salva le modifiche correnti in un nuovo stash con lock per-repo.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    stashPush(const std::filesystem::path& repoPath, const bo::StashPushOptions& options = {});

    /// Applica e rimuove uno stash con lock per-repo.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    stashPop(const std::filesystem::path& repoPath, const bo::StashApplyOptions& options = {});

    /// Applica uno stash mantenendolo salvato con lock per-repo.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    stashApply(const std::filesystem::path& repoPath, const bo::StashApplyOptions& options = {});

    /// Rimuove uno stash specifico con lock per-repo.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    stashDrop(const std::filesystem::path& repoPath, std::size_t stashIndex = 0);

    /// Rimuove tutti gli stash con lock per-repo.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    stashClear(const std::filesystem::path& repoPath);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
