#pragma once

#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::git::bl {

class GitStagingService {
public:
    explicit GitStagingService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitStagingService() = default;

    /// Aggiunge file specifici all'area di staging con validazione e mutua esclusione.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    stageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths);

    /// Aggiunge tutte le modifiche del working tree all'area di staging.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    stageAll(const std::filesystem::path& repoPath);

    /// Rimuove file specifici dall'area di staging.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    unstageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths);

    /// Rimuove tutti i file dall'area di staging.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    unstageAll(const std::filesystem::path& repoPath);

    /// Scarta le modifiche locali non staged di file tracciati o rimuove file untracked.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    discardFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths);

    /// Crea un commit git dopo aver validato il messaggio e i requisiti di staging.
    [[nodiscard]] std::expected<bo::CommitResult, ports::GitOpsError>
    createCommit(const std::filesystem::path& repoPath, const bo::CommitOptions& options);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRelativeFilePaths(const std::vector<std::string>& filePaths) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
