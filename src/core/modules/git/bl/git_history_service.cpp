#include "core/modules/git/bl/git_history_service.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

#include <algorithm>

namespace gf::git::bl {

GitHistoryService::GitHistoryService(ports::IRepositoryGitOps& gitOps) noexcept
    : m_gitOps(gitOps) {}

bool GitHistoryService::isValidTagName(std::string_view name) noexcept {
    if (name.empty() || name == "@") {
        return false;
    }
    if (name.front() == '.' || name.front() == '-' || name.front() == '/') {
        return false;
    }
    if (name.back() == '.' || name.back() == '/') {
        return false;
    }
    if (name.ends_with(".lock")) {
        return false;
    }

    for (char ch : name) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20 || c == 0x7f) {
            return false;
        }
        if (c == ' ' || c == '~' || c == '^' || c == ':' || c == '?' || c == '*' || c == '[' || c == '\\') {
            return false;
        }
    }

    if (name.find("..") != std::string_view::npos) {
        return false;
    }
    if (name.find("//") != std::string_view::npos) {
        return false;
    }
    if (name.find("@{") != std::string_view::npos) {
        return false;
    }

    return true;
}

std::expected<void, ports::GitOpsError>
GitHistoryService::validateRepoPath(const std::filesystem::path& repoPath) const {
    if (repoPath.empty()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
            .exitCode = -1,
            .message = "Il percorso del repository non può essere vuoto",
            .command = "history"
        });
    }

    std::error_code ec;
    if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec)) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::PathNotFound,
            .exitCode = -1,
            .message = "La directory del repository non esiste: " + repoPath.string(),
            .command = "history"
        });
    }

    return {};
}

std::expected<std::vector<bo::GitCommitLogItem>, ports::GitOpsError>
GitHistoryService::getCommitHistory(const std::filesystem::path& repoPath,
                                    const bo::CommitLogOptions& options) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    return m_gitOps.getCommitLog(repoPath, options);
}

std::expected<std::optional<bo::GitCommitLogItem>, ports::GitOpsError>
GitHistoryService::getCommitDetails(const std::filesystem::path& repoPath,
                                    const std::string& commitSha) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    if (commitSha.empty()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidOperation,
            .exitCode = -1,
            .message = "L'hash del commit non può essere vuoto",
            .command = "log"
        });
    }

    bo::CommitLogOptions options;
    options.revisionRange = commitSha;
    options.maxCount = 1;

    auto logResult = m_gitOps.getCommitLog(repoPath, options);
    if (!logResult) {
        return std::unexpected(logResult.error());
    }

    if (logResult->empty()) {
        return std::nullopt;
    }

    return logResult->front();
}

std::expected<std::vector<bo::CommitFileChange>, ports::GitOpsError>
GitHistoryService::getCommitFiles(const std::filesystem::path& repoPath,
                                  const std::string& commitSha) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    if (commitSha.empty()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidOperation,
            .exitCode = -1,
            .message = "L'hash del commit non può essere vuoto",
            .command = "diff-tree"
        });
    }

    return m_gitOps.getCommitFiles(repoPath, commitSha);
}

std::expected<std::vector<std::string>, ports::GitOpsError>
GitHistoryService::listTrackedFiles(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    return m_gitOps.listTrackedFiles(repoPath);
}

std::expected<std::vector<bo::GitTag>, ports::GitOpsError>
GitHistoryService::listTags(const std::filesystem::path& repoPath) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    return m_gitOps.listTags(repoPath);
}

std::expected<void, ports::GitOpsError>
GitHistoryService::createTag(const std::filesystem::path& repoPath,
                             const bo::CreateTagOptions& options) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    if (!isValidTagName(options.name)) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidTagName,
            .exitCode = -1,
            .message = "Nome del tag non valido secondo le specifiche git: '" + options.name + "'",
            .command = "tag"
        });
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.createTag(repoPath, options);
}

std::expected<void, ports::GitOpsError>
GitHistoryService::deleteTag(const std::filesystem::path& repoPath,
                             const std::string& tagName) {
    if (auto valid = validateRepoPath(repoPath); !valid) {
        return std::unexpected(valid.error());
    }

    if (tagName.empty()) {
        return std::unexpected(ports::GitOpsError{
            .kind = ports::GitOpsError::Kind::InvalidTagName,
            .exitCode = -1,
            .message = "Il nome del tag da eliminare non può essere vuoto",
            .command = "tag -d"
        });
    }

    auto lock = RepositoryLockCoordinator::instance().acquire(repoPath);
    return m_gitOps.deleteTag(repoPath, tagName);
}

} // namespace gf::git::bl
