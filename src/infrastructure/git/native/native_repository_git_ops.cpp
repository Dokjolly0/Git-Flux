#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "core/modules/git/bl/commit_log_parser.hpp"
#include "core/modules/git/bl/conflict_marker_parser.hpp"
#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/modules/git/bl/unified_diff_parser.hpp"
#include "core/modules/git/bo/git_command.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_map>

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

[[nodiscard]] std::string trimString(std::string str) {
    while (!str.empty() && (str.back() == '\n' || str.back() == '\r' || str.back() == ' ' || str.back() == '\t')) {
        str.pop_back();
    }
    std::size_t start = 0;
    while (start < str.size() && (str[start] == '\n' || str[start] == '\r' || str[start] == ' ' || str[start] == '\t')) {
        ++start;
    }
    return str.substr(start);
}

[[nodiscard]] std::string extractSummaryLine(std::string_view message) {
    auto newlinePos = message.find_first_of("\r\n");
    if (newlinePos == std::string_view::npos) {
        return std::string(message);
    }
    return std::string(message.substr(0, newlinePos));
}

[[nodiscard]] bool isStashNotFoundError(std::string_view output) noexcept {
    return output.find("is not a valid reference") != std::string_view::npos ||
           output.find("is not a valid-object") != std::string_view::npos ||
           output.find("No stash entries found") != std::string_view::npos ||
           output.find("does not exist") != std::string_view::npos ||
           output.find("non è un riferimento valido") != std::string_view::npos ||
           output.find("non è un oggetto valido") != std::string_view::npos ||
           output.find("Nessuna voce di stash trovata") != std::string_view::npos ||
           output.find("non esiste") != std::string_view::npos ||
           (output.find("stash@{") != std::string_view::npos &&
            (output.find("not a valid") != std::string_view::npos ||
             output.find("non è") != std::string_view::npos ||
             output.find("not valid") != std::string_view::npos));
}

void populateRebaseProgressAndConflicts(const std::filesystem::path& repoPath,
                                       gf::git::ports::IGitCommandExecutor& executor,
                                       gf::git::bo::RebaseResult& result) {
    auto readFirstWord = [](const std::filesystem::path& p) -> std::string {
        std::error_code ec;
        if (!std::filesystem::exists(p, ec)) return {};
        std::ifstream ifs(p);
        std::string s;
        if (ifs >> s) return s;
        return {};
    };

    auto rebaseMergeDir = repoPath / ".git" / "rebase-merge";
    auto rebaseApplyDir = repoPath / ".git" / "rebase-apply";

    std::error_code ec;
    if (std::filesystem::exists(rebaseMergeDir, ec)) {
        std::string msgnumStr = readFirstWord(rebaseMergeDir / "msgnum");
        std::string endStr = readFirstWord(rebaseMergeDir / "end");
        std::string stoppedSha = readFirstWord(rebaseMergeDir / "stopped-sha");
        if (!msgnumStr.empty()) {
            try { result.currentStep = std::stoul(msgnumStr); } catch (...) {}
        }
        if (!endStr.empty()) {
            try { result.totalSteps = std::stoul(endStr); } catch (...) {}
        }
        if (!stoppedSha.empty()) {
            result.currentCommitSha = stoppedSha;
        }
    } else if (std::filesystem::exists(rebaseApplyDir, ec)) {
        std::string nextStr = readFirstWord(rebaseApplyDir / "next");
        std::string lastStr = readFirstWord(rebaseApplyDir / "last");
        std::string origCommit = readFirstWord(rebaseApplyDir / "original-commit");
        if (!nextStr.empty()) {
            try { result.currentStep = std::stoul(nextStr); } catch (...) {}
        }
        if (!lastStr.empty()) {
            try { result.totalSteps = std::stoul(lastStr); } catch (...) {}
        }
        if (!origCommit.empty()) {
            result.currentCommitSha = origCommit;
        }
    }

    auto diffUCmd = executor.execute(gf::git::bo::GitCommand{
        .args = {"diff", "--name-only", "--diff-filter=U"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{10'000}
    });

    if (diffUCmd && diffUCmd->exitCode == 0) {
        std::string_view out(diffUCmd->stdoutBytes);
        while (!out.empty()) {
            auto nl = out.find('\n');
            std::string_view line = (nl == std::string_view::npos) ? out : out.substr(0, nl);
            out = (nl == std::string_view::npos) ? std::string_view{} : out.substr(nl + 1);
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            if (!line.empty()) {
                result.conflictedFiles.emplace_back(line);
            }
        }
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
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("index.lock") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::IndexLocked;
        }
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
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

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::stageFiles(const std::filesystem::path& repoPath,
                                   const std::vector<std::string>& filePaths) {
    if (filePaths.empty()) {
        return {};
    }

    std::vector<std::string> args{"add", "--"};
    args.insert(args.end(), filePaths.begin(), filePaths.end());

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire git add: " + execResult.error().message,
            .command = "git add"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("index.lock") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::IndexLocked;
        } else if (execResult->stderrBytes.find("did not match any files") != std::string::npos ||
                   execResult->stderrBytes.find("non corrisponde ad alcun file") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::PathNotFound;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git add"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::stageAll(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"add", "-A"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire git add -A: " + execResult.error().message,
            .command = "git add -A"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("index.lock") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::IndexLocked;
        }
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git add -A"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::unstageFiles(const std::filesystem::path& repoPath,
                                     const std::vector<std::string>& filePaths) {
    if (filePaths.empty()) {
        return {};
    }

    std::vector<std::string> restoreArgs{"restore", "--staged", "--"};
    restoreArgs.insert(restoreArgs.end(), filePaths.begin(), filePaths.end());

    gf::git::bo::GitCommand restoreCmd{
        .args = std::move(restoreArgs),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(restoreCmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire git restore --staged: " + execResult.error().message,
            .command = "git restore --staged"
        });
    }

    // In case of unborn HEAD failure (no commits in repository):
    if (execResult->exitCode != 0) {
        if (execResult->stderrBytes.find("HEAD") != std::string::npos) {
            // Fallback: git rm --cached -- <filePaths>
            std::vector<std::string> rmArgs{"rm", "--cached", "--"};
            rmArgs.insert(rmArgs.end(), filePaths.begin(), filePaths.end());

            gf::git::bo::GitCommand rmCmd{
                .args = std::move(rmArgs),
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                .timeout = std::chrono::milliseconds{30'000}
            };
            auto rmResult = m_executor.execute(rmCmd);
            if (rmResult && rmResult->exitCode == 0) {
                return {};
            }
        }

        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("index.lock") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::IndexLocked;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git restore --staged"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::unstageAll(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand restoreCmd{
        .args = {"restore", "--staged", "."},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(restoreCmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire git restore --staged .: " + execResult.error().message,
            .command = "git restore --staged"
        });
    }

    if (execResult->exitCode != 0) {
        if (execResult->stderrBytes.find("HEAD") != std::string::npos) {
            // Fallback for unborn HEAD: git rm --cached -r -- .
            gf::git::bo::GitCommand rmCmd{
                .args = {"rm", "--cached", "-r", "--", "."},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                .timeout = std::chrono::milliseconds{30'000}
            };
            auto rmResult = m_executor.execute(rmCmd);
            if (rmResult && rmResult->exitCode == 0) {
                return {};
            }
        }

        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("index.lock") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::IndexLocked;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git restore --staged"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::discardFiles(const std::filesystem::path& repoPath,
                                     const std::vector<std::string>& filePaths) {
    if (filePaths.empty()) {
        return {};
    }

    for (const auto& path : filePaths) {
        // 1. Try restoring tracked file (modified or deleted in working tree)
        gf::git::bo::GitCommand restoreCmd{
            .args = {"restore", "--", path},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto restoreRes = m_executor.execute(restoreCmd);
        if (restoreRes && restoreRes->exitCode == 0) {
            continue;
        }

        // 2. If restore fails (untracked file), attempt git clean
        gf::git::bo::GitCommand cleanCmd{
            .args = {"clean", "-f", "-d", "-q", "--", path},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto cleanRes = m_executor.execute(cleanCmd);
        if (cleanRes && cleanRes->exitCode == 0) {
            continue;
        }

        // 3. Fallback to direct filesystem removal if untracked file still exists
        std::error_code fsEc;
        auto targetPath = repoPath / path;
        if (std::filesystem::exists(targetPath, fsEc)) {
            std::filesystem::remove_all(targetPath, fsEc);
            if (!fsEc && !std::filesystem::exists(targetPath, fsEc)) {
                continue;
            }
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = restoreRes ? restoreRes->exitCode : -1,
            .message = "Impossibile scartare le modifiche per: " + path,
            .command = "discard"
        });
    }

    return {};
}

std::expected<gf::git::bo::CommitResult, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::createCommit(const std::filesystem::path& repoPath,
                                     const gf::git::bo::CommitOptions& options) {
    // Fast check for empty staging area if neither amend nor allowEmpty is specified
    if (!options.amend && !options.allowEmpty) {
        gf::git::bo::GitCommand diffCheck{
            .args = {"diff", "--cached", "--quiet"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{10'000}
        };
        auto diffRes = m_executor.execute(diffCheck);
        if (diffRes && diffRes->exitCode == 0) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::NothingToCommit,
                .exitCode = 1,
                .message = "Nessuna modifica nell'area di staging da includere nel commit",
                .command = "git commit"
            });
        }
    }

    std::vector<std::string> args{"commit", "-m", options.message};
    if (options.author.has_value() && options.author->isValid()) {
        args.push_back("--author=" + options.author->name + " <" + options.author->email + ">");
    }
    if (options.amend) {
        args.emplace_back("--amend");
    }
    if (options.allowEmpty) {
        args.emplace_back("--allow-empty");
    }

    gf::git::bo::GitCommand commitCmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(commitCmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire git commit: " + execResult.error().message,
            .command = "git commit"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("index.lock") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::IndexLocked;
        } else if (execResult->stdoutBytes.find("nothing to commit") != std::string::npos ||
                   execResult->stdoutBytes.find("non c'è nulla") != std::string::npos ||
                   execResult->stdoutBytes.find("no changes added") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::NothingToCommit;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
            .command = "git commit"
        });
    }

    // Commit SHA resolution
    gf::git::bo::GitCommand revParseCmd{
        .args = {"rev-parse", "HEAD"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{10'000}
    };
    auto revParseRes = m_executor.execute(revParseCmd);
    std::string commitSha = revParseRes ? trimString(revParseRes->stdoutBytes) : "";
    std::string shortSha = commitSha.size() >= 8 ? commitSha.substr(0, 8) : commitSha;

    // Branch resolution
    gf::git::bo::GitCommand branchCmd{
        .args = {"rev-parse", "--abbrev-ref", "HEAD"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{10'000}
    };
    auto branchRes = m_executor.execute(branchCmd);
    std::string branch = branchRes ? trimString(branchRes->stdoutBytes) : "";

    return gf::git::bo::CommitResult{
        .commitSha = std::move(commitSha),
        .shortSha = std::move(shortSha),
        .summary = extractSummaryLine(options.message),
        .branch = std::move(branch)
    };
}

std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::getFileDiff(const std::filesystem::path& repoPath,
                                    const std::string& filePath,
                                    gf::git::bo::DiffScope scope) {
    std::vector<std::string> args;
    if (scope == gf::git::bo::DiffScope::Staged) {
        args = {"diff", "--cached", "--no-color", "-p", "--", filePath};
    } else if (scope == gf::git::bo::DiffScope::Unstaged) {
        args = {"diff", "--no-color", "-p", "--", filePath};
    } else {
        // Untracked: use --no-index against /dev/null
        args = {"diff", "--no-index", "--no-color", "-p", "--", "/dev/null", filePath};
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire git diff: " + execResult.error().message,
            .command = "git diff"
        });
    }

    // For --no-index, exitCode 1 indicates differences are present
    const bool isNoIndex = (scope == gf::git::bo::DiffScope::Untracked);
    if ((isNoIndex && execResult->exitCode >= 2) || (!isNoIndex && execResult->exitCode > 1)) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git diff"
        });
    }

    if (execResult->stdoutBytes.empty()) {
        return gf::git::bo::FileDiff{
            .oldPath = filePath,
            .newPath = filePath
        };
    }

    auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(execResult->stdoutBytes);
    if (diffOpt.has_value()) {
        if (diffOpt->newPath.empty()) {
            diffOpt->newPath = filePath;
        }
        return std::move(*diffOpt);
    }

    return gf::git::bo::FileDiff{
        .oldPath = filePath,
        .newPath = filePath
    };
}

