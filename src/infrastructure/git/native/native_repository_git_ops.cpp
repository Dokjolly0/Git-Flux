#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "core/modules/git/bl/commit_log_parser.hpp"
#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/modules/git/bl/unified_diff_parser.hpp"
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
        // Untracked: usa --no-index rispetto a /dev/null
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

    // Per --no-index, exitCode 1 significa che sono presenti differenze
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
        // Per Untracked, elenca i file non tracciati e ricava il diff per ciascuno
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

        // Parsing ahead/behind da track (es. "[ahead 2, behind 1]")
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

    // Se la lista è vuota (es. repository appena inizializzato con unborn HEAD)
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

} // namespace gf::infra::git
