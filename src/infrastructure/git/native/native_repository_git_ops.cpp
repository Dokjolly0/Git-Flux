#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/modules/git/bo/git_command.hpp"

#include <algorithm>

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

    // Se fallisce a causa di unborn HEAD (nessun commit nel repository):
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
            // Fallback per unborn HEAD: git rm --cached -r -- .
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
        // 1. Prova a ripristinare il file tracciato (modificato o rimosso nel working tree)
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

        // 2. Se restore fallisce (file untracked), prova con git clean
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

        // 3. Fallback per pulizia diretta su filesystem se il file untracked esiste ancora
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
    // Controllo rapido per staging vuoto se non è richiesto amend o allowEmpty
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

    // Risoluzione commit SHA
    gf::git::bo::GitCommand revParseCmd{
        .args = {"rev-parse", "HEAD"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
        .timeout = std::chrono::milliseconds{10'000}
    };
    auto revParseRes = m_executor.execute(revParseCmd);
    std::string commitSha = revParseRes ? trimString(revParseRes->stdoutBytes) : "";
    std::string shortSha = commitSha.size() >= 8 ? commitSha.substr(0, 8) : commitSha;

    // Risoluzione branch
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

} // namespace gf::infra::git
