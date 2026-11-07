#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/modules/git/bo/git_command.hpp"

namespace gf::infra::git {

namespace {

[[nodiscard]] gf::git::bo::FileChangeKind mapStatusCharToKind(char code) noexcept {
    switch (code) {
    case 'M': return gf::git::bo::FileChangeKind::Modified;
    case 'A': return gf::git::bo::FileChangeKind::Added;
    case 'D': return gf::git::bo::FileChangeKind::Deleted;
    case 'R': return gf::git::bo::FileChangeKind::Renamed;
    case 'C': return gf::git::bo::FileChangeKind::Copied;
    case '?': return gf::git::bo::FileChangeKind::Untracked;
    case 'U': return gf::git::bo::FileChangeKind::Conflicted;
    default:  return gf::git::bo::FileChangeKind::Modified;
    }
}

} // namespace

NativeRepositoryGitOps::NativeRepositoryGitOps(gf::git::ports::IGitCommandExecutor& executor) noexcept
    : m_executor(executor) {}

std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::getWorkingTreeStatus(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"status", "--porcelain=v2", "-z", "--branch", "--ahead-behind"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire git status: " + execResult.error().message,
            .command = "git status"
        });
    }

    if (execResult->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? "Errore generico in git status" : execResult->stderrBytes,
            .command = "git status"
        });
    }

    auto parseResult = gf::git::bl::PorcelainStatusParser::parse(execResult->stdoutBytes);
    if (!parseResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = 0,
            .message = "Parsing porcelain fallito: " + parseResult.error(),
            .command = "git status"
        });
    }

    gf::git::bo::RepoWorkingTreeStatus workingTree;
    workingTree.branch = std::move(parseResult->branch);

    for (const auto& entry : parseResult->entries) {
        if (entry.isConflict) {
            workingTree.conflictedFiles.push_back(gf::git::bo::WorkingTreeFileItem{
                .path = entry.path,
                .originalPath = entry.originalPath,
                .kind = gf::git::bo::FileChangeKind::Conflicted,
                .statusCode = entry.unstagedStatus != '.' ? entry.unstagedStatus : entry.stagedStatus
            });
            continue;
        }

        if (entry.isUntracked) {
            workingTree.untrackedFiles.push_back(gf::git::bo::WorkingTreeFileItem{
                .path = entry.path,
                .originalPath = {},
                .kind = gf::git::bo::FileChangeKind::Untracked,
                .statusCode = '?'
            });
            continue;
        }

        if (entry.hasStagedChanges()) {
            workingTree.stagedFiles.push_back(gf::git::bo::WorkingTreeFileItem{
                .path = entry.path,
                .originalPath = entry.originalPath,
                .kind = mapStatusCharToKind(entry.stagedStatus),
                .statusCode = entry.stagedStatus
            });
        }

        if (entry.hasUnstagedChanges()) {
            workingTree.unstagedFiles.push_back(gf::git::bo::WorkingTreeFileItem{
                .path = entry.path,
                .originalPath = entry.originalPath,
                .kind = mapStatusCharToKind(entry.unstagedStatus),
                .statusCode = entry.unstagedStatus
            });
        }
    }

    return workingTree;
}

} // namespace gf::infra::git
