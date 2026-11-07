#pragma once

#include "core/modules/git/bo/repo_working_tree_status.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace gf::git::ports {

struct GitOpsError {
    enum class Kind {
        InvalidRepositoryPath,
        PathNotFound,
        ExecutionFailed,
        IndexLocked,
        ConflictPresent,
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
};

} // namespace gf::git::ports