std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::getAllDiffs(const std::filesystem::path& repoPath,
                                    gf::git::bo::DiffScope scope) {
    if (scope == gf::git::bo::DiffScope::Untracked) {
        // For untracked scope, list untracked files and get diff for each
        auto statusRes = getWorkingTreeStatus(repoPath);
        if (!statusRes) {
            return std::unexpected(statusRes.error());
        }

        std::vector<gf::git::bo::FileDiff> result;
        result.reserve(statusRes->untrackedFiles.size());
        for (const auto& item : statusRes->untrackedFiles) {
            auto diff = getFileDiff(repoPath, item.path, gf::git::bo::DiffScope::Untracked);
            if (diff) {
                result.push_back(std::move(*diff));
            }
        }
        return result;
    }

    std::vector<std::string> args;
    if (scope == gf::git::bo::DiffScope::Staged) {
        args = {"diff", "--cached", "--no-color", "-p"};
    } else {
        args = {"diff", "--no-color", "-p"};
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire git diff: " + execResult.error().message,
            .command = "git diff"
        });
    }

    if (execResult->exitCode > 1) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git diff"
        });
    }

    return gf::git::bl::UnifiedDiffParser::parse(execResult->stdoutBytes);
}

std::expected<std::vector<gf::git::bo::CommitFileChange>, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::getCommitFiles(const std::filesystem::path& repoPath,
                                       const std::string& commitSha) {
    if (commitSha.empty()) {
        return std::vector<gf::git::bo::CommitFileChange>{};
    }

    // 1. Run git show -m --first-parent --name-status --format="" <commitSha>
    gf::git::bo::GitCommand diffTreeCmd{
        .args = {"show", "-m", "--first-parent", "--name-status", "--format=", commitSha},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto diffTreeRes = m_executor.execute(diffTreeCmd);
    if (!diffTreeRes || diffTreeRes->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = diffTreeRes ? diffTreeRes->exitCode : -1,
            .message = diffTreeRes ? diffTreeRes->stderrBytes : "Execution failed",
            .command = "git show"
        });
    }

    // 2. Run git show -m --first-parent --numstat --format="" <commitSha>
    gf::git::bo::GitCommand numstatCmd{
        .args = {"show", "-m", "--first-parent", "--numstat", "--format=", commitSha},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };
    auto numstatRes = m_executor.execute(numstatCmd);

    struct NumStatEntry {
        int additions = 0;
        int deletions = 0;
        bool isBinary = false;
    };
    std::unordered_map<std::string, NumStatEntry> numstatMap;

    if (numstatRes && numstatRes->exitCode == 0) {
        std::istringstream nss(numstatRes->stdoutBytes);
        std::string nline;
        while (std::getline(nss, nline)) {
            while (!nline.empty() && (nline.back() == '\r' || nline.back() == ' ')) {
                nline.pop_back();
            }
            if (nline.empty()) continue;

            auto tab1 = nline.find('\t');
            if (tab1 == std::string::npos) continue;
            auto tab2 = nline.find('\t', tab1 + 1);
            if (tab2 == std::string::npos) continue;

            std::string addStr = nline.substr(0, tab1);
            std::string delStr = nline.substr(tab1 + 1, tab2 - tab1 - 1);
            std::string pathStr = nline.substr(tab2 + 1);

            NumStatEntry entry;
            if (addStr == "-" || delStr == "-") {
                entry.isBinary = true;
            } else {
                try {
                    entry.additions = std::stoi(addStr);
                    entry.deletions = std::stoi(delStr);
                } catch (...) {
                    entry.additions = 0;
                    entry.deletions = 0;
                }
            }
            numstatMap[pathStr] = entry;
        }
    }

    // Parse diff-tree output
    std::vector<gf::git::bo::CommitFileChange> changes;
    std::istringstream dss(diffTreeRes->stdoutBytes);
    std::string dline;
    while (std::getline(dss, dline)) {
        while (!dline.empty() && (dline.back() == '\r' || dline.back() == ' ')) {
            dline.pop_back();
        }
        if (dline.empty()) continue;

        auto tab1 = dline.find('\t');
        if (tab1 == std::string::npos) continue;

        std::string statusStr = dline.substr(0, tab1);
        std::string rest = dline.substr(tab1 + 1);

        gf::git::bo::CommitFileChange change;
        char sChar = statusStr.empty() ? 'M' : statusStr.front();
        switch (sChar) {
        case 'A': change.status = gf::git::bo::FileChangeStatus::Added; break;
        case 'D': change.status = gf::git::bo::FileChangeStatus::Deleted; break;
        case 'R': change.status = gf::git::bo::FileChangeStatus::Renamed; break;
        case 'C': change.status = gf::git::bo::FileChangeStatus::Copied; break;
        default:  change.status = gf::git::bo::FileChangeStatus::Modified; break;
        }

        if (change.status == gf::git::bo::FileChangeStatus::Renamed ||
            change.status == gf::git::bo::FileChangeStatus::Copied) {
            auto tab2 = rest.find('\t');
            if (tab2 != std::string::npos) {
                change.oldPath = rest.substr(0, tab2);
                change.path = rest.substr(tab2 + 1);
            } else {
                change.path = rest;
            }
        } else {
            change.path = rest;
        }

        // Match numstat
        auto it = numstatMap.find(change.path);
        if (it != numstatMap.end()) {
            change.additions = it->second.additions;
            change.deletions = it->second.deletions;
            change.isBinary = it->second.isBinary;
        }

        changes.push_back(std::move(change));
    }

    return changes;
}

