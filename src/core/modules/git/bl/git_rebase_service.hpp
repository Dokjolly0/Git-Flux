#pragma once

#include "core/modules/git/bo/rebase_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>

namespace gf::git::bl {

class GitRebaseService {
public:
    explicit GitRebaseService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitRebaseService() = default;

    /// Avvia una sessione di rebase con lock per-repo.
    [[nodiscard]] std::expected<bo::RebaseResult, ports::GitOpsError>
    startRebase(const std::filesystem::path& repoPath, const bo::RebaseOptions& options);

    /// Continua un rebase in corso dopo aver risolto i conflitti con lock per-repo.
    [[nodiscard]] std::expected<bo::RebaseResult, ports::GitOpsError>
    continueRebase(const std::filesystem::path& repoPath);

    /// Salta il commit corrente durante un rebase con lock per-repo.
    [[nodiscard]] std::expected<bo::RebaseResult, ports::GitOpsError>
    skipRebase(const std::filesystem::path& repoPath);

    /// Annulla un rebase in corso ripristinando lo stato precedente con lock per-repo.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    abortRebase(const std::filesystem::path& repoPath);

    /// Verifica se una sessione di rebase è attualmente in corso.
    [[nodiscard]] std::expected<bool, ports::GitOpsError>
    isRebaseInProgress(const std::filesystem::path& repoPath);

    /// Esegue il cherry-pick di un commit specificato con lock per-repo.
    [[nodiscard]] std::expected<bo::CherryPickResult, ports::GitOpsError>
    cherryPick(const std::filesystem::path& repoPath, const bo::CherryPickOptions& options);

    /// Annulla un cherry-pick in corso con lock per-repo.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    abortCherryPick(const std::filesystem::path& repoPath);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
