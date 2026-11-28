#include "infrastructure/git/native/native_git_ops_internal.hpp"
#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/shared/utf8_path.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace gf::infra::git
{
    namespace
    {
        [[nodiscard]] gf::git::bo::FileChangeKind mapStatusCharToKind(const char code) noexcept
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

        [[nodiscard]] std::string extractSummaryLine(const std::string_view message)
        {
            const auto newlinePos = message.find_first_of("\r\n");
            if (newlinePos == std::string_view::npos)
            {
                return std::string(message);
            }
            return std::string(message.substr(0, newlinePos));
        }

        [[nodiscard]] gf::git::ports::GitOpsError
        classifyIndexLockedOrPath(const gf::git::bo::GitProcessResult& exec, const std::string& commandLabel)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            if (exec.stderrBytes.find("index.lock") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::IndexLocked;
            }
            else if (exec.stderrBytes.find("did not match any files") != std::string::npos ||
                     exec.stderrBytes.find("non corrisponde ad alcun file") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::PathNotFound;
            }
            return {kind, exec.exitCode, exec.stderrBytes, commandLabel};
        }

        [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
        unstageWithFallback(gf::git::ports::IGitCommandExecutor& executor,
                            const std::filesystem::path& repoPath,
                            std::vector<std::string> restoreArgs,
                            std::vector<std::string> fallbackRmArgs,
                            const std::string_view errorVerb)
        {
            auto exec = detail::runGitRaw(executor, repoPath, std::move(restoreArgs),
                                          errorVerb, "git restore --staged");
            if (!exec) return std::unexpected(exec.error());
            if (exec->exitCode == 0) return {};

            if (exec->stderrBytes.find("HEAD") != std::string::npos)
            {
                if (const auto rmResult = executor.execute(gf::git::bo::GitCommand{
                    .args = std::move(fallbackRmArgs),
                    .workingDirectory = repoPath,
                    .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                    .timeout = std::chrono::milliseconds{30'000}
                }); rmResult && rmResult->exitCode == 0)
                {
                    return {};
                }
            }

            const auto kind = exec->stderrBytes.find("index.lock") != std::string::npos
                ? gf::git::ports::GitOpsError::Kind::IndexLocked
                : gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = kind,
                .exitCode = exec->exitCode,
                .message = exec->stderrBytes,
                .command = "git restore --staged"
            });
        }
    } // namespace

    NativeRepositoryGitOps::NativeRepositoryGitOps(gf::git::ports::IGitCommandExecutor& executor) noexcept
        : m_executor(executor)
    {
    }

    std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getWorkingTreeStatus(const std::filesystem::path& repoPath)
    {
        const gf::git::bo::GitCommand cmd{
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

        auto res = detail::runGitChecked(m_executor, repoPath, std::move(args), "git add", "git add",
                                         classifyIndexLockedOrPath);
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::stageAll(const std::filesystem::path& repoPath)
    {
        auto res = detail::runGitChecked(m_executor, repoPath, {"add", "-A"}, "git add -A", "git add -A",
                                         classifyIndexLockedOrPath);
        if (!res) return std::unexpected(res.error());
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

        std::vector<std::string> rmArgs{"rm", "--cached", "--"};
        rmArgs.insert(rmArgs.end(), filePaths.begin(), filePaths.end());

        return unstageWithFallback(m_executor, repoPath, std::move(restoreArgs),
                                   std::move(rmArgs), "git restore --staged");
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::unstageAll(const std::filesystem::path& repoPath)
    {
        return unstageWithFallback(m_executor, repoPath,
                                   {"restore", "--staged", "."},
                                   {"rm", "--cached", "-r", "--", "."},
                                   "git restore --staged .");
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
            const gf::git::bo::GitCommand restoreCmd{
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
            const gf::git::bo::GitCommand cleanCmd{
                .args = {"clean", "-f", "-d", "-q", "--", path},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                .timeout = std::chrono::milliseconds{30'000}
            };

            if (const auto cleanRes = m_executor.execute(cleanCmd); cleanRes && cleanRes->exitCode == 0)
            {
                continue;
            }

            // 3. Fallback to direct filesystem removal if untracked file still exists
            if (std::error_code fsEc; std::filesystem::exists(repoPath / path, fsEc))
            {
                const auto targetPath = repoPath / path;
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
            const gf::git::bo::GitCommand diffCheck{
                .args = {"diff", "--cached", "--quiet"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{10'000}
            };
            if (const auto diffRes = m_executor.execute(diffCheck); diffRes && diffRes->exitCode == 0)
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

        const gf::git::bo::GitCommand commitCmd{
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
        const gf::git::bo::GitCommand revParseCmd{
            .args = {"rev-parse", "HEAD"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{10'000}
        };
        auto revParseRes = m_executor.execute(revParseCmd);
        std::string commitSha = revParseRes ? detail::trimString(revParseRes->stdoutBytes) : "";
        std::string shortSha = commitSha.size() >= 8 ? commitSha.substr(0, 8) : commitSha;

        // Branch resolution
        const gf::git::bo::GitCommand branchCmd{
            .args = {"rev-parse", "--abbrev-ref", "HEAD"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{10'000}
        };
        auto branchRes = m_executor.execute(branchCmd);
        std::string branch = branchRes ? detail::trimString(branchRes->stdoutBytes) : "";

        return gf::git::bo::CommitResult{
            .commitSha = std::move(commitSha),
            .shortSha = std::move(shortSha),
            .summary = extractSummaryLine(options.message),
            .branch = std::move(branch)
        };
    }
} // namespace gf::infra::git