std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::getCommitFileDiff(const std::filesystem::path& repoPath,
                                          const std::string& commitSha,
                                          const std::string& filePath) {
    if (commitSha.empty() || filePath.empty()) {
        return gf::git::bo::FileDiff{.oldPath = filePath, .newPath = filePath};
    }

    gf::git::bo::GitCommand cmd{
        .args = {"show", "-m", "--first-parent", "--format=", "--no-color", "-p", commitSha, "--", filePath},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult || execResult->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult ? execResult->exitCode : -1,
            .message = execResult ? execResult->stderrBytes : "Execution failed",
            .command = "git show"
        });
    }

    if (execResult->stdoutBytes.empty()) {
        return gf::git::bo::FileDiff{
            .oldPath = filePath,
            .newPath = filePath
        };
    }

    auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(execResult->stdoutBytes);
    if (diffOpt.has_value()) {
        if (diffOpt->newPath.empty()) {
            diffOpt->newPath = filePath;
        }
        if (diffOpt->oldPath.empty()) {
            diffOpt->oldPath = filePath;
        }
        return std::move(*diffOpt);
    }

    return gf::git::bo::FileDiff{
        .oldPath = filePath,
        .newPath = filePath
    };
}

std::expected<std::vector<gf::git::bo::BlameLine>, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::getFileBlame(const std::filesystem::path& repoPath,
                                     const std::string& revision,
                                     const std::string& filePath) {
    if (filePath.empty()) {
        return std::vector<gf::git::bo::BlameLine>{};
    }

    std::vector<std::string> args = {"blame", "--line-porcelain"};
    if (!revision.empty()) {
        args.push_back(revision);
    }
    args.push_back("--");
    args.push_back(filePath);

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult || execResult->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult ? execResult->exitCode : -1,
            .message = execResult ? execResult->stderrBytes : "Execution failed",
            .command = "git blame"
        });
    }

    std::vector<gf::git::bo::BlameLine> result;
    std::istringstream stream(execResult->stdoutBytes);
    std::string line;

    std::string currentSha;
    std::string currentAuthor;
    std::string currentEmail;
    std::string currentDate;
    int currentLineNum = 1;

    while (std::getline(stream, line)) {
        while (!line.empty() && (line.back() == '\r')) {
            line.pop_back();
        }

        if (line.starts_with('\t')) {
            // Source content line ends the block
            gf::git::bo::BlameLine bLine;
            bLine.commitSha = currentSha;
            bLine.shortSha = currentSha.size() >= 8 ? currentSha.substr(0, 8) : currentSha;
            bLine.author = currentAuthor;
            bLine.authorEmail = currentEmail;
            bLine.date = currentDate;
            bLine.lineNumber = currentLineNum;
            bLine.content = line.substr(1);
            result.push_back(std::move(bLine));
            continue;
        }

        if (line.starts_with("author ")) {
            currentAuthor = line.substr(7);
        } else if (line.starts_with("author-mail ")) {
            std::string mail = line.substr(12);
            if (!mail.empty() && mail.front() == '<') mail = mail.substr(1);
            if (!mail.empty() && mail.back() == '>') mail.pop_back();
            currentEmail = std::move(mail);
        } else if (line.starts_with("author-time ")) {
            try {
                long long epoch = std::stoll(line.substr(12));
                std::time_t t = static_cast<std::time_t>(epoch);
                std::tm tmBuf{};
                gmtime_r(&t, &tmBuf);
                char dateBuf[32];
                std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tmBuf);
                currentDate = dateBuf;
            } catch (...) {
                currentDate.clear();
            }
        } else {
            // Check for header line: "<sha> <orig-line> <final-line>"
            auto firstSpace = line.find(' ');
            if (firstSpace == 40) { // 40-character sha
                currentSha = line.substr(0, 40);
                auto secondSpace = line.find(' ', firstSpace + 1);
                if (secondSpace != std::string::npos) {
                    auto thirdSpace = line.find(' ', secondSpace + 1);
                    std::string fLineStr = (thirdSpace != std::string::npos)
                        ? line.substr(secondSpace + 1, thirdSpace - secondSpace - 1)
                        : line.substr(secondSpace + 1);
                    try {
                        currentLineNum = std::stoi(fLineStr);
                    } catch (...) {
                        currentLineNum = 1;
                    }
                }
            }
        }
    }

    return result;
}

