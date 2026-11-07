#pragma once

#include "core/modules/git/bo/conflict_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace gf::git::bl {

class GitConflictService {
public:
    explicit GitConflictService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitConflictService() = default;

    /// Ritorna la struttura dettagliata di un file in conflitto (marker, hunks, stage).
    [[nodiscard]] std::expected<bo::ConflictFile, ports::GitOpsError>
    getConflictFile(const std::filesystem::path& repoPath, const std::string& filePath);

    /// Estrae il contenuto grezzo di uno stage (1=base, 2=ours, 3=theirs).
    [[nodiscard]] std::expected<std::string, ports::GitOpsError>
    getFileStageContent(const std::filesystem::path& repoPath, const std::string& filePath, int stage);

    /// Salva il contenuto risolto del file ed opzionalmente aggiunge il file all'area di staging con lock per-repo.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    applyConflictResolution(const std::filesystem::path& repoPath,
                            const bo::ApplyConflictResolutionOptions& options);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
