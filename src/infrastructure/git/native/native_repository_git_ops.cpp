#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "core/shared/utf8_path.hpp"
#include "core/modules/git/bl/commit_log_parser.hpp"
#include "core/modules/git/bl/conflict_marker_parser.hpp"
#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/modules/git/bl/unified_diff_parser.hpp"
#include "core/modules/git/bo/git_command.hpp"

#include <algorithm>
#include <fstream>
#include <unordered_map>

namespace gf::infra::git
{
    namespace
    {
        [[nodiscard]] gf::git::bo::FileChangeKind mapStatusCharToKind(char code) noexcept
        {
            switch (code)
            {
            case 'M': return gf::git::bo::FileChangeKind::Modified;
            case 'A': return gf::git::bo::FileChangeKind::Added;
            case 'D': return gf::git::bo::FileChangeKind::Deleted;
            case 'R': return gf::git::bo::FileChangeKind::Renamed;
            case 'C': return gf::git::bo::FileChangeKind::Copied;
            case '?': return gf::git::bo::FileChangeKind::Untracked;
            case 'U': return gf::git::bo::FileChangeKind::Conflicted;
            default: return gf::git::bo::FileChangeKind::Modified;
            }
        }

        [[nodiscard]] std::string trimString(std::string str)
        {
            while (!str.empty() && (str.back() == '\n' || str.back() == '\r' || str.back() == ' ' || str.back() ==
                '\t'))
            {
                str.pop_back();
            }
            std::size_t start = 0;
            while (start < str.size() && (str[start] == '\n' || str[start] == '\r' || str[start] == ' ' || str[start] ==
                '\t'))
            {
                ++start;
            }
            return str.substr(start);
        }

        [[nodiscard]] std::string extractSummaryLine(std::string_view message)
        {
            auto newlinePos = message.find_first_of("\r\n");
            if (newlinePos == std::string_view::npos)
            {
                return std::string(message);
            }
            return std::string(message.substr(0, newlinePos));
        }