std::expected<std::vector<gf::git::bo::GitBranch>, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::listBranches(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"for-each-ref", "--format=%(refname)%09%(objectname)%09%(HEAD)%09%(upstream:short)%09%(upstream:track)", "refs/heads", "refs/remotes"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile recuperare i branch: " + execResult.error().message,
            .command = "git for-each-ref"
        });
    }

    if (execResult->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git for-each-ref"
        });
    }

    std::vector<gf::git::bo::GitBranch> branches;
    std::string_view output = execResult->stdoutBytes;

    std::size_t startPos = 0;
    while (startPos < output.size()) {
        auto endPos = output.find('\n', startPos);
        std::string_view line = (endPos == std::string_view::npos)
                                    ? output.substr(startPos)
                                    : output.substr(startPos, endPos - startPos);
        startPos = (endPos == std::string_view::npos) ? output.size() : endPos + 1;

        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.remove_suffix(1);
        }
        if (line.empty()) {
            continue;
        }

        std::vector<std::string_view> fields;
        std::size_t fStart = 0;
        while (fStart < line.size()) {
            auto fEnd = line.find('\t', fStart);
            if (fEnd == std::string_view::npos) {
                fields.push_back(line.substr(fStart));
                break;
            }
            fields.push_back(line.substr(fStart, fEnd - fStart));
            fStart = fEnd + 1;
        }

        if (fields.size() < 2) {
            continue;
        }

        std::string_view refname = fields[0];
        std::string_view objname = fields[1];
        std::string_view headMark = fields.size() > 2 ? fields[2] : "";
        std::string_view upstream = fields.size() > 3 ? fields[3] : "";
        std::string_view track = fields.size() > 4 ? fields[4] : "";

        gf::git::bo::GitBranch b;
        b.fullName = std::string(refname);
        b.targetCommitSha = std::string(objname);
        b.isCurrent = (headMark.find('*') != std::string_view::npos);
        b.upstream = std::string(upstream);

        if (refname.starts_with("refs/heads/")) {
            b.type = gf::git::bo::BranchType::Local;
            b.name = std::string(refname.substr(11));
            b.shortName = b.name;
        } else if (refname.starts_with("refs/remotes/")) {
            b.type = gf::git::bo::BranchType::Remote;
            b.name = std::string(refname.substr(13));
            auto slash = b.name.find('/');
            b.shortName = (slash != std::string::npos) ? b.name.substr(slash + 1) : b.name;
            if (b.shortName == "HEAD") {
                continue;
            }
        } else {
            b.type = gf::git::bo::BranchType::Local;
            b.name = std::string(refname);
            b.shortName = b.name;
        }

        // Parse ahead/behind from tracking descriptor (e.g. "[ahead 2, behind 1]")
        if (!track.empty()) {
            auto aheadPos = track.find("ahead ");
            if (aheadPos != std::string_view::npos) {
                int val = 0;
                std::size_t p = aheadPos + 6;
                while (p < track.size() && track[p] >= '0' && track[p] <= '9') {
                    val = val * 10 + (track[p] - '0');
                    ++p;
                }
                b.ahead = val;
            }

            auto behindPos = track.find("behind ");
            if (behindPos != std::string_view::npos) {
                int val = 0;
                std::size_t p = behindPos + 7;
                while (p < track.size() && track[p] >= '0' && track[p] <= '9') {
                    val = val * 10 + (track[p] - '0');
                    ++p;
                }
                b.behind = val;
            }
        }

        branches.push_back(std::move(b));
    }

    // If branch list is empty (e.g. freshly initialized repository with unborn HEAD)
    if (branches.empty()) {
        gf::git::bo::GitCommand branchCheckCmd{
            .args = {"rev-parse", "--abbrev-ref", "HEAD"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{10'000}
        };
        auto bcRes = m_executor.execute(branchCheckCmd);
        if (bcRes && bcRes->exitCode == 0) {
            std::string currentHead = trimString(bcRes->stdoutBytes);
            if (!currentHead.empty() && currentHead != "HEAD") {
                branches.push_back(gf::git::bo::GitBranch{
                    .name = currentHead,
                    .shortName = currentHead,
                    .fullName = "refs/heads/" + currentHead,
                    .targetCommitSha = {},
                    .upstream = {},
                    .ahead = 0,
                    .behind = 0,
                    .isCurrent = true,
                    .isDetached = false,
                    .type = gf::git::bo::BranchType::Local
                });
            }
        }
    }

    return branches;
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::createBranch(const std::filesystem::path& repoPath,
                                     const gf::git::bo::CreateBranchOptions& options) {
    std::vector<std::string> args;
    if (options.checkoutAfterCreate) {
        args = {"checkout", "-b", options.name};
    } else {
        args = {"branch", options.name};
    }

    if (options.startPoint.has_value() && !options.startPoint->empty()) {
        args.push_back(*options.startPoint);
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile creare il branch: " + execResult.error().message,
            .command = "git branch"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("already exists") != std::string::npos ||
            execResult->stderrBytes.find("esiste già") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::BranchAlreadyExists;
        } else if (execResult->stderrBytes.find("not found") != std::string::npos ||
                   execResult->stderrBytes.find("non trovato") != std::string::npos ||
                   execResult->stderrBytes.find("No branch named") != std::string::npos ||
                   execResult->stderrBytes.find("Nessun branch denominato") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git branch"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::checkoutBranch(const std::filesystem::path& repoPath,
                                       const gf::git::bo::CheckoutOptions& options) {
    std::vector<std::string> args{"checkout"};
    if (options.createNewBranch) {
        args.emplace_back("-b");
    }
    if (options.force) {
        args.emplace_back("-f");
    }
    args.push_back(options.branchOrCommit);

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire il checkout: " + execResult.error().message,
            .command = "git checkout"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("would be overwritten by checkout") != std::string::npos ||
            execResult->stderrBytes.find("sarebbero sovrascritte con il checkout") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::DirtyWorkingTree;
        } else if (execResult->stderrBytes.find("did not match any") != std::string::npos ||
                   execResult->stderrBytes.find("non corrisponde ad alcun") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
        } else if (execResult->stderrBytes.find("already exists") != std::string::npos ||
                   execResult->stderrBytes.find("esiste già") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::BranchAlreadyExists;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git checkout"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::deleteBranch(const std::filesystem::path& repoPath,
                                     const std::string& branchName,
                                     bool force) {
    gf::git::bo::GitCommand cmd{
        .args = {"branch", force ? "-D" : "-d", branchName},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eliminare il branch: " + execResult.error().message,
            .command = "git branch -d"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("cannot delete") != std::string::npos ||
            execResult->stderrBytes.find("impossibile eliminare") != std::string::npos ||
            execResult->stderrBytes.find("not fully merged") != std::string::npos ||
            execResult->stderrBytes.find("sottoposto a merge") != std::string::npos ||
            execResult->stderrBytes.find("used by worktree") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::InvalidOperation;
        } else if (execResult->stderrBytes.find("not found") != std::string::npos ||
                   execResult->stderrBytes.find("non trovato") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git branch -d"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::renameBranch(const std::filesystem::path& repoPath,
                                     const std::string& oldName,
                                     const std::string& newName) {
    gf::git::bo::GitCommand cmd{
        .args = {"branch", "-m", oldName, newName},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile rinominare il branch: " + execResult.error().message,
            .command = "git branch -m"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("already exists") != std::string::npos ||
            execResult->stderrBytes.find("esiste già") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::BranchAlreadyExists;
        } else if (execResult->stderrBytes.find("not found") != std::string::npos ||
                   execResult->stderrBytes.find("non trovato") != std::string::npos ||
                   execResult->stderrBytes.find("No branch named") != std::string::npos ||
                   execResult->stderrBytes.find("Nessun branch denominato") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git branch -m"
        });
    }

    return {};
}

std::expected<std::vector<gf::git::bo::GitCommitLogItem>, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::getCommitLog(const std::filesystem::path& repoPath,
                                     const gf::git::bo::CommitLogOptions& options) {
    std::vector<std::string> args{
        "log",
        "--format=%H%x00%h%x00%P%x00%an%x00%ae%x00%aI%x00%cn%x00%ce%x00%cI%x00%D%x00%s%x00%b%x1e"
    };

    if (options.topoOrder) {
        args.emplace_back("--topo-order");
    }

    if (options.maxCount > 0) {
        args.emplace_back("-n");
        args.push_back(std::to_string(options.maxCount));
    }

    if (options.skip > 0) {
        args.push_back("--skip=" + std::to_string(options.skip));
    }

    if (options.followRenames && options.pathFilter.has_value() && !options.pathFilter->empty()) {
        args.emplace_back("--follow");
    }

    if (options.revisionRange.has_value() && !options.revisionRange->empty()) {
        args.push_back(*options.revisionRange);
    }

    if (options.pathFilter.has_value() && !options.pathFilter->empty()) {
        args.emplace_back("--");
        args.push_back(*options.pathFilter);
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile recuperare la cronologia commit: " + execResult.error().message,
            .command = "git log"
        });
    }

    if (execResult->exitCode != 0) {
        if (execResult->stderrBytes.find("does not have any commits yet") != std::string::npos ||
            execResult->stderrBytes.find("non ha ancora alcun commit") != std::string::npos ||
            execResult->stderrBytes.find("unknown revision or path not in the working tree") != std::string::npos) {
            return std::vector<gf::git::bo::GitCommitLogItem>{};
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git log"
        });
    }

    return gf::git::bl::CommitLogParser::parse(execResult->stdoutBytes);
}

