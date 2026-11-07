#include "core/modules/git/bl/git_diff_service.hpp"

namespace gf::git::bl {

GitDiffService::GitDiffService(ports::IRepositoryGitOps& gitOps) noexcept
    : m_gitOps(gitOps) {}

std::expected<void, ports::GitOpsError>
GitDiffService::validateRepoPath(const std::filesystem::path& repoPath) const {
    if (repoPath.empty()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
            .exitCode = -1,
            .message = "Il percorso del repository non può essere vuoto",
            .command = "diff"
        });
    }

    std::error_code ec;
    if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec)) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::PathNotFound,
            .exitCode = -1,
            .message = "La directory del repository non esiste: " + repoPath.string(),
            .command = "diff"
        });
    }

    return {};
}

std::expected<void, ports::GitOpsError>
GitDiffService::validateRelativeFilePath(const std::string& filePath) const {
    if (filePath.empty()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
            .exitCode = -1,
            .message = "Il percorso del file non può essere vuoto",
            .command = "diff"
        });
    }

    std::filesystem::path p(filePath);
    if (p.is_absolute()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
            .exitCode = -1,
            .message = "Il percorso del file deve essere relativo al repository: " + filePath,
            .command = "diff"
        });
    }

    for (const auto& part : p) {
        if (part == "..") {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Rilevato tentativo di path traversal illegale: " + filePath,
                .command = "diff"
            });
        }
    }

    return {};
}

std::expected<bo::FileDiff, ports::GitOpsError>
GitDiffService::getFileDiff(const std::filesystem::path& repoPath,
                            const std::string& filePath,
                            bo::DiffScope scope) {
    if (auto validRepo = validateRepoPath(repoPath); !validRepo) {
        return std::unexpected(validRepo.error());
    }

    if (auto validPath = validateRelativeFilePath(filePath); !validPath) {
        return std::unexpected(validPath.error());
    }

    return m_gitOps.getFileDiff(repoPath, filePath, scope);
}

std::expected<std::vector<bo::FileDiff>, ports::GitOpsError>
GitDiffService::getAllDiffs(const std::filesystem::path& repoPath,
                            bo::DiffScope scope) {
    if (auto validRepo = validateRepoPath(repoPath); !validRepo) {
        return std::unexpected(validRepo.error());
    }

    return m_gitOps.getAllDiffs(repoPath, scope);
}

} // namespace gf::git::bl