        /// Runs a git command in `repoPath`; returns trimmed stdout on exit code 0, an error otherwise.
        [[nodiscard]] std::expected<std::string, gf::git::ports::GitOpsError>
        runGitCapture(gf::git::ports::IGitCommandExecutor& executor,
                      const std::filesystem::path& repoPath,
                      std::vector<std::string> args)
        {
            const std::string label = "git " + (args.empty() ? std::string{} : args.front());
            gf::git::bo::GitCommand cmd{
                .args = std::move(args),
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                .timeout = std::chrono::milliseconds{30'000}
            };
            auto exec = executor.execute(cmd);
            if (!exec)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                    .exitCode = -1,
                    .message = "Impossibile eseguire " + label + ": " + exec.error().message,
                    .command = label
                });
            }
            if (exec->exitCode != 0)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                    .exitCode = exec->exitCode,
                    .message = exec->stderrBytes.empty() ? exec->stdoutBytes : exec->stderrBytes,
                    .command = label
                });
            }
            return trimString(std::move(exec->stdoutBytes));
        }

        [[nodiscard]] bool isStashNotFoundError(std::string_view output) noexcept
        {
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
                                                gf::git::bo::RebaseResult& result)
        {
            auto readFirstWord = [](const std::filesystem::path& p) -> std::string
            {
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
            if (std::filesystem::exists(rebaseMergeDir, ec))
            {
                std::string msgnumStr = readFirstWord(rebaseMergeDir / "msgnum");
                std::string endStr = readFirstWord(rebaseMergeDir / "end");
                std::string stoppedSha = readFirstWord(rebaseMergeDir / "stopped-sha");
                if (!msgnumStr.empty())
                {
                    try { result.currentStep = std::stoul(msgnumStr); }
                    catch (...)
                    {
                    }
                }
                if (!endStr.empty())
                {
                    try { result.totalSteps = std::stoul(endStr); }
                    catch (...)
                    {
                    }
                }
                if (!stoppedSha.empty())
                {
                    result.currentCommitSha = stoppedSha;
                }
            }
            else if (std::filesystem::exists(rebaseApplyDir, ec))
            {
                std::string nextStr = readFirstWord(rebaseApplyDir / "next");
                std::string lastStr = readFirstWord(rebaseApplyDir / "last");
                std::string origCommit = readFirstWord(rebaseApplyDir / "original-commit");
                if (!nextStr.empty())
                {
                    try { result.currentStep = std::stoul(nextStr); }
                    catch (...)
                    {
                    }
                }
                if (!lastStr.empty())
                {
                    try { result.totalSteps = std::stoul(lastStr); }
                    catch (...)
                    {
                    }
                }
                if (!origCommit.empty())
                {
                    result.currentCommitSha = origCommit;
                }
            }

            auto diffUCmd = executor.execute(gf::git::bo::GitCommand{
                .args = {"diff", "--name-only", "--diff-filter=U"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{10'000}
            });

            if (diffUCmd && diffUCmd->exitCode == 0)
            {
                std::string_view out(diffUCmd->stdoutBytes);
                while (!out.empty())
                {
                    auto nl = out.find('\n');
                    std::string_view line = (nl == std::string_view::npos) ? out : out.substr(0, nl);
                    out = (nl == std::string_view::npos) ? std::string_view{} : out.substr(nl + 1);
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.remove_suffix(1);
                    }
                    if (!line.empty())
                    {
                        result.conflictedFiles.emplace_back(line);
                    }
                }
            }
        }
    } // namespace

    NativeRepositoryGitOps::NativeRepositoryGitOps(gf::git::ports::IGitCommandExecutor& executor) noexcept
        : m_executor(executor)
    {
    }

    std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getWorkingTreeStatus(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"status", "--porcelain=v2", "-z", "--branch", "--ahead-behind", "--untracked-files=all"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire git status: " + execResult.error().message,
                .command = "git status"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("index.lock") != std::string::npos)
            {
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
        if (!parseResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = 0,
                .message = "Parsing porcelain fallito: " + parseResult.error(),
                .command = "git status"
            });
        }

        gf::git::bo::RepoWorkingTreeStatus workingTree;
        workingTree.branch = std::move(parseResult->branch);

        for (const auto& entry : parseResult->entries)
        {
            if (entry.isConflict)
            {
                workingTree.conflictedFiles.push_back(gf::git::bo::WorkingTreeFileItem{
                    .path = entry.path,
                    .originalPath = entry.originalPath,
                    .kind = gf::git::bo::FileChangeKind::Conflicted,
                    .statusCode = entry.unstagedStatus != '.' ? entry.unstagedStatus : entry.stagedStatus
                });
                continue;
            }

            if (entry.isUntracked)
            {
                workingTree.untrackedFiles.push_back(gf::git::bo::WorkingTreeFileItem{
                    .path = entry.path,
                    .originalPath = {},
                    .kind = gf::git::bo::FileChangeKind::Untracked,
                    .statusCode = '?'
                });
                continue;
            }

            if (entry.hasStagedChanges())
            {
                workingTree.stagedFiles.push_back(gf::git::bo::WorkingTreeFileItem{
                    .path = entry.path,
                    .originalPath = entry.originalPath,
                    .kind = mapStatusCharToKind(entry.stagedStatus),
                    .statusCode = entry.stagedStatus
                });
            }

            if (entry.hasUnstagedChanges())
            {
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
                                       const std::vector<std::string>& filePaths)
    {
        if (filePaths.empty())
        {
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
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire git add: " + execResult.error().message,
                .command = "git add"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("index.lock") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::IndexLocked;
            }
            else if (execResult->stderrBytes.find("did not match any files") != std::string::npos ||
                execResult->stderrBytes.find("non corrisponde ad alcun file") != std::string::npos)
            {
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
    NativeRepositoryGitOps::stageAll(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"add", "-A"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire git add -A: " + execResult.error().message,
                .command = "git add -A"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("index.lock") != std::string::npos)
            {
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
                                         const std::vector<std::string>& filePaths)
    {
        if (filePaths.empty())
        {
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
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire git restore --staged: " + execResult.error().message,
                .command = "git restore --staged"
            });
        }

        // In case of unborn HEAD failure (no commits in repository):
        if (execResult->exitCode != 0)
        {
            if (execResult->stderrBytes.find("HEAD") != std::string::npos)
            {
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
                if (rmResult && rmResult->exitCode == 0)
                {
                    return {};
                }
            }

            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("index.lock") != std::string::npos)
            {
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
    NativeRepositoryGitOps::unstageAll(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand restoreCmd{
            .args = {"restore", "--staged", "."},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(restoreCmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire git restore --staged .: " + execResult.error().message,
                .command = "git restore --staged"
            });
        }

        if (execResult->exitCode != 0)
        {
            if (execResult->stderrBytes.find("HEAD") != std::string::npos)
            {
                // Fallback for unborn HEAD: git rm --cached -r -- .
                gf::git::bo::GitCommand rmCmd{
                    .args = {"rm", "--cached", "-r", "--", "."},
                    .workingDirectory = repoPath,
                    .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                    .timeout = std::chrono::milliseconds{30'000}
                };
                auto rmResult = m_executor.execute(rmCmd);
                if (rmResult && rmResult->exitCode == 0)
                {
                    return {};
                }
            }

            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("index.lock") != std::string::npos)
            {
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
                                         const std::vector<std::string>& filePaths)
    {
        if (filePaths.empty())
        {
            return {};
        }

        for (const auto& path : filePaths)
        {
            // 1. Try restoring tracked file (modified or deleted in working tree)
            gf::git::bo::GitCommand restoreCmd{
                .args = {"restore", "--", path},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                .timeout = std::chrono::milliseconds{30'000}
            };

            auto restoreRes = m_executor.execute(restoreCmd);
            if (restoreRes && restoreRes->exitCode == 0)
            {
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
            if (cleanRes && cleanRes->exitCode == 0)
            {
                continue;
            }

            // 3. Fallback to direct filesystem removal if untracked file still exists
            std::error_code fsEc;
            auto targetPath = repoPath / path;
            if (std::filesystem::exists(targetPath, fsEc))
            {
                std::filesystem::remove_all(targetPath, fsEc);
                if (!fsEc && !std::filesystem::exists(targetPath, fsEc))
                {
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
                                         const gf::git::bo::CommitOptions& options)
    {
        // Fast check for empty staging area if neither amend nor allowEmpty is specified
        if (!options.amend && !options.allowEmpty)
        {
            gf::git::bo::GitCommand diffCheck{
                .args = {"diff", "--cached", "--quiet"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{10'000}
            };
            auto diffRes = m_executor.execute(diffCheck);
            if (diffRes && diffRes->exitCode == 0)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::NothingToCommit,
                    .exitCode = 1,
                    .message = "Nessuna modifica nell'area di staging da includere nel commit",
                    .command = "git commit"
                });
            }
        }

        std::vector<std::string> args{"commit", "-m", options.message};
        if (options.author.has_value() && options.author->isValid())
        {
            args.push_back("--author=" + options.author->name + " <" + options.author->email + ">");
        }
        if (options.amend)
        {
            args.emplace_back("--amend");
        }
        if (options.allowEmpty)
        {
            args.emplace_back("--allow-empty");
        }

        gf::git::bo::GitCommand commitCmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(commitCmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire git commit: " + execResult.error().message,
                .command = "git commit"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("index.lock") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::IndexLocked;
            }
            else if (execResult->stdoutBytes.find("nothing to commit") != std::string::npos ||
                execResult->stdoutBytes.find("non c'è nulla") != std::string::npos ||
                execResult->stdoutBytes.find("no changes added") != std::string::npos)
            {
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
                                        gf::git::bo::DiffScope scope)
    {
        return getFileDiffWithContext(repoPath, filePath, scope, 3);
    }

    std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getFileDiffWithContext(const std::filesystem::path& repoPath,
                                                   const std::string& filePath,
                                                   gf::git::bo::DiffScope scope,
                                                   int contextLines)
    {
        const int effectiveContext = (contextLines < 0) ? 999999 : contextLines;
        const std::string contextArg = "-U" + std::to_string(effectiveContext);
        std::vector<std::string> args;
        if (scope == gf::git::bo::DiffScope::Staged)
        {
            args = {
                "diff", "--cached", "--no-ext-diff", "--no-textconv", "--no-color", contextArg, "-p", "--", filePath
            };
        }
        else if (scope == gf::git::bo::DiffScope::Unstaged)
        {
            args = {"diff", "--no-ext-diff", "--no-textconv", "--no-color", contextArg, "-p", "--", filePath};
        }
        else
        {
            std::error_code ec;
            if (filePath.ends_with('/') || std::filesystem::is_directory(repoPath / filePath, ec))
            {
                return gf::git::bo::FileDiff{
                    .oldPath = filePath,
                    .newPath = filePath
                };
            }
            // Untracked: use --no-index against /dev/null
            const auto untrackedFullPath = repoPath / gf::core::utf8ToPath(filePath);
            const auto fileSize = std::filesystem::file_size(untrackedFullPath, ec);
            if (!ec && fileSize > 2 * 1024 * 1024)
            {
                return gf::git::bo::FileDiff{
                    .oldPath = filePath,
                    .newPath = filePath,
                    .isBinary = true,
                    .isNewFile = true
                };
            }
            args = {
                "diff", "--no-index", "--no-ext-diff", "--no-textconv", "--no-color", contextArg, "-p", "--",
                "/dev/null", filePath
            };
        }

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire git diff: " + execResult.error().message,
                .command = "git diff"
            });
        }

        // For --no-index, exitCode 1 indicates differences are present
        const bool isNoIndex = (scope == gf::git::bo::DiffScope::Untracked);
        if ((isNoIndex && execResult->exitCode >= 2) || (!isNoIndex && execResult->exitCode > 1))
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult->exitCode,
                .message = execResult->stderrBytes,
                .command = "git diff"
            });
        }

        if (execResult->stdoutBytes.empty())
        {
            return gf::git::bo::FileDiff{
                .oldPath = filePath,
                .newPath = filePath
            };
        }

        auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(execResult->stdoutBytes);
        if (diffOpt.has_value())
        {
            if (diffOpt->newPath.empty())
            {
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
                                        gf::git::bo::DiffScope scope)
    {
        if (scope == gf::git::bo::DiffScope::Untracked)
        {
            // For untracked scope, list untracked files and get diff for each
            auto statusRes = getWorkingTreeStatus(repoPath);
            if (!statusRes)
            {
                return std::unexpected(statusRes.error());
            }

            std::vector<gf::git::bo::FileDiff> result;
            result.reserve(statusRes->untrackedFiles.size());
            for (const auto& item : statusRes->untrackedFiles)
            {
                auto diff = getFileDiff(repoPath, item.path, gf::git::bo::DiffScope::Untracked);
                if (diff)
                {
                    result.push_back(std::move(*diff));
                }
            }
            return result;
        }

        std::vector<std::string> args;
        if (scope == gf::git::bo::DiffScope::Staged)
        {
            args = {"diff", "--cached", "--no-color", "-p"};
        }
        else
        {
            args = {"diff", "--no-color", "-p"};
        }

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire git diff: " + execResult.error().message,
                .command = "git diff"
            });
        }

        if (execResult->exitCode > 1)
        {
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
                                           const std::string& commitSha)
    {
        if (commitSha.empty())
        {
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
        if (!diffTreeRes || diffTreeRes->exitCode != 0)
        {
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

        struct NumStatEntry
        {
            int additions = 0;
            int deletions = 0;
            bool isBinary = false;
        };
        std::unordered_map<std::string, NumStatEntry> numstatMap;

        if (numstatRes && numstatRes->exitCode == 0)
        {
            std::istringstream nss(numstatRes->stdoutBytes);
            std::string nline;
            while (std::getline(nss, nline))
            {
                while (!nline.empty() && (nline.back() == '\r' || nline.back() == ' '))
                {
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
                if (addStr == "-" || delStr == "-")
                {
                    entry.isBinary = true;
                }
                else
                {
                    try
                    {
                        entry.additions = std::stoi(addStr);
                        entry.deletions = std::stoi(delStr);
                    }
                    catch (...)
                    {
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
        while (std::getline(dss, dline))
        {
            while (!dline.empty() && (dline.back() == '\r' || dline.back() == ' '))
            {
                dline.pop_back();
            }
            if (dline.empty()) continue;

            auto tab1 = dline.find('\t');
            if (tab1 == std::string::npos) continue;

            std::string statusStr = dline.substr(0, tab1);
            std::string rest = dline.substr(tab1 + 1);

            gf::git::bo::CommitFileChange change;
            char sChar = statusStr.empty() ? 'M' : statusStr.front();
            switch (sChar)
            {
            case 'A': change.status = gf::git::bo::FileChangeStatus::Added;
                break;
            case 'D': change.status = gf::git::bo::FileChangeStatus::Deleted;
                break;
            case 'R': change.status = gf::git::bo::FileChangeStatus::Renamed;
                break;
            case 'C': change.status = gf::git::bo::FileChangeStatus::Copied;
                break;
            default: change.status = gf::git::bo::FileChangeStatus::Modified;
                break;
            }

            if (change.status == gf::git::bo::FileChangeStatus::Renamed ||
                change.status == gf::git::bo::FileChangeStatus::Copied)
            {
                auto tab2 = rest.find('\t');
                if (tab2 != std::string::npos)
                {
                    change.oldPath = rest.substr(0, tab2);
                    change.path = rest.substr(tab2 + 1);
                }
                else
                {
                    change.path = rest;
                }
            }
            else
            {
                change.path = rest;
            }

            // Match numstat
            auto it = numstatMap.find(change.path);
            if (it != numstatMap.end())
            {
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
                                              const std::string& filePath)
    {
        return getCommitFileDiffWithContext(repoPath, commitSha, filePath, 3);
    }

    std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getCommitFileDiffWithContext(const std::filesystem::path& repoPath,
                                                         const std::string& commitSha,
                                                         const std::string& filePath,
                                                         int contextLines)
    {
        const int effectiveContext = (contextLines < 0) ? 999999 : contextLines;
        const std::string contextArg = "-U" + std::to_string(effectiveContext);
        if (commitSha.empty() || filePath.empty())
        {
            return gf::git::bo::FileDiff{.oldPath = filePath, .newPath = filePath};
        }

        gf::git::bo::GitCommand cmd{
            .args = {
                "show", "-m", "--first-parent", "--format=", "--no-ext-diff", "--no-textconv", "--no-color", contextArg,
                "-p", commitSha, "--", filePath
            },
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult || execResult->exitCode != 0)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult ? execResult->exitCode : -1,
                .message = execResult ? execResult->stderrBytes : "Execution failed",
                .command = "git show"
            });
        }

        if (execResult->stdoutBytes.empty())
        {
            return gf::git::bo::FileDiff{
                .oldPath = filePath,
                .newPath = filePath
            };
        }

        auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(execResult->stdoutBytes);
        if (diffOpt.has_value())
        {
            if (diffOpt->newPath.empty())
            {
                diffOpt->newPath = filePath;
            }
            if (diffOpt->oldPath.empty())
            {
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
                                         const std::string& filePath)
    {
        if (filePath.empty())
        {
            return std::vector<gf::git::bo::BlameLine>{};
        }

        std::vector<std::string> args = {"blame", "--line-porcelain"};
        if (!revision.empty())
        {
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
        if (!execResult || execResult->exitCode != 0)
        {
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

        while (std::getline(stream, line))
        {
            while (!line.empty() && (line.back() == '\r'))
            {
                line.pop_back();
            }

            if (line.starts_with('\t'))
            {
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

            if (line.starts_with("author "))
            {
                currentAuthor = line.substr(7);
            }
            else if (line.starts_with("author-mail "))
            {
                std::string mail = line.substr(12);
                if (!mail.empty() && mail.front() == '<') mail = mail.substr(1);
                if (!mail.empty() && mail.back() == '>') mail.pop_back();
                currentEmail = std::move(mail);
            }
            else if (line.starts_with("author-time "))
            {
                try
                {
                    long long epoch = std::stoll(line.substr(12));
                    std::time_t t = static_cast<std::time_t>(epoch);
                    std::tm tmBuf{};
#if defined(_WIN32)
                    gmtime_s(&tmBuf, &t);
#else
                    gmtime_r(&t, &tmBuf);
#endif
                    char dateBuf[32];
                    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tmBuf);
                    currentDate = dateBuf;
                }
                catch (...)
                {
                    currentDate.clear();
                }
            }
            else
            {
                // Check for header line: "<sha> <orig-line> <final-line>"
                auto firstSpace = line.find(' ');
                if (firstSpace == 40)
                {
                    // 40-character sha
                    currentSha = line.substr(0, 40);
                    auto secondSpace = line.find(' ', firstSpace + 1);
                    if (secondSpace != std::string::npos)
                    {
                        auto thirdSpace = line.find(' ', secondSpace + 1);
                        std::string fLineStr = (thirdSpace != std::string::npos)
                                                   ? line.substr(secondSpace + 1, thirdSpace - secondSpace - 1)
                                                   : line.substr(secondSpace + 1);
                        try
                        {
                            currentLineNum = std::stoi(fLineStr);
                        }
                        catch (...)
                        {
                            currentLineNum = 1;
                        }
                    }
                }
            }
        }

        return result;
    }

    std::expected<std::vector<gf::git::bo::GitBranch>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::listBranches(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {
                "for-each-ref", "--format=%(refname)%09%(objectname)%09%(HEAD)%09%(upstream:short)%09%(upstream:track)",
                "refs/heads", "refs/remotes"
            },
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile recuperare i branch: " + execResult.error().message,
                .command = "git for-each-ref"
            });
        }

        if (execResult->exitCode != 0)
        {
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
        while (startPos < output.size())
        {
            auto endPos = output.find('\n', startPos);
            std::string_view line = (endPos == std::string_view::npos)
                                        ? output.substr(startPos)
                                        : output.substr(startPos, endPos - startPos);
            startPos = (endPos == std::string_view::npos) ? output.size() : endPos + 1;

            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            {
                line.remove_suffix(1);
            }
            if (line.empty())
            {
                continue;
            }

            std::vector<std::string_view> fields;
            std::size_t fStart = 0;
            while (fStart < line.size())
            {
                auto fEnd = line.find('\t', fStart);
                if (fEnd == std::string_view::npos)
                {
                    fields.push_back(line.substr(fStart));
                    break;
                }
                fields.push_back(line.substr(fStart, fEnd - fStart));
                fStart = fEnd + 1;
            }

            if (fields.size() < 2)
            {
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

            if (refname.starts_with("refs/heads/"))
            {
                b.type = gf::git::bo::BranchType::Local;
                b.name = std::string(refname.substr(11));
                b.shortName = b.name;
            }
            else if (refname.starts_with("refs/remotes/"))
            {
                b.type = gf::git::bo::BranchType::Remote;
                b.name = std::string(refname.substr(13));
                auto slash = b.name.find('/');
                b.shortName = (slash != std::string::npos) ? b.name.substr(slash + 1) : b.name;
                if (b.shortName == "HEAD")
                {
                    continue;
                }
            }
            else
            {
                b.type = gf::git::bo::BranchType::Local;
                b.name = std::string(refname);
                b.shortName = b.name;
            }

            // Parse ahead/behind from tracking descriptor (e.g. "[ahead 2, behind 1]")
            if (!track.empty())
            {
                auto aheadPos = track.find("ahead ");
                if (aheadPos != std::string_view::npos)
                {
                    int val = 0;
                    std::size_t p = aheadPos + 6;
                    while (p < track.size() && track[p] >= '0' && track[p] <= '9')
                    {
                        val = val * 10 + (track[p] - '0');
                        ++p;
                    }
                    b.ahead = val;
                }

                auto behindPos = track.find("behind ");
                if (behindPos != std::string_view::npos)
                {
                    int val = 0;
                    std::size_t p = behindPos + 7;
                    while (p < track.size() && track[p] >= '0' && track[p] <= '9')
                    {
                        val = val * 10 + (track[p] - '0');
                        ++p;
                    }
                    b.behind = val;
                }
            }

            branches.push_back(std::move(b));
        }

        // If branch list is empty (e.g. freshly initialized repository with unborn HEAD)
        if (branches.empty())
        {
            gf::git::bo::GitCommand branchCheckCmd{
                .args = {"rev-parse", "--abbrev-ref", "HEAD"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{10'000}
            };
            auto bcRes = m_executor.execute(branchCheckCmd);
            if (bcRes && bcRes->exitCode == 0)
            {
                std::string currentHead = trimString(bcRes->stdoutBytes);
                if (!currentHead.empty() && currentHead != "HEAD")
                {
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
                                         const gf::git::bo::CreateBranchOptions& options)
    {
        std::vector<std::string> args;
        if (options.checkoutAfterCreate)
        {
            args = {"checkout", "-b", options.name};
        }
        else
        {
            args = {"branch", options.name};
        }

        if (options.startPoint.has_value() && !options.startPoint->empty())
        {
            args.push_back(*options.startPoint);
        }

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile creare il branch: " + execResult.error().message,
                .command = "git branch"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("already exists") != std::string::npos ||
                execResult->stderrBytes.find("esiste già") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::BranchAlreadyExists;
            }
            else if (execResult->stderrBytes.find("not found") != std::string::npos ||
                execResult->stderrBytes.find("non trovato") != std::string::npos ||
                execResult->stderrBytes.find("No branch named") != std::string::npos ||
                execResult->stderrBytes.find("Nessun branch denominato") != std::string::npos)
            {
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
                                           const gf::git::bo::CheckoutOptions& options)
    {
        // A remote-tracking name ("origin/foo") passed to `git checkout` would detach HEAD.
        // Resolve it to the matching local branch, or create a local tracking branch for it.
        std::string checkoutTarget = options.branchOrCommit;
        bool trackRemote = false;
        if (!options.createNewBranch && !checkoutTarget.empty() && checkoutTarget.front() != '-' &&
            !runGitCapture(m_executor, repoPath,
                           {"rev-parse", "--verify", "--quiet", "refs/heads/" + checkoutTarget}) &&
            runGitCapture(m_executor, repoPath,
                          {"rev-parse", "--verify", "--quiet", "refs/remotes/" + checkoutTarget}))
        {
            if (auto remotes = runGitCapture(m_executor, repoPath, {"remote"}))
            {
                std::string localName;
                std::size_t start = 0;
                const std::string& out = *remotes;
                while (start < out.size())
                {
                    auto end = out.find('\n', start);
                    if (end == std::string::npos) end = out.size();
                    const std::string remote = trimString(out.substr(start, end - start));
                    start = end + 1;
                    // Longest matching remote prefix wins (remotes may contain '/')
                    if (!remote.empty() && checkoutTarget.size() > remote.size() + 1 &&
                        checkoutTarget.compare(0, remote.size() + 1, remote + "/") == 0 &&
                        checkoutTarget.size() - remote.size() - 1 > 0)
                    {
                        std::string candidate = checkoutTarget.substr(remote.size() + 1);
                        if (localName.empty() || candidate.size() < localName.size())
                        {
                            localName = std::move(candidate);
                        }
                    }
                }

                if (!localName.empty() && localName != "HEAD")
                {
                    if (runGitCapture(m_executor, repoPath,
                                      {"rev-parse", "--verify", "--quiet", "refs/heads/" + localName}))
                    {
                        checkoutTarget = localName;
                    }
                    else
                    {
                        trackRemote = true; // `git checkout --track origin/foo` creates local "foo"
                    }
                }
            }
        }

        std::vector<std::string> args{"checkout"};
        if (options.createNewBranch)
        {
            args.emplace_back("-b");
        }
        if (options.force)
        {
            args.emplace_back("-f");
        }
        if (trackRemote)
        {
            args.emplace_back("--track");
        }
        args.push_back(checkoutTarget);

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire il checkout: " + execResult.error().message,
                .command = "git checkout"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("would be overwritten by checkout") != std::string::npos ||
                execResult->stderrBytes.find("sarebbero sovrascritte con il checkout") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::DirtyWorkingTree;
            }
            else if (execResult->stderrBytes.find("did not match any") != std::string::npos ||
                execResult->stderrBytes.find("non corrisponde ad alcun") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
            }
            else if (execResult->stderrBytes.find("already exists") != std::string::npos ||
                execResult->stderrBytes.find("esiste già") != std::string::npos)
            {
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
                                         bool force)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"branch", force ? "-D" : "-d", branchName},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eliminare il branch: " + execResult.error().message,
                .command = "git branch -d"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("cannot delete") != std::string::npos ||
                execResult->stderrBytes.find("impossibile eliminare") != std::string::npos ||
                execResult->stderrBytes.find("not fully merged") != std::string::npos ||
                execResult->stderrBytes.find("sottoposto a merge") != std::string::npos ||
                execResult->stderrBytes.find("used by worktree") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::InvalidOperation;
            }
            else if (execResult->stderrBytes.find("not found") != std::string::npos ||
                execResult->stderrBytes.find("non trovato") != std::string::npos)
            {
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
                                         const std::string& newName)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"branch", "-m", oldName, newName},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile rinominare il branch: " + execResult.error().message,
                .command = "git branch -m"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("already exists") != std::string::npos ||
                execResult->stderrBytes.find("esiste già") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::BranchAlreadyExists;
            }
            else if (execResult->stderrBytes.find("not found") != std::string::npos ||
                execResult->stderrBytes.find("non trovato") != std::string::npos ||
                execResult->stderrBytes.find("No branch named") != std::string::npos ||
                execResult->stderrBytes.find("Nessun branch denominato") != std::string::npos)
            {
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
                                         const gf::git::bo::CommitLogOptions& options)
    {
        std::vector<std::string> args{
            "log",
            "--format=%H%x00%h%x00%P%x00%an%x00%ae%x00%aI%x00%cn%x00%ce%x00%cI%x00%D%x00%s%x00%b%x1e"
        };

        if (options.dateOrder)
        {
            args.emplace_back("--date-order");
        }
        else if (options.topoOrder)
        {
            args.emplace_back("--topo-order");
        }

        if (options.maxCount > 0)
        {
            args.emplace_back("-n");
            args.push_back(std::to_string(options.maxCount));
        }

        if (options.skip > 0)
        {
            args.push_back("--skip=" + std::to_string(options.skip));
        }

        if (options.followRenames && options.pathFilter.has_value() && !options.pathFilter->empty())
        {
            args.emplace_back("--follow");
        }

        if (options.revisionRange.has_value() && !options.revisionRange->empty())
        {
            args.push_back(*options.revisionRange);
        }

        if (options.pathFilter.has_value() && !options.pathFilter->empty())
        {
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
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile recuperare la cronologia commit: " + execResult.error().message,
                .command = "git log"
            });
        }

        if (execResult->exitCode != 0)
        {
            if (execResult->stderrBytes.find("does not have any commits yet") != std::string::npos ||
                execResult->stderrBytes.find("non ha ancora alcun commit") != std::string::npos ||
                execResult->stderrBytes.find("unknown revision or path not in the working tree") != std::string::npos)
            {
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

    std::expected<std::vector<std::string>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::listTrackedFiles(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"ls-files"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{15'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile recuperare i file del repository: " + execResult.error().message,
                .command = "git ls-files"
            });
        }

        if (execResult->exitCode != 0)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult->exitCode,
                .message = execResult->stderrBytes,
                .command = "git ls-files"
            });
        }

        std::vector<std::string> files;
        std::string_view out = execResult->stdoutBytes;
        std::size_t start = 0;
        while (start < out.size())
        {
            std::size_t end = out.find('\n', start);
            if (end == std::string_view::npos)
            {
                end = out.size();
            }
            std::string_view line = out.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }
            if (!line.empty())
            {
                files.emplace_back(line);
            }
            start = end + 1;
        }
        return files;
    }

    std::expected<std::vector<gf::git::bo::GitTag>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::listTags(const std::filesystem::path& repoPath)
    {
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
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile recuperare i tag: " + execResult.error().message,
                .command = "git tag -l"
            });
        }

        if (execResult->exitCode != 0)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult->exitCode,
                .message = execResult->stderrBytes,
                .command = "git tag -l"
            });
        }

        std::vector<gf::git::bo::GitTag> tags;
        std::string_view out(execResult->stdoutBytes);

        while (!out.empty())
        {
            auto nextLine = out.find('\n');
            std::string_view line = (nextLine == std::string_view::npos) ? out : out.substr(0, nextLine);
            out = (nextLine == std::string_view::npos) ? std::string_view{} : out.substr(nextLine + 1);

            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }
            if (line.empty())
            {
                continue;
            }

            std::vector<std::string_view> fields;
            std::size_t start = 0;
            while (start <= line.size())
            {
                auto tab = line.find('\t', start);
                if (tab == std::string_view::npos)
                {
                    fields.push_back(line.substr(start));
                    break;
                }
                fields.push_back(line.substr(start, tab - start));
                start = tab + 1;
            }

            if (fields.size() < 7)
            {
                continue;
            }

            gf::git::bo::GitTag tag;
            tag.name = std::string(fields[0]);
            auto objName = fields[1];
            auto peeledObj = fields[2];
            tag.taggerName = std::string(fields[3]);

            auto emailStr = std::string(fields[4]);
            if (emailStr.starts_with('<') && emailStr.ends_with('>') && emailStr.size() >= 2)
            {
                emailStr = emailStr.substr(1, emailStr.size() - 2);
            }
            tag.taggerEmail = std::move(emailStr);

            tag.tagDateIso = std::string(fields[5]);
            tag.message = std::string(fields[6]);

            if (!peeledObj.empty())
            {
                tag.type = gf::git::bo::TagType::Annotated;
                tag.targetSha = std::string(peeledObj);
            }
            else
            {
                tag.type = gf::git::bo::TagType::Lightweight;
                tag.targetSha = std::string(objName);
            }

            tags.push_back(std::move(tag));
        }

        return tags;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::createTag(const std::filesystem::path& repoPath,
                                      const gf::git::bo::CreateTagOptions& options)
    {
        std::vector<std::string> args{"tag"};
        if (options.force)
        {
            args.emplace_back("-f");
        }

        if (options.message.has_value())
        {
            args.emplace_back("-a");
            args.push_back(options.name);
            args.emplace_back("-m");
            args.push_back(*options.message);
        }
        else
        {
            args.push_back(options.name);
        }

        if (options.targetCommit.has_value() && !options.targetCommit->empty())
        {
            args.push_back(*options.targetCommit);
        }

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile creare il tag: " + execResult.error().message,
                .command = "git tag"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("already exists") != std::string::npos ||
                execResult->stderrBytes.find("esiste già") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::TagAlreadyExists;
            }
            else if (execResult->stderrBytes.find("not a valid object name") != std::string::npos ||
                execResult->stderrBytes.find("non è un nome di oggetto valido") != std::string::npos)
            {
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
                                      const std::string& tagName)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"tag", "-d", tagName},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eliminare il tag: " + execResult.error().message,
                .command = "git tag -d"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("not found") != std::string::npos ||
                execResult->stderrBytes.find("non trovato") != std::string::npos)
            {
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
    NativeRepositoryGitOps::listRemotes(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"remote", "-v"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile recuperare i remote: " + execResult.error().message,
                .command = "git remote -v"
            });
        }

        if (execResult->exitCode != 0)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult->exitCode,
                .message = execResult->stderrBytes,
                .command = "git remote -v"
            });
        }

        std::unordered_map<std::string, gf::git::bo::GitRemote> remotesMap;
        std::string_view out(execResult->stdoutBytes);

        while (!out.empty())
        {
            auto nextLine = out.find('\n');
            std::string_view line = (nextLine == std::string_view::npos) ? out : out.substr(0, nextLine);
            out = (nextLine == std::string_view::npos) ? std::string_view{} : out.substr(nextLine + 1);

            if (!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }
            if (line.empty())
            {
                continue;
            }

            auto firstSep = line.find_first_of(" \t");
            if (firstSep == std::string_view::npos)
            {
                continue;
            }

            auto name = std::string(line.substr(0, firstSep));
            auto rest = line.substr(firstSep);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t'))
            {
                rest.remove_prefix(1);
            }

            auto lastSep = rest.rfind(" (");
            std::string url;
            bool isPush = false;
            if (lastSep != std::string_view::npos)
            {
                url = std::string(rest.substr(0, lastSep));
                if (rest.find("(push)") != std::string_view::npos)
                {
                    isPush = true;
                }
            }
            else
            {
                url = std::string(rest);
            }

            auto& remote = remotesMap[name];
            remote.name = name;
            if (isPush)
            {
                remote.pushUrl = url;
                if (remote.fetchUrl.empty())
                {
                    remote.fetchUrl = url;
                }
            }
            else
            {
                remote.fetchUrl = url;
                if (remote.pushUrl.empty())
                {
                    remote.pushUrl = url;
                }
            }
        }

        std::vector<gf::git::bo::GitRemote> remotes;
        remotes.reserve(remotesMap.size());
        for (auto& [_, r] : remotesMap)
        {
            remotes.push_back(std::move(r));
        }
        std::sort(remotes.begin(), remotes.end(), [](const auto& a, const auto& b)
        {
            return a.name < b.name;
        });

        return remotes;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::addRemote(const std::filesystem::path& repoPath,
                                      const std::string& name,
                                      const std::string& url)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"remote", "add", name, url},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile aggiungere il remote: " + execResult.error().message,
                .command = "git remote add"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("already exists") != std::string::npos ||
                execResult->stderrBytes.find("esiste già") != std::string::npos)
            {
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
                                         const std::string& name)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"remote", "remove", name},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile rimuovere il remote: " + execResult.error().message,
                .command = "git remote remove"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("No such remote") != std::string::npos ||
                execResult->stderrBytes.find("non trovato") != std::string::npos ||
                execResult->stderrBytes.find("non esiste") != std::string::npos)
            {
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

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::setRemoteUrl(const std::filesystem::path& repoPath,
                                         const std::string& name,
                                         const std::string& url)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"remote", "set-url", name, url},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile aggiornare l'URL del remote: " + execResult.error().message,
                .command = "git remote set-url"
            });
        }

        if (execResult->exitCode != 0)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult->exitCode,
                .message = execResult->stderrBytes,
                .command = "git remote set-url"
            });
        }

        return {};
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::fetchRemote(const std::filesystem::path& repoPath,
                                        const gf::git::bo::RemoteOperationOptions& options)
    {
        std::vector<std::string> args{"fetch"};
        if (options.prune)
        {
            args.emplace_back("--prune");
        }
        if (options.tags)
        {
            args.emplace_back("--tags");
        }
        if (options.force)
        {
            args.emplace_back("--force");
        }
        if (options.allRemotes)
        {
            args.emplace_back("--all");
        }
        else
        {
            args.push_back(options.remoteName);
            if (options.branchName.has_value() && !options.branchName->empty())
            {
                args.push_back(*options.branchName);
            }
        }

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{60'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire il fetch: " + execResult.error().message,
                .command = "git fetch"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("Could not resolve host") != std::string::npos ||
                execResult->stderrBytes.find("Authentication failed") != std::string::npos ||
                execResult->stderrBytes.find("Permission denied") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::NetworkAuthenticationFailed;
            }
            else if (execResult->stderrBytes.find("does not appear to be a git repository") != std::string::npos ||
                execResult->stderrBytes.find("not found") != std::string::npos)
            {
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
                                       const gf::git::bo::RemoteOperationOptions& options)
    {
        std::vector<std::string> args{"pull"};
        if (options.rebase)
        {
            args.emplace_back("--rebase");
        }
        if (options.fastForwardOnly)
        {
            args.emplace_back("--ff-only");
        }
        args.push_back(options.remoteName);
        if (options.branchName.has_value() && !options.branchName->empty())
        {
            args.push_back(*options.branchName);
        }

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{60'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire il pull: " + execResult.error().message,
                .command = "git pull"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("Automatic merge failed") != std::string::npos ||
                execResult->stdoutBytes.find("Automatic merge failed") != std::string::npos ||
                execResult->stdoutBytes.find("CONFLICT") != std::string::npos ||
                execResult->stderrBytes.find("conflitti") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::MergeConflict;
            }
            else if (execResult->stderrBytes.find("Authentication failed") != std::string::npos ||
                execResult->stderrBytes.find("Permission denied") != std::string::npos)
            {
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
                                       const gf::git::bo::RemoteOperationOptions& options)
    {
        std::vector<std::string> args{"push"};
        if (options.setUpstream)
        {
            args.emplace_back("-u");
        }
        if (options.force)
        {
            args.emplace_back("--force-with-lease");
        }
        if (options.tags)
        {
            args.emplace_back("--tags");
        }
        args.push_back(options.remoteName);
        if (options.branchName.has_value() && !options.branchName->empty())
        {
            args.push_back(*options.branchName);
        }

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{60'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire il push: " + execResult.error().message,
                .command = "git push"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("rejected") != std::string::npos ||
                execResult->stderrBytes.find("non-fast-forward") != std::string::npos ||
                execResult->stderrBytes.find("fetch first") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::RemoteRejected;
            }
            else if (execResult->stderrBytes.find("Authentication failed") != std::string::npos ||
                execResult->stderrBytes.find("Permission denied") != std::string::npos)
            {
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
                                        const gf::git::bo::MergeOptions& options)
    {
        std::vector<std::string> args{"merge"};
        if (options.noFastForward)
        {
            args.emplace_back("--no-ff");
        }
        if (options.squash)
        {
            args.emplace_back("--squash");
        }
        if (options.message.has_value() && !options.message->empty())
        {
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
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire il merge: " + execResult.error().message,
                .command = "git merge"
            });
        }

        gf::git::bo::MergeResult result;
        const auto combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

        if (execResult->exitCode == 0)
        {
            result.summaryMessage = execResult->stdoutBytes;
            if (combinedOutput.find("Already up to date") != std::string::npos ||
                combinedOutput.find("È già aggiornato") != std::string::npos)
            {
                result.status = gf::git::bo::MergeStatus::UpToDate;
            }
            else if (combinedOutput.find("Fast-forward") != std::string::npos)
            {
                result.status = gf::git::bo::MergeStatus::FastForward;
            }
            else
            {
                result.status = gf::git::bo::MergeStatus::Merged;
            }

            auto revCmd = m_executor.execute(gf::git::bo::GitCommand{
                .args = {"rev-parse", "HEAD"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{5'000}
            });
            if (revCmd && revCmd->exitCode == 0)
            {
                result.targetCommitSha = trimString(revCmd->stdoutBytes);
            }

            return result;
        }

        if (combinedOutput.find("CONFLICT") != std::string::npos ||
            combinedOutput.find("Automatic merge failed") != std::string::npos ||
            combinedOutput.find("conflitti") != std::string::npos)
        {
            result.status = gf::git::bo::MergeStatus::Conflict;
            result.summaryMessage = execResult->stdoutBytes;

            auto diffUCmd = m_executor.execute(gf::git::bo::GitCommand{
                .args = {"diff", "--name-only", "--diff-filter=U"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{10'000}
            });

            if (diffUCmd && diffUCmd->exitCode == 0)
            {
                std::string_view out(diffUCmd->stdoutBytes);
                while (!out.empty())
                {
                    auto nl = out.find('\n');
                    std::string_view line = (nl == std::string_view::npos) ? out : out.substr(0, nl);
                    out = (nl == std::string_view::npos) ? std::string_view{} : out.substr(nl + 1);
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.remove_suffix(1);
                    }
                    if (!line.empty())
                    {
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
    NativeRepositoryGitOps::abortMerge(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"merge", "--abort"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile annullare il merge: " + execResult.error().message,
                .command = "git merge --abort"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (execResult->stderrBytes.find("no merge to abort") != std::string::npos ||
                execResult->stderrBytes.find("nessun merge da annullare") != std::string::npos ||
                execResult->stderrBytes.find("MERGE_HEAD missing") != std::string::npos)
            {
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
    NativeRepositoryGitOps::listStashes(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"stash", "list", "--format=%gd%x00%H%x00%aI%x00%gs%x1e"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile recuperare l'elenco degli stash: " + execResult.error().message,
                .command = "git stash list"
            });
        }

        if (execResult->exitCode != 0)
        {
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
        while (startPos < raw.size())
        {
            auto endPos = raw.find('\x1e', startPos);
            std::string_view record = (endPos == std::string_view::npos)
                                          ? raw.substr(startPos)
                                          : raw.substr(startPos, endPos - startPos);

            if (!record.empty())
            {
                std::vector<std::string_view> fields;
                std::size_t fStart = 0;
                while (fStart < record.size())
                {
                    auto fEnd = record.find('\0', fStart);
                    if (fEnd == std::string_view::npos)
                    {
                        fields.push_back(record.substr(fStart));
                        break;
                    }
                    fields.push_back(record.substr(fStart, fEnd - fStart));
                    fStart = fEnd + 1;
                }

                if (fields.size() >= 4)
                {
                    gf::git::bo::GitStashItem item;
                    item.selector = std::string(fields[0]);
                    item.commitSha = std::string(fields[1]);
                    item.dateIso = std::string(fields[2]);

                    auto openBrace = item.selector.find('{');
                    auto closeBrace = item.selector.find('}', openBrace);
                    if (openBrace != std::string::npos && closeBrace != std::string::npos && closeBrace > openBrace + 1)
                    {
                        auto numStr = item.selector.substr(openBrace + 1, closeBrace - openBrace - 1);
                        try
                        {
                            item.index = std::stoul(numStr);
                        }
                        catch (...)
                        {
                            item.index = 0;
                        }
                    }

                    std::string_view subject = fields[3];
                    constexpr std::string_view wipPrefix = "WIP on ";
                    constexpr std::string_view onPrefix = "On ";

                    if (subject.starts_with(wipPrefix))
                    {
                        auto rest = subject.substr(wipPrefix.size());
                        auto colonPos = rest.find(':');
                        if (colonPos != std::string_view::npos)
                        {
                            item.branchName = trimString(std::string(rest.substr(0, colonPos)));
                            item.message = trimString(std::string(rest.substr(colonPos + 1)));
                        }
                        else
                        {
                            item.branchName = trimString(std::string(rest));
                            item.message = std::string(subject);
                        }
                    }
                    else if (subject.starts_with(onPrefix))
                    {
                        auto rest = subject.substr(onPrefix.size());
                        auto colonPos = rest.find(':');
                        if (colonPos != std::string_view::npos)
                        {
                            item.branchName = trimString(std::string(rest.substr(0, colonPos)));
                            item.message = trimString(std::string(rest.substr(colonPos + 1)));
                        }
                        else
                        {
                            item.branchName = trimString(std::string(rest));
                            item.message = std::string(subject);
                        }
                    }
                    else
                    {
                        item.message = std::string(subject);
                    }

                    stashes.push_back(std::move(item));
                }
            }

            if (endPos == std::string_view::npos)
            {
                break;
            }
            startPos = endPos + 1;
        }

        return stashes;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::stashPush(const std::filesystem::path& repoPath,
                                      const gf::git::bo::StashPushOptions& options)
    {
        std::vector<std::string> args = {"stash", "push"};
        if (options.includeUntracked)
        {
            args.emplace_back("-u");
        }
        if (options.keepIndex)
        {
            args.emplace_back("--keep-index");
        }
        if (options.message && !options.message->empty())
        {
            args.emplace_back("-m");
            args.emplace_back(*options.message);
        }
        if (!options.paths.empty())
        {
            args.emplace_back("--");
            for (const auto& path : options.paths)
            {
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
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile salvare lo stash: " + execResult.error().message,
                .command = "git stash push"
            });
        }

        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
        if (combinedOutput.find("No local changes to save") != std::string::npos ||
            combinedOutput.find("Nessuna modifica locale da salvare") != std::string::npos)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::NothingToStash,
                .exitCode = execResult->exitCode,
                .message = "Nessuna modifica locale da salvare nello stash",
                .command = "git stash push"
            });
        }

        if (execResult->exitCode != 0)
        {
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
                                     const gf::git::bo::StashApplyOptions& options)
    {
        std::vector<std::string> args = {"stash", "pop"};
        if (options.restoreIndex)
        {
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
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile applicare lo stash: " + execResult.error().message,
                .command = "git stash pop"
            });
        }

        if (execResult->exitCode != 0)
        {
            const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;

            if (isStashNotFoundError(combinedOutput))
            {
                kind = gf::git::ports::GitOpsError::Kind::StashNotFound;
            }
            else if (combinedOutput.find("CONFLICT") != std::string::npos ||
                combinedOutput.find("conflict") != std::string::npos)
            {
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
                                       const gf::git::bo::StashApplyOptions& options)
    {
        std::vector<std::string> args = {"stash", "apply"};
        if (options.restoreIndex)
        {
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
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile applicare lo stash: " + execResult.error().message,
                .command = "git stash apply"
            });
        }

        if (execResult->exitCode != 0)
        {
            const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;

            if (isStashNotFoundError(combinedOutput))
            {
                kind = gf::git::ports::GitOpsError::Kind::StashNotFound;
            }
            else if (combinedOutput.find("CONFLICT") != std::string::npos ||
                combinedOutput.find("conflict") != std::string::npos)
            {
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
                                      std::size_t stashIndex)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"stash", "drop", "stash@{" + std::to_string(stashIndex) + "}"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eliminare lo stash: " + execResult.error().message,
                .command = "git stash drop"
            });
        }

        if (execResult->exitCode != 0)
        {
            const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;

            if (isStashNotFoundError(combinedOutput))
            {
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
    NativeRepositoryGitOps::stashClear(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"stash", "clear"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile svuotare gli stash: " + execResult.error().message,
                .command = "git stash clear"
            });
        }

        if (execResult->exitCode != 0)
        {
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
    NativeRepositoryGitOps::isRebaseInProgress(const std::filesystem::path& repoPath)
    {
        std::error_code ec;
        bool inProgress = std::filesystem::exists(repoPath / ".git" / "rebase-merge", ec) ||
            std::filesystem::exists(repoPath / ".git" / "rebase-apply", ec);
        return inProgress;
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::startRebase(const std::filesystem::path& repoPath,
                                        const gf::git::bo::RebaseOptions& options)
    {
        std::vector<std::string> args = {"rebase"};
        if (options.autostash)
        {
            args.emplace_back("--autostash");
        }
        if (options.onto && !options.onto->empty())
        {
            args.emplace_back("--onto");
            args.emplace_back(*options.onto);
        }
        args.emplace_back(options.upstream);
        if (options.branch && !options.branch->empty())
        {
            args.emplace_back(*options.branch);
        }

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{60'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile avviare il rebase: " + execResult.error().message,
                .command = "git rebase"
            });
        }

        gf::git::bo::RebaseResult result;
        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

        if (execResult->exitCode == 0)
        {
            if (combinedOutput.find("is up to date") != std::string::npos ||
                combinedOutput.find("è aggiornato") != std::string::npos)
            {
                result.status = gf::git::bo::RebaseStatus::UpToDate;
            }
            else
            {
                result.status = gf::git::bo::RebaseStatus::Finished;
            }
            result.message = execResult->stdoutBytes;
            return result;
        }

        if (combinedOutput.find("CONFLICT") != std::string::npos ||
            combinedOutput.find("Automatic merge failed") != std::string::npos ||
            combinedOutput.find("conflitti") != std::string::npos ||
            combinedOutput.find("Resolve all conflicts") != std::string::npos ||
            combinedOutput.find("could not apply") != std::string::npos)
        {
            result.status = gf::git::bo::RebaseStatus::Conflict;
            result.message = combinedOutput;
            populateRebaseProgressAndConflicts(repoPath, m_executor, result);
            return result;
        }

        if (combinedOutput.find("cannot rebase: You have unstaged changes") != std::string::npos ||
            combinedOutput.find("working tree clean") != std::string::npos ||
            combinedOutput.find("unstaged changes") != std::string::npos)
        {
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
    NativeRepositoryGitOps::continueRebase(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"-c", "core.editor=true", "rebase", "--continue"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{60'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile continuare il rebase: " + execResult.error().message,
                .command = "git rebase --continue"
            });
        }

        gf::git::bo::RebaseResult result;
        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

        if (execResult->exitCode == 0)
        {
            auto inProgress = isRebaseInProgress(repoPath);
            if (inProgress && *inProgress)
            {
                result.status = gf::git::bo::RebaseStatus::Applied;
            }
            else
            {
                result.status = gf::git::bo::RebaseStatus::Finished;
            }
            result.message = execResult->stdoutBytes;
            return result;
        }

        if (combinedOutput.find("no rebase in progress") != std::string::npos ||
            combinedOutput.find("nessun rebase in corso") != std::string::npos)
        {
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
            combinedOutput.find("unmerged") != std::string::npos)
        {
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
    NativeRepositoryGitOps::skipRebase(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"-c", "core.editor=true", "rebase", "--skip"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{60'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile saltare il commit nel rebase: " + execResult.error().message,
                .command = "git rebase --skip"
            });
        }

        gf::git::bo::RebaseResult result;
        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

        if (execResult->exitCode == 0)
        {
            auto inProgress = isRebaseInProgress(repoPath);
            if (inProgress && *inProgress)
            {
                result.status = gf::git::bo::RebaseStatus::Skipped;
            }
            else
            {
                result.status = gf::git::bo::RebaseStatus::Finished;
            }
            result.message = execResult->stdoutBytes;
            return result;
        }

        if (combinedOutput.find("no rebase in progress") != std::string::npos ||
            combinedOutput.find("nessun rebase in corso") != std::string::npos)
        {
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
            combinedOutput.find("could not apply") != std::string::npos)
        {
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
    NativeRepositoryGitOps::abortRebase(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"rebase", "--abort"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile annullare il rebase: " + execResult.error().message,
                .command = "git rebase --abort"
            });
        }

        if (execResult->exitCode != 0)
        {
            const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (combinedOutput.find("no rebase in progress") != std::string::npos ||
                combinedOutput.find("nessun rebase in corso") != std::string::npos)
            {
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
                                       const gf::git::bo::CherryPickOptions& options)
    {
        std::vector<std::string> args = {"-c", "core.editor=true", "cherry-pick"};
        if (options.noCommit)
        {
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
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire cherry-pick: " + execResult.error().message,
                .command = "git cherry-pick"
            });
        }

        gf::git::bo::CherryPickResult result;
        const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;

        if (execResult->exitCode == 0)
        {
            result.success = true;
            result.hasConflicts = false;
            result.message = execResult->stdoutBytes;

            if (!options.noCommit)
            {
                auto revCmd = m_executor.execute(gf::git::bo::GitCommand{
                    .args = {"rev-parse", "HEAD"},
                    .workingDirectory = repoPath,
                    .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                    .timeout = std::chrono::milliseconds{5'000}
                });
                if (revCmd && revCmd->exitCode == 0)
                {
                    result.newCommitSha = trimString(revCmd->stdoutBytes);
                }
            }
            return result;
        }

        if (combinedOutput.find("CONFLICT") != std::string::npos ||
            combinedOutput.find("could not apply") != std::string::npos ||
            combinedOutput.find("conflict") != std::string::npos ||
            combinedOutput.find("conflitti") != std::string::npos)
        {
            result.success = false;
            result.hasConflicts = true;
            result.message = combinedOutput;

            auto diffUCmd = m_executor.execute(gf::git::bo::GitCommand{
                .args = {"diff", "--name-only", "--diff-filter=U"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{10'000}
            });

            if (diffUCmd && diffUCmd->exitCode == 0)
            {
                std::string_view out(diffUCmd->stdoutBytes);
                while (!out.empty())
                {
                    auto nl = out.find('\n');
                    std::string_view line = (nl == std::string_view::npos) ? out : out.substr(0, nl);
                    out = (nl == std::string_view::npos) ? std::string_view{} : out.substr(nl + 1);
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.remove_suffix(1);
                    }
                    if (!line.empty())
                    {
                        result.conflictedFiles.emplace_back(line);
                    }
                }
            }

            return result;
        }

        if (combinedOutput.find("bad object") != std::string::npos ||
            combinedOutput.find("could not resolve") != std::string::npos ||
            combinedOutput.find("non è un oggetto valido") != std::string::npos)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::CommitNotFound,
                .exitCode = execResult->exitCode,
                .message = combinedOutput,
                .command = "git cherry-pick"
            });
        }

        if (combinedOutput.find("unstaged changes") != std::string::npos ||
            combinedOutput.find("dirty") != std::string::npos)
        {
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
    NativeRepositoryGitOps::abortCherryPick(const std::filesystem::path& repoPath)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"cherry-pick", "--abort"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile annullare il cherry-pick: " + execResult.error().message,
                .command = "git cherry-pick --abort"
            });
        }

        if (execResult->exitCode != 0)
        {
            const std::string combinedOutput = execResult->stdoutBytes + "\n" + execResult->stderrBytes;
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (combinedOutput.find("no cherry-pick or revert in progress") != std::string::npos ||
                combinedOutput.find("nessun cherry-pick") != std::string::npos)
            {
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
                                                int stage)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"show", ":" + std::to_string(stage) + ":" + filePath},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{15'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile recuperare stage " + std::to_string(stage) + ": " + execResult.error().message,
                .command = "git show"
            });
        }

        if (execResult->exitCode != 0)
        {
            const std::string err = execResult->stderrBytes;
            if (err.find("does not exist") != std::string::npos ||
                err.find("non esiste") != std::string::npos ||
                err.find("Path '") != std::string::npos ||
                err.find("fatal:") != std::string::npos)
            {
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
                                            const std::string& filePath)
    {
        gf::git::bo::ConflictFile file;
        file.filePath = filePath;

        auto fullPath = repoPath / filePath;
        std::error_code ec;
        if (!std::filesystem::exists(fullPath, ec))
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::PathNotFound,
                .exitCode = -1,
                .message = "File non trovato: " + fullPath.string(),
                .command = "conflict"
            });
        }

        std::ifstream ifs(fullPath, std::ios::binary);
        if (!ifs)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::StorageError,
                .exitCode = -1,
                .message = "Impossibile aprire il file in conflitto: " + fullPath.string(),
                .command = "conflict"
            });
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        file.fullContentWithMarkers = content;

        if (content.find('\0') != std::string::npos)
        {
            file.isBinary = true;
            return file;
        }

        file.isBinary = false;
        file.hunks = gf::git::bl::ConflictMarkerParser::parseHunks(content);

        bool hasMissingBase = false;
        for (const auto& h : file.hunks)
        {
            if (h.baseContent.empty())
            {
                hasMissingBase = true;
                break;
            }
        }
        if (hasMissingBase)
        {
            auto stage1 = getFileStageContent(repoPath, filePath, 1);
            if (stage1 && !stage1->empty())
            {
                for (auto& h : file.hunks)
                {
                    if (h.baseContent.empty())
                    {
                        h.baseContent = *stage1;
                    }
                }
            }
        }

        return file;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::applyConflictResolution(const std::filesystem::path& repoPath,
                                                    const gf::git::bo::ApplyConflictResolutionOptions& options)
    {
        auto fullPath = repoPath / options.filePath;
        std::error_code ec;
        std::filesystem::create_directories(fullPath.parent_path(), ec);

        {
            std::ofstream ofs(fullPath, std::ios::binary | std::ios::trunc);
            if (!ofs)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::StorageError,
                    .exitCode = -1,
                    .message = "Impossibile salvare la risoluzione nel file: " + fullPath.string(),
                    .command = "conflict-resolve"
                });
            }
            ofs << options.resolvedContent;
        }

        if (options.autoStage)
        {
            gf::git::bo::GitCommand cmd{
                .args = {"add", "--", options.filePath},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                .timeout = std::chrono::milliseconds{15'000}
            };

            auto execResult = m_executor.execute(cmd);
            if (!execResult)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                    .exitCode = -1,
                    .message = "Impossibile eseguire stage del file risolto: " + execResult.error().message,
                    .command = "git add"
                });
            }

            if (execResult->exitCode != 0)
            {
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

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::resetCommit(const std::filesystem::path& repoPath,
                                        const std::string& commitSha,
                                        const std::string& mode)
    {
        std::string flag = "--mixed";
        if (mode == "soft") flag = "--soft";
        else if (mode == "hard") flag = "--hard";
        else if (mode == "mixed") flag = "--mixed";

        gf::git::bo::GitCommand cmd{
            .args = {"reset", flag, commitSha},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire il reset: " + execResult.error().message,
                .command = "git reset"
            });
        }

        if (execResult->exitCode != 0)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult->exitCode,
                .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
                .command = "git reset"
            });
        }

        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::revertCommit(const std::filesystem::path& repoPath,
                                         const std::string& commitSha)
    {
        gf::git::bo::GitCommand cmd{
            .args = {"-c", "core.editor=true", "revert", "--no-edit", commitSha},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire revert: " + execResult.error().message,
                .command = "git revert"
            });
        }

        if (execResult->exitCode != 0)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult->exitCode,
                .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
                .command = "git revert"
            });
        }

        return {};
    }

    std::expected<std::string, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::rewordCommit(const std::filesystem::path& repoPath,
                                         const std::string& commitSha,
                                         const std::string& newMessage)
    {
        using gf::git::ports::GitOpsError;

        auto makeError = [](GitOpsError::Kind kind, int exitCode, std::string message, std::string command)
        {
            return std::unexpected(GitOpsError{
                .kind = kind,
                .exitCode = exitCode,
                .message = std::move(message),
                .command = std::move(command)
            });
        };

        // Runs a git command; returns stdout on exit code 0, or a classified error otherwise.
        auto runGit = [&](std::vector<std::string> args) -> std::expected<std::string, GitOpsError>
        {
            const std::string label = "git " + (args.empty() ? std::string{} : args.front());
            gf::git::bo::GitCommand cmd{
                .args = std::move(args),
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                .timeout = std::chrono::milliseconds{30'000}
            };
            auto exec = m_executor.execute(cmd);
            if (!exec)
            {
                return makeError(GitOpsError::Kind::ExecutionFailed, -1,
                                 "Impossibile eseguire " + label + ": " + exec.error().message, label);
            }
            if (exec->exitCode != 0)
            {
                return makeError(GitOpsError::Kind::ExecutionFailed, exec->exitCode,
                                 exec->stderrBytes.empty() ? exec->stdoutBytes : exec->stderrBytes, label);
            }
            return std::move(exec->stdoutBytes);
        };

        // --- 1. Normalize the new message (LF line endings, no leading/trailing blank lines) ---
        std::string message;
        message.reserve(newMessage.size() + 1);
        for (std::size_t i = 0; i < newMessage.size(); ++i)
        {
            if (newMessage[i] == '\r')
            {
                if (i + 1 < newMessage.size() && newMessage[i + 1] == '\n') continue;
                message.push_back('\n');
                continue;
            }
            message.push_back(newMessage[i]);
        }
        message = trimString(std::move(message));
        if (message.empty())
        {
            return makeError(GitOpsError::Kind::EmptyCommitMessage, -1,
                             "Il messaggio del commit non può essere vuoto", "reword");
        }
        message.push_back('\n');

        // --- 2. Refuse while another sequencer operation is in progress ---
        if (auto rebasing = isRebaseInProgress(repoPath); rebasing && *rebasing)
        {
            return makeError(GitOpsError::Kind::RebaseInProgress, -1,
                             "Impossibile modificare il messaggio: un rebase è in corso", "reword");
        }
        for (const char* pseudoRef : {"MERGE_HEAD", "CHERRY_PICK_HEAD", "REVERT_HEAD"})
        {
            if (runGit({"rev-parse", "-q", "--verify", pseudoRef}))
            {
                return makeError(GitOpsError::Kind::InvalidOperation, -1,
                                 std::string("Impossibile modificare il messaggio: operazione in corso (") +
                                 pseudoRef + ")", "reword");
            }
        }

        // --- 3. Resolve target and HEAD, check that target is an ancestor of HEAD ---
        if (commitSha.empty() || commitSha.front() == '-')
        {
            return makeError(GitOpsError::Kind::CommitNotFound, -1, "SHA del commit non valido", "reword");
        }
        auto targetRes = runGit({"rev-parse", "--verify", "--quiet", commitSha + "^{commit}"});
        if (!targetRes)
        {
            return makeError(GitOpsError::Kind::CommitNotFound, targetRes.error().exitCode,
                             "Commit non trovato: " + commitSha, "git rev-parse");
        }
        const std::string targetSha = trimString(*targetRes);

        auto headRes = runGit({"rev-parse", "--verify", "HEAD"});
        if (!headRes)
        {
            return std::unexpected(headRes.error());
        }
        const std::string headSha = trimString(*headRes);

        if (targetSha != headSha)
        {
            if (!runGit({"merge-base", "--is-ancestor", targetSha, headSha}))
            {
                return makeError(GitOpsError::Kind::InvalidOperation, -1,
                                 "Il commit " + targetSha.substr(0, 7) +
                                 " non appartiene al branch corrente: fai prima il checkout del branch che lo contiene",
                                 "reword");
            }
        }

        // Descendants of the target on the current branch, parents before children.
        std::vector<std::string> descendants;
        if (targetSha != headSha)
        {
            auto listRes = runGit({"rev-list", "--topo-order", "--reverse", "--ancestry-path",
                                   targetSha + ".." + headSha});
            if (!listRes)
            {
                return std::unexpected(listRes.error());
            }
            std::size_t start = 0;
            const std::string& out = *listRes;
            while (start < out.size())
            {
                auto end = out.find('\n', start);
                if (end == std::string::npos) end = out.size();
                auto line = trimString(out.substr(start, end - start));
                if (!line.empty()) descendants.push_back(std::move(line));
                start = end + 1;
            }
        }

        // --- 4. Current committer identity (like a real rebase, rewritten commits get a new committer) ---
        auto identRes = runGit({"var", "GIT_COMMITTER_IDENT"});
        if (!identRes)
        {
            return makeError(GitOpsError::Kind::InvalidOperation, identRes.error().exitCode,
                             "Identità git non configurata (user.name / user.email): " + identRes.error().message,
                             "git var");
        }
        const std::string committerIdent = trimString(*identRes);

        // Scratch file inside the git dir, used to feed raw commit objects to `git hash-object`.
        auto gitDirRes = runGit({"rev-parse", "--git-dir"});
        if (!gitDirRes)
        {
            return std::unexpected(gitDirRes.error());
        }
        std::filesystem::path gitDir = gf::core::utf8ToPath(trimString(*gitDirRes));
        if (gitDir.is_relative())
        {
            gitDir = repoPath / gitDir;
        }
        const std::filesystem::path scratchFile = gitDir / "GITFLUX_REWORD_OBJECT";
        struct ScratchCleanup
        {
            std::filesystem::path path;
            ~ScratchCleanup()
            {
                std::error_code ec;
                std::filesystem::remove(path, ec);
            }
        } cleanup{scratchFile};

        std::unordered_map<std::string, std::string> rewritten; // old sha -> new sha

        // Rebuilds a raw commit object: remaps parents, refreshes committer, drops signatures
        // (they would no longer be valid) and optionally replaces the message.
        auto rewriteCommit = [&](const std::string& oldSha,
                                 const std::string* replacementMessage) -> std::expected<std::string, GitOpsError>
        {
            auto rawRes = runGit({"cat-file", "commit", oldSha});
            if (!rawRes)
            {
                return std::unexpected(rawRes.error());
            }
            const std::string& raw = *rawRes;

            const auto sep = raw.find("\n\n");
            const std::string headerBlock = (sep == std::string::npos) ? raw : raw.substr(0, sep + 1);
            const std::string oldMessage = (sep == std::string::npos) ? std::string{} : raw.substr(sep + 2);

            std::string rebuilt;
            rebuilt.reserve(raw.size() + 64);
            bool skipContinuation = false;
            std::size_t pos = 0;
            while (pos < headerBlock.size())
            {
                auto end = headerBlock.find('\n', pos);
                if (end == std::string::npos) end = headerBlock.size();
                const std::string line = headerBlock.substr(pos, end - pos);
                pos = end + 1;

                if (!line.empty() && line.front() == ' ')
                {
                    // Continuation of a multi-line header (gpgsig, mergetag, ...)
                    if (!skipContinuation) rebuilt += line + "\n";
                    continue;
                }
                skipContinuation = false;

                const auto spacePos = line.find(' ');
                const std::string key = line.substr(0, spacePos);
                const std::string value = (spacePos == std::string::npos) ? std::string{} : line.substr(spacePos + 1);

                if (key == "parent")
                {
                    auto it = rewritten.find(value);
                    rebuilt += "parent " + (it != rewritten.end() ? it->second : value) + "\n";
                }
                else if (key == "committer")
                {
                    rebuilt += "committer " + committerIdent + "\n";
                }
                else if (key == "gpgsig" || key == "gpgsig-sha256")
                {
                    skipContinuation = true;
                }
                else if (key == "encoding" && replacementMessage)
                {
                    // The new message is always UTF-8 (git's default): drop any legacy encoding header.
                }
                else
                {
                    rebuilt += line + "\n";
                }
            }
            rebuilt += "\n";
            rebuilt += replacementMessage ? *replacementMessage : oldMessage;

            {
                std::ofstream ofs(scratchFile, std::ios::binary | std::ios::trunc);
                if (!ofs)
                {
                    return makeError(GitOpsError::Kind::StorageError, -1,
                                     "Impossibile scrivere il file temporaneo per il commit riscritto",
                                     "reword");
                }
                ofs.write(rebuilt.data(), static_cast<std::streamsize>(rebuilt.size()));
            }

            auto hashRes = runGit({"hash-object", "-t", "commit", "-w", gf::core::pathToUtf8(scratchFile)});
            if (!hashRes)
            {
                return std::unexpected(hashRes.error());
            }
            return trimString(*hashRes);
        };

        // --- 5. Rewrite the target, then replay every descendant on top of it ---
        auto newTarget = rewriteCommit(targetSha, &message);
        if (!newTarget)
        {
            return std::unexpected(newTarget.error());
        }
        rewritten[targetSha] = *newTarget;

        std::string newHead = *newTarget;
        for (const auto& sha : descendants)
        {
            auto newSha = rewriteCommit(sha, nullptr);
            if (!newSha)
            {
                return std::unexpected(newSha.error());
            }
            rewritten[sha] = *newSha;
            newHead = *newSha;
        }

        // --- 6. Atomically move HEAD (and the branch it points to), guarded by the old value ---
        auto updateRes = runGit({"update-ref", "-m", "GitFlux: reword " + targetSha.substr(0, 7),
                                 "HEAD", newHead, headSha});
        if (!updateRes)
        {
            return std::unexpected(updateRes.error());
        }

        return *newTarget;
    }

    namespace
    {
        struct FastForwardPlan
        {
            std::string targetRef;   // refs/heads/<target>
            std::string targetSha;
            std::string sourceSha;
            bool possible = false;   // target is a strict ancestor of source
        };

        [[nodiscard]] std::expected<FastForwardPlan, gf::git::ports::GitOpsError>
        planFastForward(gf::git::ports::IGitCommandExecutor& executor,
                        const std::filesystem::path& repoPath,
                        const std::string& targetBranch,
                        const std::string& sourceRef)
        {
            using gf::git::ports::GitOpsError;
            if (targetBranch.empty() || sourceRef.empty() ||
                targetBranch.front() == '-' || sourceRef.front() == '-')
            {
                return std::unexpected(GitOpsError{
                    .kind = GitOpsError::Kind::InvalidBranchName,
                    .exitCode = -1,
                    .message = "Branch di origine o destinazione non valido",
                    .command = "fast-forward"
                });
            }

            FastForwardPlan plan;
            plan.targetRef = "refs/heads/" + targetBranch;

            auto tgt = runGitCapture(executor, repoPath, {"rev-parse", "--verify", "--quiet", plan.targetRef + "^{commit}"});
            if (!tgt)
            {
                return std::unexpected(GitOpsError{
                    .kind = GitOpsError::Kind::BranchNotFound,
                    .exitCode = tgt.error().exitCode,
                    .message = "Branch locale non trovato: " + targetBranch,
                    .command = "git rev-parse"
                });
            }
            auto src = runGitCapture(executor, repoPath, {"rev-parse", "--verify", "--quiet", sourceRef + "^{commit}"});
            if (!src)
            {
                return std::unexpected(GitOpsError{
                    .kind = GitOpsError::Kind::BranchNotFound,
                    .exitCode = src.error().exitCode,
                    .message = "Riferimento non trovato: " + sourceRef,
                    .command = "git rev-parse"
                });
            }
            plan.targetSha = *tgt;
            plan.sourceSha = *src;

            if (plan.targetSha != plan.sourceSha)
            {
                // exit 0 => ancestor, exit 1 => not ancestor (diverged or ahead)
                plan.possible = runGitCapture(executor, repoPath,
                                              {"merge-base", "--is-ancestor", plan.targetSha, plan.sourceSha})
                    .has_value();
            }
            return plan;
        }
    } // namespace

    std::expected<bool, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::canFastForwardBranch(const std::filesystem::path& repoPath,
                                                 const std::string& targetBranch,
                                                 const std::string& sourceRef)
    {
        auto plan = planFastForward(m_executor, repoPath, targetBranch, sourceRef);
        if (!plan)
        {
            return std::unexpected(plan.error());
        }
        return plan->possible;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::fastForwardBranch(const std::filesystem::path& repoPath,
                                              const std::string& targetBranch,
                                              const std::string& sourceRef)
    {
        using gf::git::ports::GitOpsError;

        auto plan = planFastForward(m_executor, repoPath, targetBranch, sourceRef);
        if (!plan)
        {
            return std::unexpected(plan.error());
        }
        if (plan->targetSha == plan->sourceSha)
        {
            return {}; // already up to date
        }
        if (!plan->possible)
        {
            return std::unexpected(GitOpsError{
                .kind = GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Fast-forward impossibile: '" + targetBranch + "' non è un antenato di '" + sourceRef +
                           "' (i branch sono divergenti)",
                .command = "fast-forward"
            });
        }

        // Checked-out branch: update the working tree too, git refuses if local changes would be overwritten.
        auto headRef = runGitCapture(m_executor, repoPath, {"symbolic-ref", "-q", "HEAD"});
        if (headRef && *headRef == plan->targetRef)
        {
            auto merged = runGitCapture(m_executor, repoPath, {"merge", "--ff-only", plan->sourceSha});
            if (!merged)
            {
                auto err = merged.error();
                err.command = "git merge --ff-only";
                return std::unexpected(std::move(err));
            }
            return {};
        }

        // Branch checked out in another worktree: moving the ref would desync that worktree.
        if (auto worktrees = runGitCapture(m_executor, repoPath, {"worktree", "list", "--porcelain"}))
        {
            const std::string needle = "branch " + plan->targetRef;
            std::size_t start = 0;
            const std::string& out = *worktrees;
            while (start < out.size())
            {
                auto end = out.find('\n', start);
                if (end == std::string::npos) end = out.size();
                if (trimString(out.substr(start, end - start)) == needle)
                {
                    return std::unexpected(GitOpsError{
                        .kind = GitOpsError::Kind::InvalidOperation,
                        .exitCode = -1,
                        .message = "Il branch '" + targetBranch + "' è in checkout in un altro worktree",
                        .command = "fast-forward"
                    });
                }
                start = end + 1;
            }
        }

        auto updated = runGitCapture(m_executor, repoPath,
                                     {"update-ref", "-m", "GitFlux: fast-forward " + targetBranch + " to " + sourceRef,
                                      plan->targetRef, plan->sourceSha, plan->targetSha});
        if (!updated)
        {
            return std::unexpected(updated.error());
        }
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::createWorktree(const std::filesystem::path& repoPath,
                                           const std::filesystem::path& worktreePath,
                                           const std::string& commitOrBranch)
    {
        std::vector<std::string> args = {"worktree", "add", worktreePath.string()};
        if (!commitOrBranch.empty())
        {
            args.push_back(commitOrBranch);
        }

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{30'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile creare il worktree: " + execResult.error().message,
                .command = "git worktree"
            });
        }

        if (execResult->exitCode != 0)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = execResult->exitCode,
                .message = execResult->stderrBytes.empty() ? execResult->stdoutBytes : execResult->stderrBytes,
                .command = "git worktree"
            });
        }

        return {};
    }

    std::expected<gf::git::bo::CloneRepositoryResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::cloneRepository(const gf::git::bo::CloneRepositoryOptions& options)
    {
        if (options.remoteUrl.empty())
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::InvalidRemoteUrl,
                .exitCode = -1,
                .message = "L'URL del repository remoto non può essere vuoto",
                .command = "git clone"
            });
        }

        if (options.destinationPath.empty())
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso di destinazione non può essere vuoto",
                .command = "git clone"
            });
        }

        std::error_code ec;
        if (std::filesystem::exists(options.destinationPath, ec) && !std::filesystem::is_empty(
            options.destinationPath, ec))
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::DestinationAlreadyExists,
                .exitCode = -1,
                .message = "La cartella di destinazione esiste già e non è vuota: " + options.destinationPath.string(),
                .command = "git clone"
            });
        }

        // Ensure parent directory exists
        const auto parentDir = options.destinationPath.parent_path();
        if (!parentDir.empty() && !std::filesystem::exists(parentDir, ec))
        {
            std::filesystem::create_directories(parentDir, ec);
        }

        std::vector<std::string> args = {"clone", "--progress"};
        if (options.recursiveSubmodules)
        {
            args.emplace_back("--recurse-submodules");
        }
        if (options.branch.has_value() && !options.branch->empty())
        {
            args.emplace_back("-b");
            args.push_back(*options.branch);
        }
        args.push_back(options.remoteUrl);
        args.push_back(options.destinationPath.string());

        const auto workingDir = (!parentDir.empty() && std::filesystem::exists(parentDir, ec))
                                    ? parentDir
                                    : std::filesystem::temp_directory_path();

        std::string lineBuffer;
        auto parseProgressChunk = [&lineBuffer, &options](std::string_view chunk)
        {
            if (!options.progressCallback) return;
            lineBuffer.append(chunk);

            size_t pos = 0;
            while (pos < lineBuffer.size())
            {
                size_t delim = lineBuffer.find_first_of("\r\n", pos);
                if (delim == std::string::npos)
                {
                    break;
                }

                std::string_view line(lineBuffer.data() + pos, delim - pos);
                while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.remove_prefix(1);
                while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.remove_suffix(1);

                if (!line.empty())
                {
                    int percent = -1;
                    auto pctPos = line.find('%');
                    if (pctPos != std::string_view::npos && pctPos > 0)
                    {
                        size_t startDigits = pctPos;
                        while (startDigits > 0 && std::isdigit(static_cast<unsigned char>(line[startDigits - 1])))
                        {
                            --startDigits;
                        }
                        if (startDigits < pctPos)
                        {
                            try
                            {
                                percent = std::stoi(std::string(line.substr(startDigits, pctPos - startDigits)));
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                    options.progressCallback(percent, line);
                }

                pos = delim + 1;
            }

            if (pos > 0)
            {
                lineBuffer.erase(0, pos);
            }
        };

        gf::git::bo::GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = workingDir,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = options.timeout,
            .onStderrChunk = std::move(parseProgressChunk)
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire il clone: " + execResult.error().message,
                .command = "git clone"
            });
        }

        if (execResult->exitCode != 0)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            const auto& err = execResult->stderrBytes;

            if (err.find("already exists and is not an empty directory") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::DestinationAlreadyExists;
            }
            else if (err.find("Could not resolve host") != std::string::npos ||
                err.find("Authentication failed") != std::string::npos ||
                err.find("Permission denied") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::NetworkAuthenticationFailed;
            }
            else if (err.find("does not appear to be a git repository") != std::string::npos ||
                err.find("not found") != std::string::npos ||
                err.find("fatal: repository") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::RemoteNotFound;
            }

            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = kind,
                .exitCode = execResult->exitCode,
                .message = err.empty() ? execResult->stdoutBytes : err,
                .command = "git clone"
            });
        }

        // Detect initial branch of cloned repository
        std::string branchName = "main";
        gf::git::bo::GitCommand branchCmd{
            .args = {"branch", "--show-current"},
            .workingDirectory = options.destinationPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{5'000}
        };
        if (auto branchRes = m_executor.execute(branchCmd); branchRes && branchRes->exitCode == 0)
        {
            auto out = branchRes->stdoutBytes;
            while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
            {
                out.pop_back();
            }
            if (!out.empty())
            {
                branchName = std::move(out);
            }
        }

        std::string displayName = options.destinationPath.filename().string();

        return gf::git::bo::CloneRepositoryResult{
            .repositoryPath = options.destinationPath,
            .displayName = std::move(displayName),
            .initialBranch = std::move(branchName)
        };
    }

    std::expected<std::string, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getFileContent(const std::filesystem::path& repoPath,
                                           const std::string& revision,
                                           const std::string& filePath)
    {
        if (filePath.empty())
        {
            return std::string{};
        }

        constexpr std::size_t kMaxContentBytes = 2 * 1024 * 1024;
        auto stripUtf8Bom = [](std::string& text)
        {
            if (text.size() >= 3 &&
                static_cast<unsigned char>(text[0]) == 0xEF &&
                static_cast<unsigned char>(text[1]) == 0xBB &&
                static_cast<unsigned char>(text[2]) == 0xBF)
            {
                text.erase(0, 3);
            }
        };

        if (revision.empty() || revision == "WORKTREE")
        {
            const auto fullPath = repoPath / gf::core::utf8ToPath(filePath);
            std::error_code ec;
            if (!std::filesystem::exists(fullPath, ec) || !std::filesystem::is_regular_file(fullPath, ec))
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::PathNotFound,
                    .exitCode = -1,
                    .message = "File non trovato nella working tree: " + filePath,
                    .command = "read file"
                });
            }

            const auto fileSize = std::filesystem::file_size(fullPath, ec);
            if (!ec && fileSize > kMaxContentBytes)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                    .exitCode = -1,
                    .message = "File troppo grande (> 2 MiB) per l'anteprima testuale",
                    .command = "read file"
                });
            }

            std::ifstream ifs(fullPath, std::ios::binary);
            if (!ifs)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::StorageError,
                    .exitCode = -1,
                    .message = "Impossibile leggere il file: " + filePath,
                    .command = "read file"
                });
            }

            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            if (content.size() > kMaxContentBytes)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                    .exitCode = -1,
                    .message = "File troppo grande (> 2 MiB) per l'anteprima testuale",
                    .command = "read file"
                });
            }
            if (content.find('\0') != std::string::npos)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                    .exitCode = -1,
                    .message = "Il file contiene dati binari",
                    .command = "read file"
                });
            }

            stripUtf8Bom(content);
            return content;
        }

        std::string normPath = filePath;
        std::replace(normPath.begin(), normPath.end(), '\\', '/');
        const std::string targetRev = (revision == "INDEX") ? std::string(":0") : revision;

        gf::git::bo::GitCommand cmd{
            .args = {"show", targetRev + ":" + normPath},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{15'000}
        };

        auto execResult = m_executor.execute(cmd);
        if (!execResult)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = -1,
                .message = "Impossibile eseguire git show: " + execResult.error().message,
                .command = "git show"
            });
        }

        if (execResult->exitCode != 0)
        {
            const std::string& err = execResult->stderrBytes;
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (err.find("does not exist") != std::string::npos ||
                err.find("non esiste") != std::string::npos ||
                err.find("exists on disk") != std::string::npos ||
                err.find("Invalid object name") != std::string::npos ||
                err.find("Path '") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::PathNotFound;
            }
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = kind,
                .exitCode = execResult->exitCode,
                .message = err.empty() ? execResult->stdoutBytes : err,
                .command = "git show"
            });
        }

        std::string content = std::move(execResult->stdoutBytes);
        if (content.size() > kMaxContentBytes)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "File troppo grande (> 2 MiB) per l'anteprima testuale",
                .command = "git show"
            });
        }
        if (content.find('\0') != std::string::npos)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il file contiene dati binari",
                .command = "git show"
            });
        }

        stripUtf8Bom(content);
        return content;
    }
} // namespace gf::infra::git