std::expected<std::vector<gf::git::bo::GitTag>, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::listTags(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {
            "tag",
            "-l",
            "--format=%(refname:strip=2)%09%(objectname)%09%(*objectname)%09%(taggername)%09%(taggeremail)%09%(taggerdate:iso-strict)%09%(contents:subject)"
        },
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile recuperare i tag: " + execResult.error().message,
            .command = "git tag -l"
        });
    }

    if (execResult->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git tag -l"
        });
    }

    std::vector<gf::git::bo::GitTag> tags;
    std::string_view out(execResult->stdoutBytes);

    while (!out.empty()) {
        auto nextLine = out.find('\n');
        std::string_view line = (nextLine == std::string_view::npos) ? out : out.substr(0, nextLine);
        out = (nextLine == std::string_view::npos) ? std::string_view{} : out.substr(nextLine + 1);

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.empty()) {
            continue;
        }

        std::vector<std::string_view> fields;
        std::size_t start = 0;
        while (start <= line.size()) {
            auto tab = line.find('\t', start);
            if (tab == std::string_view::npos) {
                fields.push_back(line.substr(start));
                break;
            }
            fields.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }

        if (fields.size() < 7) {
            continue;
        }

        gf::git::bo::GitTag tag;
        tag.name = std::string(fields[0]);
        auto objName = fields[1];
        auto peeledObj = fields[2];
        tag.taggerName = std::string(fields[3]);

        auto emailStr = std::string(fields[4]);
        if (emailStr.starts_with('<') && emailStr.ends_with('>') && emailStr.size() >= 2) {
            emailStr = emailStr.substr(1, emailStr.size() - 2);
        }
        tag.taggerEmail = std::move(emailStr);

        tag.tagDateIso = std::string(fields[5]);
        tag.message = std::string(fields[6]);

        if (!peeledObj.empty()) {
            tag.type = gf::git::bo::TagType::Annotated;
            tag.targetSha = std::string(peeledObj);
        } else {
            tag.type = gf::git::bo::TagType::Lightweight;
            tag.targetSha = std::string(objName);
        }

        tags.push_back(std::move(tag));
    }

    return tags;
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::createTag(const std::filesystem::path& repoPath,
                                  const gf::git::bo::CreateTagOptions& options) {
    std::vector<std::string> args{"tag"};
    if (options.force) {
        args.emplace_back("-f");
    }

    if (options.message.has_value()) {
        args.emplace_back("-a");
        args.push_back(options.name);
        args.emplace_back("-m");
        args.push_back(*options.message);
    } else {
        args.push_back(options.name);
    }

    if (options.targetCommit.has_value() && !options.targetCommit->empty()) {
        args.push_back(*options.targetCommit);
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile creare il tag: " + execResult.error().message,
            .command = "git tag"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("already exists") != std::string::npos ||
            execResult->stderrBytes.find("esiste già") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::TagAlreadyExists;
        } else if (execResult->stderrBytes.find("not a valid object name") != std::string::npos ||
                   execResult->stderrBytes.find("non è un nome di oggetto valido") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git tag"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::deleteTag(const std::filesystem::path& repoPath,
                                  const std::string& tagName) {
    gf::git::bo::GitCommand cmd{
        .args = {"tag", "-d", tagName},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eliminare il tag: " + execResult.error().message,
            .command = "git tag -d"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("not found") != std::string::npos ||
            execResult->stderrBytes.find("non trovato") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::TagNotFound;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git tag -d"
        });
    }

    return {};
}

std::expected<std::vector<gf::git::bo::GitRemote>, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::listRemotes(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"remote", "-v"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile recuperare i remote: " + execResult.error().message,
            .command = "git remote -v"
        });
    }

    if (execResult->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git remote -v"
        });
    }

    std::unordered_map<std::string, gf::git::bo::GitRemote> remotesMap;
    std::string_view out(execResult->stdoutBytes);

    while (!out.empty()) {
        auto nextLine = out.find('\n');
        std::string_view line = (nextLine == std::string_view::npos) ? out : out.substr(0, nextLine);
        out = (nextLine == std::string_view::npos) ? std::string_view{} : out.substr(nextLine + 1);

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.empty()) {
            continue;
        }

        auto firstSep = line.find_first_of(" \t");
        if (firstSep == std::string_view::npos) {
            continue;
        }

        auto name = std::string(line.substr(0, firstSep));
        auto rest = line.substr(firstSep);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.remove_prefix(1);
        }

        auto lastSep = rest.rfind(" (");
        std::string url;
        bool isPush = false;
        if (lastSep != std::string_view::npos) {
            url = std::string(rest.substr(0, lastSep));
            if (rest.find("(push)") != std::string_view::npos) {
                isPush = true;
            }
        } else {
            url = std::string(rest);
        }

        auto& remote = remotesMap[name];
        remote.name = name;
        if (isPush) {
            remote.pushUrl = url;
            if (remote.fetchUrl.empty()) {
                remote.fetchUrl = url;
            }
        } else {
            remote.fetchUrl = url;
            if (remote.pushUrl.empty()) {
                remote.pushUrl = url;
            }
        }
    }

    std::vector<gf::git::bo::GitRemote> remotes;
    remotes.reserve(remotesMap.size());
    for (auto& [_, r] : remotesMap) {
        remotes.push_back(std::move(r));
    }
    std::sort(remotes.begin(), remotes.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });

    return remotes;
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::addRemote(const std::filesystem::path& repoPath,
                                  const std::string& name,
                                  const std::string& url) {
    gf::git::bo::GitCommand cmd{
        .args = {"remote", "add", name, url},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile aggiungere il remote: " + execResult.error().message,
            .command = "git remote add"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("already exists") != std::string::npos ||
            execResult->stderrBytes.find("esiste già") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::RemoteAlreadyExists;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git remote add"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::removeRemote(const std::filesystem::path& repoPath,
                                     const std::string& name) {
    gf::git::bo::GitCommand cmd{
        .args = {"remote", "remove", name},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile rimuovere il remote: " + execResult.error().message,
            .command = "git remote remove"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("No such remote") != std::string::npos ||
            execResult->stderrBytes.find("non trovato") != std::string::npos ||
            execResult->stderrBytes.find("non esiste") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::RemoteNotFound;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git remote remove"
        });
    }

    return {};
}

std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::fetchRemote(const std::filesystem::path& repoPath,
                                    const gf::git::bo::RemoteOperationOptions& options) {
    std::vector<std::string> args{"fetch"};
    if (options.prune) {
        args.emplace_back("--prune");
    }
    if (options.tags) {
        args.emplace_back("--tags");
    }
    if (options.force) {
        args.emplace_back("--force");
    }
    args.push_back(options.remoteName);
    if (options.branchName.has_value() && !options.branchName->empty()) {
        args.push_back(*options.branchName);
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{60'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire il fetch: " + execResult.error().message,
            .command = "git fetch"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("Could not resolve host") != std::string::npos ||
            execResult->stderrBytes.find("Authentication failed") != std::string::npos ||
            execResult->stderrBytes.find("Permission denied") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::NetworkAuthenticationFailed;
        } else if (execResult->stderrBytes.find("does not appear to be a git repository") != std::string::npos ||
                   execResult->stderrBytes.find("not found") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::RemoteNotFound;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git fetch"
        });
    }

    gf::git::bo::RemoteOperationResult result;
    result.success = true;
    result.output = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes;
    return result;
}

std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::pullRemote(const std::filesystem::path& repoPath,
                                   const gf::git::bo::RemoteOperationOptions& options) {
    std::vector<std::string> args{"pull"};
    args.push_back(options.remoteName);
    if (options.branchName.has_value() && !options.branchName->empty()) {
        args.push_back(*options.branchName);
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{60'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire il pull: " + execResult.error().message,
            .command = "git pull"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("Automatic merge failed") != std::string::npos ||
            execResult->stdoutBytes.find("Automatic merge failed") != std::string::npos ||
            execResult->stdoutBytes.find("CONFLICT") != std::string::npos ||
            execResult->stderrBytes.find("conflitti") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::MergeConflict;
        } else if (execResult->stderrBytes.find("Authentication failed") != std::string::npos ||
                   execResult->stderrBytes.find("Permission denied") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::NetworkAuthenticationFailed;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
            .command = "git pull"
        });
    }

    gf::git::bo::RemoteOperationResult result;
    result.success = true;
    result.output = execResult->stdoutBytes;
    return result;
}

