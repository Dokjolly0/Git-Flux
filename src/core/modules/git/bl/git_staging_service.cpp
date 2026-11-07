#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

#include <algorithm>
#include <cctype>

namespace gf::git::bl {

namespace {

[[nodiscard]] bool isWhitespaceOnly(std::string_view str) noexcept {
    return std::all_of(str.begin(), str.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
}

} // namespace

GitStagingService::GitStagingService(ports::IRepositoryGitOps& gitOps) noexcept
    : m_gitOps(gitOps) {}

std::expected<void, ports::GitOpsError>
GitStagingService::validateRepoPath(const std::filesystem::path& repoPath) const {
    if (repoPath.empty()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
            .exitCode = -1,
            .message = "Il percorso del repository non può essere vuoto",
            .command = "staging"
        });
    }

    std::error_code ec;
    if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec)) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::PathNotFound,
            .exitCode = -1,
            .message = "La directory del repository non esiste: " + repoPath.string(),
            .command = "staging"
        });
    }

    return {};
}

std::expected<void, ports::GitOpsError>
GitStagingService::validateRelativeFilePaths(const std::vector<std::string>& filePaths) const {
    for (const auto& pathStr : filePaths) {
        if (pathStr.empty()) {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso di un file non può essere vuoto",
                .command = "staging"
            });
        }

        std::filesystem::path p(pathStr);
        if (p.is_absolute()) {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "I percorsi dei file devono essere relativi al repository: " + pathStr,
                .command = "staging"
            });
        }

        // Reject path traversal
        for (const auto& part : p) {
            if (part == "..") {
                return std::unexpected(ports::GitOpsError{
                    .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                    .exitCode = -1,
                    .message = "Rilevato tentativo di path traversal illegale: " + pathStr,
                    .command = "staging"
                });
            }
        }
    }
    return {};
}

std::expected<void, ports::GitOpsError>
GitStagingService::stageFiles(const std::filesystem::path& repoPath,
                              const std::vector<std::string>& filePaths) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return valid;
    }
    if (filePaths.empty()) {
        return {};
    }
    if (auto validPaths = validateRelativeFilePaths(filePaths); !validPaths) {
        return validPaths;
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.stageFiles(repoPath, filePaths);
}

std::expected<void, ports::GitOpsError>
GitStagingService::stageAll(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return valid;
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.stageAll(repoPath);
}

std::expected<void, ports::GitOpsError>
GitStagingService::unstageFiles(const std::filesystem::path& repoPath,
                                const std::vector<std::string>& filePaths) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return valid;
    }
    if (filePaths.empty()) {
        return {};
    }
    if (auto validPaths = validateRelativeFilePaths(filePaths); !validPaths) {
        return validPaths;
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.unstageFiles(repoPath, filePaths);
}

std::expected<void, ports::GitOpsError>
GitStagingService::unstageAll(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return valid;
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.unstageAll(repoPath);
}

std::expected<void, ports::GitOpsError>
GitStagingService::discardFiles(const std::filesystem::path& repoPath,
                                const std::vector<std::string>& filePaths) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return valid;
    }
    if (filePaths.empty()) {
        return {};
    }
    if (auto validPaths = validateRelativeFilePaths(filePaths); !validPaths) {
        return validPaths;
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.discardFiles(repoPath, filePaths);
}

std::expected<bo::CommitResult, ports::GitOpsError>
GitStagingService::createCommit(const std::filesystem::path& repoPath,
                                const bo::CommitOptions& options) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    if (options.message.empty() || isWhitespaceOnly(options.message)) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::EmptyCommitMessage,
            .exitCode = -1,
            .message = "Il messaggio di commit non può essere vuoto",
            .command = "commit"
        });
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.createCommit(repoPath, options);
}

} // namespace gf::git::bl
