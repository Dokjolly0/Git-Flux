#pragma once

#include "core/modules/git/bo/commit_options.hpp"
#include "core/modules/git/bo/diff_models.hpp"
#include "core/modules/git/bo/repo_working_tree_status.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::git::ports {

struct GitOpsError {
    enum class Kind {
        InvalidRepositoryPath,
        PathNotFound,
        ExecutionFailed,
        IndexLocked,
        ConflictPresent,
        EmptyCommitMessage,
        NothingToCommit,
        StorageError
    };

    Kind kind = Kind::ExecutionFailed;
    int exitCode = -1;
    std::string message;
    std::string command;
};

class IRepositoryGitOps {
public:
    virtual ~IRepositoryGitOps() = default;

    /// Ritorna lo stato dettagliato e classificato del working tree del repository.
    [[nodiscard]] virtual std::expected<bo::RepoWorkingTreeStatus, GitOpsError>
    getWorkingTreeStatus(const std::filesystem::path& repoPath) = 0;

    /// Aggiunge percorsi specifici all'area di staging (`git add -- <paths>`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    stageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) = 0;

    /// Aggiunge tutte le modifiche del working tree all'area di staging (`git add -A`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    stageAll(const std::filesystem::path& repoPath) = 0;

    /// Rimuove percorsi specifici dall'area di staging.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    unstageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) = 0;

    /// Rimuove tutte le modifiche dall'area di staging.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    unstageAll(const std::filesystem::path& repoPath) = 0;

    /// Scarta le modifiche locali non staged ripristinando file tracciati o rimuovendo file untracked.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    discardFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) = 0;

    /// Crea un commit con i file attualmente staged.
    [[nodiscard]] virtual std::expected<bo::CommitResult, GitOpsError>
    createCommit(const std::filesystem::path& repoPath, const bo::CommitOptions& options) = 0;

    /// Estrae il diff per un file specifico nello scope specificato (staged, unstaged o untracked).
    [[nodiscard]] virtual std::expected<bo::FileDiff, GitOpsError>
    getFileDiff(const std::filesystem::path& repoPath,
                const std::string& filePath,
                bo::DiffScope scope) = 0;

    /// Estrae tutti i diff per lo scope specificato (staged o unstaged).
    [[nodiscard]] virtual std::expected<std::vector<bo::FileDiff>, GitOpsError>
    getAllDiffs(const std::filesystem::path& repoPath,
                bo::DiffScope scope) = 0;
};

} // namespace gf::git::ports