std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::pushRemote(const std::filesystem::path& repoPath,
                                   const gf::git::bo::RemoteOperationOptions& options) {
    std::vector<std::string> args{"push"};
    if (options.setUpstream) {
        args.emplace_back("-u");
    }
    if (options.force) {
        args.emplace_back("--force-with-lease");
    }
    if (options.tags) {
        args.emplace_back("--tags");
    }
    args.push_back(options.remoteName);
    if (options.branchName.has_value() && !options.branchName->empty()) {
        args.push_back(*options.branchName);
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{60'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire il push: " + execResult.error().message,
            .command = "git push"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("rejected") != std::string::npos ||
            execResult->stderrBytes.find("non-fast-forward") != std::string::npos ||
            execResult->stderrBytes.find("fetch first") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::RemoteRejected;
        } else if (execResult->stderrBytes.find("Authentication failed") != std::string::npos ||
                   execResult->stderrBytes.find("Permission denied") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::NetworkAuthenticationFailed;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git push"
        });
    }

    gf::git::bo::RemoteOperationResult result;
    result.success = true;
    result.output = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes;
    return result;
}

std::expected<gf::git::bo::MergeResult, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::mergeBranch(const std::filesystem::path& repoPath,
                                    const gf::git::bo::MergeOptions& options) {
    std::vector<std::string> args{"merge"};
    if (options.noFastForward) {
        args.emplace_back("--no-ff");
    }
    if (options.squash) {
        args.emplace_back("--squash");
    }
    if (options.message.has_value() && !options.message->empty()) {
        args.emplace_back("-m");
        args.push_back(*options.message);
    }
    args.push_back(options.sourceBranchOrCommit);

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire il merge: " + execResult.error().message,
            .command = "git merge"
        });
    }

    gf::git::bo::MergeResult result;
    const auto combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

    if (execResult->exitCode == 0) {
        result.summaryMessage = execResult->stdoutBytes;
        if (combinedOutput.find("Already up to date") != std::string::npos ||
            combinedOutput.find("È già aggiornato") != std::string::npos) {
            result.status = gf::git::bo::MergeStatus::UpToDate;
        } else if (combinedOutput.find("Fast-forward") != std::string::npos) {
            result.status = gf::git::bo::MergeStatus::FastForward;
        } else {
            result.status = gf::git::bo::MergeStatus::Merged;
        }

        auto revCmd = m_executor.execute(gf::git::bo::GitCommand{
            .args = {"rev-parse", "HEAD"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{5'000}
        });
        if (revCmd && revCmd->exitCode == 0) {
            result.targetCommitSha = trimString(revCmd->stdoutBytes);
        }

        return result;
    }

    if (combinedOutput.find("CONFLICT") != std::string::npos ||
        combinedOutput.find("Automatic merge failed") != std::string::npos ||
        combinedOutput.find("conflitti") != std::string::npos) {
        result.status = gf::git::bo::MergeStatus::Conflict;
        result.summaryMessage = execResult->stdoutBytes;

        auto diffUCmd = m_executor.execute(gf::git::bo::GitCommand{
            .args = {"diff", "--name-only", "--diff-filter=U"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{10'000}
        });

        if (diffUCmd && diffUCmd->exitCode == 0) {
            std::string_view out(diffUCmd->stdoutBytes);
            while (!out.empty()) {
                auto nl = out.find('\n');
                std::string_view line = (nl == std::string_view::npos) ? out : out.substr(0, nl);
                out = (nl == std::string_view::npos) ? std::string_view{} : out.substr(nl + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }
                if (!line.empty()) {
                    result.conflictedFiles.emplace_back(line);
                }
            }
        }

        return result;
    }

    return std::unexpected(gf::git::ports::GitOpsError{
        .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
        .exitCode = execResult->exitCode,
        .message = execResult->stderrBytes,
        .command = "git merge"
    });
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::abortMerge(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"merge", "--abort"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile annullare il merge: " + execResult.error().message,
            .command = "git merge --abort"
        });
    }

    if (execResult->exitCode != 0) {
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (execResult->stderrBytes.find("no merge to abort") != std::string::npos ||
            execResult->stderrBytes.find("nessun merge da annullare") != std::string::npos ||
            execResult->stderrBytes.find("MERGE_HEAD missing") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::InvalidOperation;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git merge --abort"
        });
    }

    return {};
}

std::expected<std::vector<gf::git::bo::GitStashItem>, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::listStashes(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"stash", "list", "--format=%gd%x00%H%x00%aI%x00%gs%x1e"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile recuperare l'elenco degli stash: " + execResult.error().message,
            .command = "git stash list"
        });
    }

    if (execResult->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git stash list"
        });
    }

    std::vector<gf::git::bo::GitStashItem> stashes;
    std::string_view raw = execResult->stdoutBytes;

    std::size_t startPos = 0;
    while (startPos < raw.size()) {
        auto endPos = raw.find('\x1e', startPos);
        std::string_view record = (endPos == std::string_view::npos)
                                      ? raw.substr(startPos)
                                      : raw.substr(startPos, endPos - startPos);

        if (!record.empty()) {
            std::vector<std::string_view> fields;
            std::size_t fStart = 0;
            while (fStart < record.size()) {
                auto fEnd = record.find('\0', fStart);
                if (fEnd == std::string_view::npos) {
                    fields.push_back(record.substr(fStart));
                    break;
                }
                fields.push_back(record.substr(fStart, fEnd - fStart));
                fStart = fEnd + 1;
            }

            if (fields.size() >= 4) {
                gf::git::bo::GitStashItem item;
                item.selector = std::string(fields[0]);
                item.commitSha = std::string(fields[1]);
                item.dateIso = std::string(fields[2]);

                auto openBrace = item.selector.find('{');
                auto closeBrace = item.selector.find('}', openBrace);
                if (openBrace != std::string::npos && closeBrace != std::string::npos && closeBrace > openBrace + 1) {
                    auto numStr = item.selector.substr(openBrace + 1, closeBrace - openBrace - 1);
                    try {
                        item.index = std::stoul(numStr);
                    } catch (...) {
                        item.index = 0;
                    }
                }

                std::string_view subject = fields[3];
                constexpr std::string_view wipPrefix = "WIP on ";
                constexpr std::string_view onPrefix = "On ";

                if (subject.starts_with(wipPrefix)) {
                    auto rest = subject.substr(wipPrefix.size());
                    auto colonPos = rest.find(':');
                    if (colonPos != std::string_view::npos) {
                        item.branchName = trimString(std::string(rest.substr(0, colonPos)));
                        item.message = trimString(std::string(rest.substr(colonPos + 1)));
                    } else {
                        item.branchName = trimString(std::string(rest));
                        item.message = std::string(subject);
                    }
                } else if (subject.starts_with(onPrefix)) {
                    auto rest = subject.substr(onPrefix.size());
                    auto colonPos = rest.find(':');
                    if (colonPos != std::string_view::npos) {
                        item.branchName = trimString(std::string(rest.substr(0, colonPos)));
                        item.message = trimString(std::string(rest.substr(colonPos + 1)));
                    } else {
                        item.branchName = trimString(std::string(rest));
                        item.message = std::string(subject);
                    }
                } else {
                    item.message = std::string(subject);
                }

                stashes.push_back(std::move(item));
            }
        }

        if (endPos == std::string_view::npos) {
            break;
        }
        startPos = endPos + 1;
    }

    return stashes;
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::stashPush(const std::filesystem::path& repoPath,
                                  const gf::git::bo::StashPushOptions& options) {
    std::vector<std::string> args = {"stash", "push"};
    if (options.includeUntracked) {
        args.emplace_back("-u");
    }
    if (options.keepIndex) {
        args.emplace_back("--keep-index");
    }
    if (options.message && !options.message->empty()) {
        args.emplace_back("-m");
        args.emplace_back(*options.message);
    }
    if (!options.paths.empty()) {
        args.emplace_back("--");
        for (const auto& path : options.paths) {
            args.emplace_back(path);
        }
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile salvare lo stash: " + execResult.error().message,
            .command = "git stash push"
        });
    }

    const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
    if (combinedOutput.find("No local changes to save") != std::string::npos ||
        combinedOutput.find("Nessuna modifica locale da salvare") != std::string::npos) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::NothingToStash,
            .exitCode = execResult->exitCode,
            .message = "Nessuna modifica locale da salvare nello stash",
            .command = "git stash push"
        });
    }

    if (execResult->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
            .command = "git stash push"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::stashPop(const std::filesystem::path& repoPath,
                                 const gf::git::bo::StashApplyOptions& options) {
    std::vector<std::string> args = {"stash", "pop"};
    if (options.restoreIndex) {
        args.emplace_back("--index");
    }
    args.emplace_back("stash@{" + std::to_string(options.stashIndex) + "}");

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile applicare lo stash: " + execResult.error().message,
            .command = "git stash pop"
        });
    }

    if (execResult->exitCode != 0) {
        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;

        if (isStashNotFoundError(combinedOutput)) {
            kind = gf::git::ports::GitOpsError::Kind::StashNotFound;
        } else if (combinedOutput.find("CONFLICT") != std::string::npos ||
                   combinedOutput.find("conflict") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::MergeConflict;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
            .command = "git stash pop"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::stashApply(const std::filesystem::path& repoPath,
                                   const gf::git::bo::StashApplyOptions& options) {
    std::vector<std::string> args = {"stash", "apply"};
    if (options.restoreIndex) {
        args.emplace_back("--index");
    }
    args.emplace_back("stash@{" + std::to_string(options.stashIndex) + "}");

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile applicare lo stash: " + execResult.error().message,
            .command = "git stash apply"
        });
    }

    if (execResult->exitCode != 0) {
        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;

        if (isStashNotFoundError(combinedOutput)) {
            kind = gf::git::ports::GitOpsError::Kind::StashNotFound;
        } else if (combinedOutput.find("CONFLICT") != std::string::npos ||
                   combinedOutput.find("conflict") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::MergeConflict;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
            .command = "git stash apply"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::stashDrop(const std::filesystem::path& repoPath,
                                  std::size_t stashIndex) {
    gf::git::bo::GitCommand cmd{
        .args = {"stash", "drop", "stash@{" + std::to_string(stashIndex) + "}"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eliminare lo stash: " + execResult.error().message,
            .command = "git stash drop"
        });
    }

    if (execResult->exitCode != 0) {
        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;

        if (isStashNotFoundError(combinedOutput)) {
            kind = gf::git::ports::GitOpsError::Kind::StashNotFound;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
            .command = "git stash drop"
        });
    }

    return {};
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::stashClear(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"stash", "clear"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile svuotare gli stash: " + execResult.error().message,
            .command = "git stash clear"
        });
    }

    if (execResult->exitCode != 0) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
            .command = "git stash clear"
        });
    }

    return {};
}

std::expected<bool, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::isRebaseInProgress(const std::filesystem::path& repoPath) {
    std::error_code ec;
    bool inProgress = std::filesystem::exists(repoPath / ".git" / "rebase-merge", ec) ||
                      std::filesystem::exists(repoPath / ".git" / "rebase-apply", ec);
    return inProgress;
}

std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::startRebase(const std::filesystem::path& repoPath,
                                    const gf::git::bo::RebaseOptions& options) {
    std::vector<std::string> args = {"rebase"};
    if (options.autostash) {
        args.emplace_back("--autostash");
    }
    if (options.onto && !options.onto->empty()) {
        args.emplace_back("--onto");
        args.emplace_back(*options.onto);
    }
    args.emplace_back(options.upstream);
    if (options.branch && !options.branch->empty()) {
        args.emplace_back(*options.branch);
    }

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{60'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile avviare il rebase: " + execResult.error().message,
            .command = "git rebase"
        });
    }

    gf::git::bo::RebaseResult result;
    const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

    if (execResult->exitCode == 0) {
        if (combinedOutput.find("is up to date") != std::string::npos ||
            combinedOutput.find("è aggiornato") != std::string::npos) {
            result.status = gf::git::bo::RebaseStatus::UpToDate;
        } else {
            result.status = gf::git::bo::RebaseStatus::Finished;
        }
        result.message = execResult->stdoutBytes;
        return result;
    }

    if (combinedOutput.find("CONFLICT") != std::string::npos ||
        combinedOutput.find("Automatic merge failed") != std::string::npos ||
        combinedOutput.find("conflitti") != std::string::npos ||
        combinedOutput.find("Resolve all conflicts") != std::string::npos ||
        combinedOutput.find("could not apply") != std::string::npos) {
        result.status = gf::git::bo::RebaseStatus::Conflict;
        result.message = combinedOutput;
        populateRebaseProgressAndConflicts(repoPath, m_executor, result);
        return result;
    }

    if (combinedOutput.find("cannot rebase: You have unstaged changes") != std::string::npos ||
        combinedOutput.find("working tree clean") != std::string::npos ||
        combinedOutput.find("unstaged changes") != std::string::npos) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::DirtyWorkingTree,
            .exitCode = execResult->exitCode,
            .message = combinedOutput,
            .command = "git rebase"
        });
    }

    return std::unexpected(gf::git::ports::GitOpsError{
        .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
        .exitCode = execResult->exitCode,
        .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
        .command = "git rebase"
    });
}

std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::continueRebase(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"-c", "core.editor=true", "rebase", "--continue"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{60'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile continuare il rebase: " + execResult.error().message,
            .command = "git rebase --continue"
        });
    }

    gf::git::bo::RebaseResult result;
    const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

    if (execResult->exitCode == 0) {
        auto inProgress = isRebaseInProgress(repoPath);
        if (inProgress && *inProgress) {
            result.status = gf::git::bo::RebaseStatus::Applied;
        } else {
            result.status = gf::git::bo::RebaseStatus::Finished;
        }
        result.message = execResult->stdoutBytes;
        return result;
    }

    if (combinedOutput.find("no rebase in progress") != std::string::npos ||
        combinedOutput.find("nessun rebase in corso") != std::string::npos) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::NoRebaseInProgress,
            .exitCode = execResult->exitCode,
            .message = combinedOutput,
            .command = "git rebase --continue"
        });
    }

    if (combinedOutput.find("CONFLICT") != std::string::npos ||
        combinedOutput.find("Automatic merge failed") != std::string::npos ||
        combinedOutput.find("conflitti") != std::string::npos ||
        combinedOutput.find("Resolve all conflicts") != std::string::npos ||
        combinedOutput.find("could not apply") != std::string::npos ||
        combinedOutput.find("unmerged") != std::string::npos) {
        result.status = gf::git::bo::RebaseStatus::Conflict;
        result.message = combinedOutput;
        populateRebaseProgressAndConflicts(repoPath, m_executor, result);
        return result;
    }

    return std::unexpected(gf::git::ports::GitOpsError{
        .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
        .exitCode = execResult->exitCode,
        .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
        .command = "git rebase --continue"
    });
}

std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::skipRebase(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"-c", "core.editor=true", "rebase", "--skip"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{60'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile saltare il commit nel rebase: " + execResult.error().message,
            .command = "git rebase --skip"
        });
    }

    gf::git::bo::RebaseResult result;
    const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

    if (execResult->exitCode == 0) {
        auto inProgress = isRebaseInProgress(repoPath);
        if (inProgress && *inProgress) {
            result.status = gf::git::bo::RebaseStatus::Skipped;
        } else {
            result.status = gf::git::bo::RebaseStatus::Finished;
        }
        result.message = execResult->stdoutBytes;
        return result;
    }

    if (combinedOutput.find("no rebase in progress") != std::string::npos ||
        combinedOutput.find("nessun rebase in corso") != std::string::npos) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::NoRebaseInProgress,
            .exitCode = execResult->exitCode,
            .message = combinedOutput,
            .command = "git rebase --skip"
        });
    }

    if (combinedOutput.find("CONFLICT") != std::string::npos ||
        combinedOutput.find("Automatic merge failed") != std::string::npos ||
        combinedOutput.find("conflitti") != std::string::npos ||
        combinedOutput.find("Resolve all conflicts") != std::string::npos ||
        combinedOutput.find("could not apply") != std::string::npos) {
        result.status = gf::git::bo::RebaseStatus::Conflict;
        result.message = combinedOutput;
        populateRebaseProgressAndConflicts(repoPath, m_executor, result);
        return result;
    }

    return std::unexpected(gf::git::ports::GitOpsError{
        .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
        .exitCode = execResult->exitCode,
        .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
        .command = "git rebase --skip"
    });
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::abortRebase(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"rebase", "--abort"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile annullare il rebase: " + execResult.error().message,
            .command = "git rebase --abort"
        });
    }

    if (execResult->exitCode != 0) {
        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (combinedOutput.find("no rebase in progress") != std::string::npos ||
            combinedOutput.find("nessun rebase in corso") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::NoRebaseInProgress;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
            .command = "git rebase --abort"
        });
    }

    return {};
}

std::expected<gf::git::bo::CherryPickResult, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::cherryPick(const std::filesystem::path& repoPath,
                                   const gf::git::bo::CherryPickOptions& options) {
    std::vector<std::string> args = {"-c", "core.editor=true", "cherry-pick"};
    if (options.noCommit) {
        args.emplace_back("-n");
    }
    args.emplace_back(options.commitSha);

    gf::git::bo::GitCommand cmd{
        .args = std::move(args),
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{60'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile eseguire cherry-pick: " + execResult.error().message,
            .command = "git cherry-pick"
        });
    }

    gf::git::bo::CherryPickResult result;
    const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

    if (execResult->exitCode == 0) {
        result.success = true;
        result.hasConflicts = false;
        result.message = execResult->stdoutBytes;

        if (!options.noCommit) {
            auto revCmd = m_executor.execute(gf::git::bo::GitCommand{
                .args = {"rev-parse", "HEAD"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{5'000}
            });
            if (revCmd && revCmd->exitCode == 0) {
                result.newCommitSha = trimString(revCmd->stdoutBytes);
            }
        }
        return result;
    }

    if (combinedOutput.find("CONFLICT") != std::string::npos ||
        combinedOutput.find("could not apply") != std::string::npos ||
        combinedOutput.find("conflict") != std::string::npos ||
        combinedOutput.find("conflitti") != std::string::npos) {
        result.success = false;
        result.hasConflicts = true;
        result.message = combinedOutput;

        auto diffUCmd = m_executor.execute(gf::git::bo::GitCommand{
            .args = {"diff", "--name-only", "--diff-filter=U"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{10'000}
        });

        if (diffUCmd && diffUCmd->exitCode == 0) {
            std::string_view out(diffUCmd->stdoutBytes);
            while (!out.empty()) {
                auto nl = out.find('\n');
                std::string_view line = (nl == std::string_view::npos) ? out : out.substr(0, nl);
                out = (nl == std::string_view::npos) ? std::string_view{} : out.substr(nl + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.remove_suffix(1);
                }
                if (!line.empty()) {
                    result.conflictedFiles.emplace_back(line);
                }
            }
        }

        return result;
    }

    if (combinedOutput.find("bad object") != std::string::npos ||
        combinedOutput.find("could not resolve") != std::string::npos ||
        combinedOutput.find("non è un oggetto valido") != std::string::npos) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::CommitNotFound,
            .exitCode = execResult->exitCode,
            .message = combinedOutput,
            .command = "git cherry-pick"
        });
    }

    if (combinedOutput.find("unstaged changes") != std::string::npos ||
        combinedOutput.find("dirty") != std::string::npos) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::DirtyWorkingTree,
            .exitCode = execResult->exitCode,
            .message = combinedOutput,
            .command = "git cherry-pick"
        });
    }

    return std::unexpected(gf::git::ports::GitOpsError{
        .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
        .exitCode = execResult->exitCode,
        .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
        .command = "git cherry-pick"
    });
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::abortCherryPick(const std::filesystem::path& repoPath) {
    gf::git::bo::GitCommand cmd{
        .args = {"cherry-pick", "--abort"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
        .timeout = std::chrono::milliseconds{30'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile annullare il cherry-pick: " + execResult.error().message,
            .command = "git cherry-pick --abort"
        });
    }

    if (execResult->exitCode != 0) {
        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
        auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
        if (combinedOutput.find("no cherry-pick or revert in progress") != std::string::npos ||
            combinedOutput.find("nessun cherry-pick") != std::string::npos) {
            kind = gf::git::ports::GitOpsError::Kind::InvalidOperation;
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = kind,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
            .command = "git cherry-pick --abort"
        });
    }

    return {};
}

std::expected<std::string, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::getFileStageContent(const std::filesystem::path& repoPath,
                                            const std::string& filePath,
                                            int stage) {
    gf::git::bo::GitCommand cmd{
        .args = {"show", ":" + std::to_string(stage) + ":" + filePath},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{15'000}
    };

    auto execResult = m_executor.execute(cmd);
    if (!execResult) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = -1,
            .message = "Impossibile recuperare stage " + std::to_string(stage) + ": " + execResult.error().message,
            .command = "git show"
        });
    }

    if (execResult->exitCode != 0) {
        const std::string err = execResult->stderrBytes;
        if (err.find("does not exist") != std::string::npos ||
            err.find("non esiste") != std::string::npos ||
            err.find("Path '") != std::string::npos ||
            err.find("fatal:") != std::string::npos) {
            return std::string{};
        }

        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = execResult->exitCode,
            .message = execResult->stderrBytes,
            .command = "git show"
        });
    }

    return execResult->stdoutBytes;
}

std::expected<gf::git::bo::ConflictFile, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::getConflictFile(const std::filesystem::path& repoPath,
                                        const std::string& filePath) {
    gf::git::bo::ConflictFile file;
    file.filePath = filePath;

    auto fullPath = repoPath / filePath;
    std::error_code ec;
    if (!std::filesystem::exists(fullPath, ec)) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::PathNotFound,
            .exitCode = -1,
            .message = "File non trovato: " + fullPath.string(),
            .command = "conflict"
        });
    }

    std::ifstream ifs(fullPath, std::ios::binary);
    if (!ifs) {
        return std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::StorageError,
            .exitCode = -1,
            .message = "Impossibile aprire il file in conflitto: " + fullPath.string(),
            .command = "conflict"
        });
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    file.fullContentWithMarkers = content;

    if (content.find('\0') != std::string::npos) {
        file.isBinary = true;
        return file;
    }

    file.isBinary = false;
    file.hunks = gf::git::bl::ConflictMarkerParser::parseHunks(content);

    bool hasMissingBase = false;
    for (const auto& h : file.hunks) {
        if (h.baseContent.empty()) {
            hasMissingBase = true;
            break;
        }
    }
    if (hasMissingBase) {
        auto stage1 = getFileStageContent(repoPath, filePath, 1);
        if (stage1 && !stage1->empty()) {
            for (auto& h : file.hunks) {
                if (h.baseContent.empty()) {
                    h.baseContent = *stage1;
                }
            }
        }
    }

    return file;
}

std::expected<void, gf::git::ports::GitOpsError>
NativeRepositoryGitOps::applyConflictResolution(const std::filesystem::path& repoPath,
                                                const gf::git::bo::ApplyConflictResolutionOptions& options) {
    auto fullPath = repoPath / options.filePath;
    std::error_code ec;
    std::filesystem::create_directories(fullPath.parent_path(), ec);

    {
        std::ofstream ofs(fullPath, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::StorageError,
                .exitCode = -1,
                .message = "Impossibile salvare la risoluzione nel file: " + fullPath.string(),
                .command = "conflict-resolve"
            });
        }
        ofs << options.resolvedContent;
    }

    if (options.autoStage) {
        gf::git::bo::GitCommand cmd{
            .args = {"add", "--", options.filePath},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{15'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire stage del file risolto: " + execResult.error().message,
                .command = "git add"
            });
        }

        if (execResult->exitCode != 0) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult->exitCode,
                .message = execResult->stderrBytes,
                .command = "git add"
            });
        }
    }

    return {};
}

} // namespace gf::infra::git
