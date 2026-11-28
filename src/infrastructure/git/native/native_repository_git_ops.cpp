#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "core/modules/git/bl/https_credential.hpp"
#include "core/modules/git/bl/remote_access_diagnostics.hpp"
#include "core/shared/utf8_path.hpp"
#include "core/modules/git/bl/commit_log_parser.hpp"
#include "core/modules/git/bl/conflict_marker_parser.hpp"
#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/modules/git/bl/unified_diff_parser.hpp"
#include "core/modules/git/bo/git_command.hpp"
#include "core/shared/string_utils.hpp"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

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

        [[nodiscard]] std::string trimString(const std::string_view str)
        {
            return std::string(core::trim(str));
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

        [[nodiscard]] std::string pickErrorMessage(const gf::git::bo::GitProcessResult& res)
        {
            return res.stderrBytes.empty() ? res.stdoutBytes : res.stderrBytes;
        }

        [[nodiscard]] std::expected<gf::git::bo::GitProcessResult, gf::git::ports::GitOpsError>
        runGitRaw(gf::git::ports::IGitCommandExecutor& executor,
                  const std::filesystem::path& repoPath,
                  std::vector<std::string> args,
                  const std::string_view errorVerb,
                  const std::string_view commandLabel,
                  const gf::git::bo::GitLockPolicy lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                  const int timeoutMs = 30'000,
                  const gf::git::bo::HttpsCredentialLoader& httpsCredential = {})
        {
            gf::git::bo::GitCommand cmd{
                .args = std::move(args),
                .workingDirectory = repoPath,
                .lockPolicy = lockPolicy,
                .timeout = std::chrono::milliseconds{timeoutMs}
            };
            if (httpsCredential)
            {
                gf::git::bl::applyHttpsCredential(cmd, httpsCredential);
            }
            auto exec = executor.execute(cmd);
            if (!exec)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                    .exitCode = -1,
                    .message = "Impossibile eseguire " + std::string(errorVerb) + ": " + exec.error().message,
                    .command = std::string(commandLabel)
                });
            }
            return std::move(*exec);
        }

        template <typename ErrorClassifier>
        [[nodiscard]] std::expected<gf::git::bo::GitProcessResult, gf::git::ports::GitOpsError>
        runGitChecked(gf::git::ports::IGitCommandExecutor& executor,
                      const std::filesystem::path& repoPath,
                      std::vector<std::string> args,
                      const std::string_view errorVerb,
                      const std::string_view commandLabel,
                      ErrorClassifier&& classifyError,
                      const gf::git::bo::GitLockPolicy lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                      const int timeoutMs = 30'000,
                      const gf::git::bo::HttpsCredentialLoader& httpsCredential = {})
        {
            auto exec = runGitRaw(executor, repoPath, std::move(args), errorVerb, commandLabel,
                                  lockPolicy, timeoutMs, httpsCredential);
            if (!exec) return exec;
            if (exec->exitCode != 0)
            {
                return std::unexpected(classifyError(*exec, std::string(commandLabel)));
            }
            return exec;
        }

        [[nodiscard]] gf::git::ports::GitOpsError
        defaultGitFailure(const gf::git::bo::GitProcessResult& exec, const std::string& commandLabel)
        {
            return gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = exec.exitCode,
                .message = pickErrorMessage(exec),
                .command = commandLabel
            };
        }

        [[nodiscard]] bool isGitObjectPathMissing(const std::string& err) noexcept
        {
            return err.find("does not exist") != std::string::npos ||
                   err.find("non esiste") != std::string::npos ||
                   err.find("exists on disk") != std::string::npos ||
                   err.find("Not a valid object name") != std::string::npos ||
                   err.find("Invalid object name") != std::string::npos ||
                   err.find("Path '") != std::string::npos;
        }

        [[nodiscard]] gf::git::ports::GitOpsError
        classifyRemoteFailure(const gf::git::bo::GitProcessResult& ex, const std::string& label)
        {
            auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
            const auto& err = ex.stderrBytes;
            if (err.find("already exists and is not an empty directory") != std::string::npos)
            {
                kind = gf::git::ports::GitOpsError::Kind::DestinationAlreadyExists;
            }
            else if (err.find("rejected") != std::string::npos ||
                err.find("non-fast-forward") != std::string::npos ||
                err.find("fetch first") != std::string::npos ||
                err.find("stale info") != std::string::npos ||
                (label == "git push" && err.find("already exists") != std::string::npos))
            {
                kind = gf::git::ports::GitOpsError::Kind::RemoteRejected;
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
            return gf::git::ports::GitOpsError{
                .kind = kind,
                .exitCode = ex.exitCode,
                .message = pickErrorMessage(ex),
                .command = label
            };
        }

        [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
        runGitVoid(gf::git::ports::IGitCommandExecutor& executor,
                   const std::filesystem::path& repoPath,
                   std::vector<std::string> args,
                   const std::string_view errorVerb,
                   const std::string_view commandLabel,
                   const gf::git::bo::GitLockPolicy lockPolicy = gf::git::bo::GitLockPolicy::Standard,
                   const int timeoutMs = 30'000)
        {
            auto res = runGitChecked(executor, repoPath, std::move(args), errorVerb, commandLabel,
                                     defaultGitFailure, lockPolicy, timeoutMs);
            if (!res) return std::unexpected(res.error());
            return {};
        }

        /// Runs a git command in `repoPath`; returns trimmed stdout on exit code 0, an error otherwise.
        [[nodiscard]] std::expected<std::string, gf::git::ports::GitOpsError>
        runGitCapture(gf::git::ports::IGitCommandExecutor& executor,
                      const std::filesystem::path& repoPath,
                      std::vector<std::string> args)
        {
            const std::string label = "git " + (args.empty() ? std::string{} : args.front());
            auto exec = runGitChecked(executor, repoPath, std::move(args), label, label, defaultGitFailure);
            if (!exec) return std::unexpected(exec.error());
            return trimString(exec->stdoutBytes);
        }

        [[nodiscard]] std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
        parseSingleFileDiff(gf::git::ports::IGitCommandExecutor& executor,
                            const std::filesystem::path& repoPath,
                            std::vector<std::string> args,
                            const std::string& filePath,
                            const std::string_view commandLabel,
                            const std::string_view errorVerb,
                            const bool allowExitCode1 = false,
                            const bool fillEmptyOldPath = true)
        {
            auto exec = runGitRaw(executor, repoPath, std::move(args), errorVerb, commandLabel,
                                  gf::git::bo::GitLockPolicy::NoOptionalLocks);
            if (!exec) return std::unexpected(exec.error());
            if ((allowExitCode1 && exec->exitCode > 1) || (!allowExitCode1 && exec->exitCode != 0))
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                    .exitCode = exec->exitCode,
                    .message = exec->stderrBytes,
                    .command = std::string(commandLabel)
                });
            }
            if (exec->stdoutBytes.empty())
            {
                return gf::git::bo::FileDiff{.oldPath = filePath, .newPath = filePath};
            }
            if (auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(exec->stdoutBytes);
                diffOpt.has_value())
            {
                if (diffOpt->newPath.empty()) diffOpt->newPath = filePath;
                if (fillEmptyOldPath && diffOpt->oldPath.empty()) diffOpt->oldPath = filePath;
                return std::move(*diffOpt);
            }
            return gf::git::bo::FileDiff{.oldPath = filePath, .newPath = filePath};
        }

        struct NumStatEntry
        {
            int additions = 0;
            int deletions = 0;
            bool isBinary = false;
        };

        [[nodiscard]] gf::git::bo::FileChangeStatus parseFileChangeStatusChar(const std::string_view statusStr) noexcept
        {
            switch (statusStr.empty() ? 'M' : statusStr.front())
            {
            case 'A': return gf::git::bo::FileChangeStatus::Added;
            case 'D': return gf::git::bo::FileChangeStatus::Deleted;
            case 'R': return gf::git::bo::FileChangeStatus::Renamed;
            case 'C': return gf::git::bo::FileChangeStatus::Copied;
            default: return gf::git::bo::FileChangeStatus::Modified;
            }
        }

        [[nodiscard]] std::string normalizeNumstatPath(const std::string& raw)
        {
            const auto arrow = raw.find(" => ");
            if (arrow == std::string::npos) return raw;
            const auto lbrace = raw.rfind('{', arrow);
            const auto rbrace = raw.find('}', arrow + 4);
            if (lbrace != std::string::npos && rbrace != std::string::npos)
            {
                std::string out;
                out.append(raw, 0, lbrace);
                out.append(raw, arrow + 4, rbrace - (arrow + 4));
                out.append(raw, rbrace + 1, std::string::npos);
                std::size_t pos = 0;
                while ((pos = out.find("//", pos)) != std::string::npos)
                {
                    out.erase(pos, 1);
                }
                if (!out.empty() && out.front() == '/') out.erase(0, 1);
                return out;
            }
            return raw.substr(arrow + 4);
        }

        [[nodiscard]] bool parseNumstatLine(const std::string_view nline,
                                            std::string& outPath,
                                            NumStatEntry& outEntry)
        {
            const auto tab1 = nline.find('\t');
            if (tab1 == std::string_view::npos) return false;
            const auto tab2 = nline.find('\t', tab1 + 1);
            if (tab2 == std::string_view::npos) return false;
            const std::string_view addStr = nline.substr(0, tab1);
            const std::string_view delStr = nline.substr(tab1 + 1, tab2 - tab1 - 1);
            outPath = std::string(nline.substr(tab2 + 1));
            outEntry = {};
            if (addStr == "-" || delStr == "-")
            {
                outEntry.isBinary = true;
            }
            else
            {
                try
                {
                    outEntry.additions = std::stoi(std::string(addStr));
                    outEntry.deletions = std::stoi(std::string(delStr));
                }
                catch (...)
                {
                    outEntry.additions = 0;
                    outEntry.deletions = 0;
                }
            }
            return true;
        }

        [[nodiscard]] bool parseNameStatusLine(const std::string_view dline,
                                               gf::git::bo::CommitFileChange& outChange)
        {
            const auto tab1 = dline.find('\t');
            if (tab1 == std::string_view::npos) return false;
            const std::string_view statusStr = dline.substr(0, tab1);
            const std::string_view rest = dline.substr(tab1 + 1);
            outChange = {};
            outChange.status = parseFileChangeStatusChar(statusStr);
            if (outChange.status == gf::git::bo::FileChangeStatus::Renamed ||
                outChange.status == gf::git::bo::FileChangeStatus::Copied)
            {
                if (const auto tab2 = rest.find('\t'); tab2 != std::string_view::npos)
                {
                    outChange.oldPath = std::string(rest.substr(0, tab2));
                    outChange.path = std::string(rest.substr(tab2 + 1));
                }
                else
                {
                    outChange.path = std::string(rest);
                }
            }
            else
            {
                outChange.path = std::string(rest);
            }
            return !outChange.path.empty();
        }

        [[nodiscard]] std::vector<std::string>
        readConflictedFiles(gf::git::ports::IGitCommandExecutor& executor,
                            const std::filesystem::path& repoPath)
        {
            std::vector<std::string> files;
            const auto diffUCmd = executor.execute(gf::git::bo::GitCommand{
                .args = {"diff", "--name-only", "--diff-filter=U"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{10'000}
            });
            if (!diffUCmd || diffUCmd->exitCode != 0)
            {
                return files;
            }
            std::string_view out(diffUCmd->stdoutBytes);
            while (!out.empty())
            {
                const auto nl = out.find('\n');
                std::string_view line = nl == std::string_view::npos ? out : out.substr(0, nl);
                out = nl == std::string_view::npos ? std::string_view{} : out.substr(nl + 1);
                line = core::trimTrailingNewlines(line);
                if (!line.empty())
                {
                    files.emplace_back(line);
                }
            }
            return files;
        }

        [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
        unstageWithFallback(gf::git::ports::IGitCommandExecutor& executor,
                            const std::filesystem::path& repoPath,
                            std::vector<std::string> restoreArgs,
                            std::vector<std::string> fallbackRmArgs,
                            const std::string_view errorVerb)
        {
            auto exec = runGitRaw(executor, repoPath, std::move(restoreArgs), errorVerb, "git restore --staged");
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

        [[nodiscard]] bool hasRebaseConflictOutput(const std::string_view combinedOutput) noexcept
        {
            return combinedOutput.find("CONFLICT") != std::string_view::npos ||
                combinedOutput.find("Automatic merge failed") != std::string_view::npos ||
                combinedOutput.find("conflitti") != std::string_view::npos ||
                combinedOutput.find("Resolve all conflicts") != std::string_view::npos ||
                combinedOutput.find("could not apply") != std::string_view::npos ||
                combinedOutput.find("unmerged") != std::string_view::npos;
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

        [[nodiscard]] bool isStashNotFoundError(const std::string_view output) noexcept
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
                if (std::error_code ec; !std::filesystem::exists(p, ec)) return {};
                std::ifstream ifs(p);
                if (std::string s; ifs >> s) return s;
                return {};
            };
            auto readStepCount = [&readFirstWord](const std::filesystem::path& p) -> int
            {
                const std::string s = readFirstWord(p);
                if (s.empty()) return 0;
                try { return std::stoi(s); }
                catch (...) { return 0; }
            };

            const auto rebaseMergeDir = repoPath / ".git" / "rebase-merge";
            const auto rebaseApplyDir = repoPath / ".git" / "rebase-apply";

            if (std::error_code ec; std::filesystem::exists(rebaseMergeDir, ec))
            {
                if (const int step = readStepCount(rebaseMergeDir / "msgnum"); step > 0) result.currentStep = step;
                if (const int total = readStepCount(rebaseMergeDir / "end"); total > 0) result.totalSteps = total;
                if (std::string sha = readFirstWord(rebaseMergeDir / "stopped-sha"); !sha.empty())
                {
                    result.currentCommitSha = std::move(sha);
                }
            }
            else if (std::filesystem::exists(rebaseApplyDir, ec))
            {
                if (const int step = readStepCount(rebaseApplyDir / "next"); step > 0) result.currentStep = step;
                if (const int total = readStepCount(rebaseApplyDir / "last"); total > 0) result.totalSteps = total;
                if (std::string sha = readFirstWord(rebaseApplyDir / "original-commit"); !sha.empty())
                {
                    result.currentCommitSha = std::move(sha);
                }
            }

            result.conflictedFiles = readConflictedFiles(executor, repoPath);
        }

        [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
        applyOrPopStash(gf::git::ports::IGitCommandExecutor& executor,
                        const std::filesystem::path& repoPath,
                        const std::string_view verb,
                        const gf::git::bo::StashApplyOptions& options)
        {
            std::vector<std::string> args{"stash", std::string(verb)};
            if (options.restoreIndex) args.emplace_back("--index");
            args.emplace_back("stash@{" + std::to_string(options.stashIndex) + "}");
            const std::string cmdLabel = "git stash " + std::string(verb);
            auto res = runGitChecked(
                executor, repoPath, std::move(args), "applicare lo stash", cmdLabel,
                [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
                {
                    const std::string combined = exec.stdoutBytes + "\n" + exec.stderrBytes;
                    auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                    if (isStashNotFoundError(combined))
                    {
                        kind = gf::git::ports::GitOpsError::Kind::StashNotFound;
                    }
                    else if (combined.find("CONFLICT") != std::string::npos ||
                        combined.find("conflict") != std::string::npos)
                    {
                        kind = gf::git::ports::GitOpsError::Kind::MergeConflict;
                    }
                    return gf::git::ports::GitOpsError{kind, exec.exitCode, pickErrorMessage(exec), label};
                });
            if (!res) return std::unexpected(res.error());
            return {};
        }

        template <typename InProgressFn>
        [[nodiscard]] std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
        executeRebaseStep(gf::git::ports::IGitCommandExecutor& executor,
                          const std::filesystem::path& repoPath,
                          std::vector<std::string> args,
                          const std::string_view errorVerb,
                          const std::string_view commandLabel,
                          const bool checkNoRebaseInProgress,
                          const bool checkDirtyTree,
                          const gf::git::bo::RebaseStatus successWhenStillInProgress,
                          InProgressFn&& inProgressCheck)
        {
            auto exec = runGitRaw(executor, repoPath, std::move(args), errorVerb, commandLabel,
                                  gf::git::bo::GitLockPolicy::Standard, 60'000);
            if (!exec) return std::unexpected(exec.error());

            gf::git::bo::RebaseResult result;
            const std::string combinedOutput = exec->stdoutBytes + "\n" + exec->stderrBytes;

            if (exec->exitCode == 0)
            {
                if (successWhenStillInProgress == gf::git::bo::RebaseStatus::UpToDate)
                {
                    result.status = (combinedOutput.find("is up to date") != std::string::npos ||
                                     combinedOutput.find("è aggiornato") != std::string::npos)
                                        ? gf::git::bo::RebaseStatus::UpToDate
                                        : gf::git::bo::RebaseStatus::Finished;
                }
                else
                {
                    result.status = inProgressCheck() ? successWhenStillInProgress : gf::git::bo::RebaseStatus::Finished;
                }
                result.message = exec->stdoutBytes;
                return result;
            }

            if (checkNoRebaseInProgress &&
                (combinedOutput.find("no rebase in progress") != std::string::npos ||
                 combinedOutput.find("nessun rebase in corso") != std::string::npos))
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    gf::git::ports::GitOpsError::Kind::NoRebaseInProgress,
                    exec->exitCode, combinedOutput, std::string(commandLabel)
                });
            }

            if (hasRebaseConflictOutput(combinedOutput))
            {
                result.status = gf::git::bo::RebaseStatus::Conflict;
                result.message = combinedOutput;
                populateRebaseProgressAndConflicts(repoPath, executor, result);
                return result;
            }

            if (checkDirtyTree &&
                (combinedOutput.find("cannot rebase: You have unstaged changes") != std::string::npos ||
                 combinedOutput.find("working tree clean") != std::string::npos ||
                 combinedOutput.find("unstaged changes") != std::string::npos))
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    gf::git::ports::GitOpsError::Kind::DirtyWorkingTree,
                    exec->exitCode, combinedOutput, std::string(commandLabel)
                });
            }

            return std::unexpected(defaultGitFailure(*exec, std::string(commandLabel)));
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

        auto res = runGitChecked(m_executor, repoPath, std::move(args), "git add", "git add",
                                 classifyIndexLockedOrPath);
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::stageAll(const std::filesystem::path& repoPath)
    {
        auto res = runGitChecked(m_executor, repoPath, {"add", "-A"}, "git add -A", "git add -A",
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
        std::string commitSha = revParseRes ? trimString(revParseRes->stdoutBytes) : "";
        std::string shortSha = commitSha.size() >= 8 ? commitSha.substr(0, 8) : commitSha;

        // Branch resolution
        const gf::git::bo::GitCommand branchCmd{
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
                                        const gf::git::bo::DiffScope scope)
    {
        return getFileDiffWithContext(repoPath, filePath, scope, 3);
    }

    std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getFileDiffWithContext(const std::filesystem::path& repoPath,
                                                   const std::string& filePath,
                                                   const gf::git::bo::DiffScope scope,
                                                   const int contextLines)
    {
        const int effectiveContext = contextLines < 0 ? 999999 : contextLines;
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
                return gf::git::bo::FileDiff{.oldPath = filePath, .newPath = filePath};
            }
            // Untracked: use --no-index against /dev/null
            const auto untrackedFullPath = repoPath / core::utf8ToPath(filePath);
            if (const auto fileSize = std::filesystem::file_size(untrackedFullPath, ec);
                !ec && fileSize > 2 * 1024 * 1024)
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

        // For --no-index, exitCode 1 indicates differences are present
        return parseSingleFileDiff(m_executor, repoPath, std::move(args), filePath,
                                   "git diff", "git diff", true, false);
    }

    std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getAllDiffs(const std::filesystem::path& repoPath,
                                        const gf::git::bo::DiffScope scope)
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
                if (auto diff = getFileDiff(repoPath, item.path, gf::git::bo::DiffScope::Untracked))
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

        auto execResult = runGitRaw(
            m_executor, repoPath, std::move(args), "git diff", "git diff",
            gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!execResult) return std::unexpected(execResult.error());

        if (execResult->exitCode > 1)
        {
            return std::unexpected(defaultGitFailure(*execResult, "git diff"));
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
        auto diffTreeRes = runGitChecked(
            m_executor, repoPath,
            {"show", "-m", "--first-parent", "--name-status", "--format=", commitSha},
            "git show", "git show", defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!diffTreeRes)
        {
            return std::unexpected(diffTreeRes.error());
        }

        // 2. Run git show -m --first-parent --numstat --format="" <commitSha>
        auto numstatRes = runGitRaw(
            m_executor, repoPath,
            {"show", "-m", "--first-parent", "--numstat", "--format=", commitSha},
            "git show", "git show", gf::git::bo::GitLockPolicy::NoOptionalLocks);

        std::unordered_map<std::string, NumStatEntry> numstatMap;
        if (numstatRes && numstatRes->exitCode == 0)
        {
            std::istringstream nss(numstatRes->stdoutBytes);
            std::string nline;
            while (std::getline(nss, nline))
            {
                while (!nline.empty() && (nline.back() == '\r' || nline.back() == ' ')) nline.pop_back();
                if (nline.empty()) continue;
                std::string pathStr;
                NumStatEntry entry;
                if (parseNumstatLine(nline, pathStr, entry))
                {
                    numstatMap[pathStr] = entry;
                }
            }
        }

        // Parse name-status output
        std::vector<gf::git::bo::CommitFileChange> changes;
        std::istringstream dss(diffTreeRes->stdoutBytes);
        std::string dline;
        while (std::getline(dss, dline))
        {
            while (!dline.empty() && (dline.back() == '\r' || dline.back() == ' ')) dline.pop_back();
            if (dline.empty()) continue;

            gf::git::bo::CommitFileChange change;
            if (!parseNameStatusLine(dline, change)) continue;
            if (const auto it = numstatMap.find(change.path); it != numstatMap.end())
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
                                                         const int contextLines)
    {
        if (commitSha.empty() || filePath.empty())
        {
            return gf::git::bo::FileDiff{.oldPath = filePath, .newPath = filePath};
        }
        const int effectiveContext = contextLines < 0 ? 999999 : contextLines;
        return parseSingleFileDiff(
            m_executor, repoPath,
            {
                "show", "-m", "--first-parent", "--format=", "--no-ext-diff", "--no-textconv", "--no-color",
                "-U" + std::to_string(effectiveContext), "-p", commitSha, "--", filePath
            },
            filePath, "git show", "git show");
    }

    std::expected<gf::git::bo::MultiCommitChangesSummary, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getMultiCommitChanges(const std::filesystem::path& repoPath,
                                                  const std::vector<std::string>& commitShas,
                                                  const bool isContiguousRange,
                                                  const std::string& baseRevision,
                                                  const std::string& headSha)
    {
        gf::git::bo::MultiCommitChangesSummary summary;
        if (commitShas.empty())
        {
            return summary;
        }

        summary.isContiguousRange = isContiguousRange && !baseRevision.empty() && !headSha.empty();
        if (summary.isContiguousRange)
        {
            summary.rangeBaseRevision = baseRevision;
            summary.rangeHeadSha = headSha;
        }

        constexpr std::string_view kCommitMarker = "__GF_COMMIT__";

        // 1. Per-commit name-status + metadata in a single git invocation
        std::vector<std::string> nameStatusArgs = {
            "show", "--no-walk=unsorted", "-m", "--first-parent", "--name-status", "-M",
            "--format=__GF_COMMIT__%H%x1f%h%x1f%an%x1f%aI%x1f%s"
        };
        nameStatusArgs.insert(nameStatusArgs.end(), commitShas.begin(), commitShas.end());

        auto nameStatusRes = m_executor.execute(gf::git::bo::GitCommand{
            .args = std::move(nameStatusArgs),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{30'000}
        });
        if (!nameStatusRes || nameStatusRes->exitCode != 0)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = nameStatusRes ? nameStatusRes->exitCode : -1,
                .message = nameStatusRes ? nameStatusRes->stderrBytes : "Execution failed",
                .command = "git show --no-walk --name-status"
            });
        }

        // 2. Per-commit numstat in a single git invocation
        std::vector<std::string> numstatArgs = {
            "show", "--no-walk=unsorted", "-m", "--first-parent", "--numstat", "-M",
            "--format=__GF_COMMIT__%H"
        };
        numstatArgs.insert(numstatArgs.end(), commitShas.begin(), commitShas.end());

        auto numstatRes = m_executor.execute(gf::git::bo::GitCommand{
            .args = std::move(numstatArgs),
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
            .timeout = std::chrono::milliseconds{30'000}
        });

        std::unordered_map<std::string, std::unordered_map<std::string, NumStatEntry>> perCommitNumstat;
        if (numstatRes && numstatRes->exitCode == 0)
        {
            std::istringstream nss(numstatRes->stdoutBytes);
            std::string line;
            std::string currentSha;
            while (std::getline(nss, line))
            {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                if (line.empty()) continue;
                if (line.starts_with(kCommitMarker))
                {
                    currentSha = line.substr(kCommitMarker.size());
                    continue;
                }
                if (currentSha.empty()) continue;
                std::string rawPath;
                NumStatEntry entry;
                if (parseNumstatLine(line, rawPath, entry))
                {
                    auto& map = perCommitNumstat[currentSha];
                    map[rawPath] = entry;
                    const std::string norm = normalizeNumstatPath(rawPath);
                    if (norm != rawPath) map[norm] = entry;
                }
            }
        }

        // Parse per-commit groups from name-status output
        {
            std::istringstream dss(nameStatusRes->stdoutBytes);
            std::string line;
            gf::git::bo::CommitChangesGroup* currentGroup = nullptr;
            while (std::getline(dss, line))
            {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                if (line.empty()) continue;
                if (line.starts_with(kCommitMarker))
                {
                    const std::string payload = line.substr(kCommitMarker.size());
                    std::vector<std::string> fields;
                    std::size_t pos = 0;
                    for (int f = 0; f < 4; ++f)
                    {
                        const auto sep = payload.find('\x1f', pos);
                        if (sep == std::string::npos) break;
                        fields.push_back(payload.substr(pos, sep - pos));
                        pos = sep + 1;
                    }
                    fields.push_back(pos <= payload.size() ? payload.substr(pos) : std::string{});

                    gf::git::bo::CommitChangesGroup grp;
                    if (fields.size() >= 1) grp.commitSha = fields[0];
                    if (fields.size() >= 2) grp.shortSha = fields[1];
                    if (fields.size() >= 3) grp.authorName = fields[2];
                    if (fields.size() >= 4) grp.authorDateIso = fields[3];
                    if (fields.size() >= 5) grp.subject = fields[4];
                    summary.perCommitGroups.push_back(std::move(grp));
                    currentGroup = &summary.perCommitGroups.back();
                    continue;
                }
                if (!currentGroup) continue;
                gf::git::bo::CommitFileChange change;
                if (parseNameStatusLine(line, change))
                {
                    if (const auto cit = perCommitNumstat.find(currentGroup->commitSha);
                        cit != perCommitNumstat.end())
                    {
                        if (const auto pit = cit->second.find(change.path); pit != cit->second.end())
                        {
                            change.additions = pit->second.additions;
                            change.deletions = pit->second.deletions;
                            change.isBinary = pit->second.isBinary;
                        }
                    }
                    currentGroup->totalAdditions += change.additions;
                    currentGroup->totalDeletions += change.deletions;
                    currentGroup->files.push_back(std::move(change));
                }
            }
        }

        // Build lookup path -> ordered list of CommitFileAttribution (newest -> oldest)
        std::unordered_map<std::string, std::vector<gf::git::bo::CommitFileAttribution>> attributionsByPath;
        for (const auto& grp : summary.perCommitGroups)
        {
            for (const auto& f : grp.files)
            {
                gf::git::bo::CommitFileAttribution attr{
                    .commitSha = grp.commitSha,
                    .shortSha = grp.shortSha,
                    .subject = grp.subject,
                    .authorName = grp.authorName,
                    .status = f.status,
                    .additions = f.additions,
                    .deletions = f.deletions,
                    .isBinary = f.isBinary
                };
                attributionsByPath[f.path].push_back(attr);
                if (!f.oldPath.empty() && f.oldPath != f.path)
                {
                    attributionsByPath[f.oldPath].push_back(std::move(attr));
                }
            }
        }

        auto collectAttributions = [&](const std::string& path, const std::string& oldPath)
        {
            std::vector<gf::git::bo::CommitFileAttribution> out;
            std::unordered_set<std::string> seenShas;
            auto appendFrom = [&](const std::string& key)
            {
                if (key.empty()) return;
                if (const auto it = attributionsByPath.find(key); it != attributionsByPath.end())
                {
                    for (const auto& a : it->second)
                    {
                        if (seenShas.insert(a.commitSha).second)
                        {
                            out.push_back(a);
                        }
                    }
                }
            };
            appendFrom(path);
            appendFrom(oldPath);
            return out;
        };

        // 3. Build aggregatedFiles
        if (summary.isContiguousRange)
        {
            auto rangeStatusRes = m_executor.execute(gf::git::bo::GitCommand{
                .args = {"diff", "--name-status", "-M", baseRevision, headSha},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{30'000}
            });
            auto rangeNumstatRes = m_executor.execute(gf::git::bo::GitCommand{
                .args = {"diff", "--numstat", "-M", baseRevision, headSha},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{30'000}
            });

            if (rangeStatusRes && rangeStatusRes->exitCode == 0)
            {
                std::unordered_map<std::string, NumStatEntry> rangeNumstat;
                if (rangeNumstatRes && rangeNumstatRes->exitCode == 0)
                {
                    std::istringstream nss(rangeNumstatRes->stdoutBytes);
                    std::string line;
                    while (std::getline(nss, line))
                    {
                        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                        if (line.empty()) continue;
                        std::string rawPath;
                        NumStatEntry entry;
                        if (parseNumstatLine(line, rawPath, entry))
                        {
                            rangeNumstat[rawPath] = entry;
                            const std::string norm = normalizeNumstatPath(rawPath);
                            if (norm != rawPath) rangeNumstat[norm] = entry;
                        }
                    }
                }

                std::istringstream dss(rangeStatusRes->stdoutBytes);
                std::string line;
                while (std::getline(dss, line))
                {
                    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                    if (line.empty()) continue;
                    gf::git::bo::CommitFileChange base;
                    if (!parseNameStatusLine(line, base)) continue;

                    gf::git::bo::MultiCommitFileChange item;
                    item.path = std::move(base.path);
                    item.oldPath = std::move(base.oldPath);
                    item.status = base.status;
                    if (const auto it = rangeNumstat.find(item.path); it != rangeNumstat.end())
                    {
                        item.additions = it->second.additions;
                        item.deletions = it->second.deletions;
                        item.isBinary = it->second.isBinary;
                    }
                    item.commits = collectAttributions(item.path, item.oldPath);
                    summary.totalAdditions += item.additions;
                    summary.totalDeletions += item.deletions;
                    summary.aggregatedFiles.push_back(std::move(item));
                }
                return summary;
            }
            summary.isContiguousRange = false;
            summary.rangeBaseRevision.clear();
            summary.rangeHeadSha.clear();
        }

        // Sparse / cross-branch aggregation from oldest to newest
        std::unordered_map<std::string, std::size_t> aggIndex;
        for (auto it = summary.perCommitGroups.rbegin(); it != summary.perCommitGroups.rend(); ++it)
        {
            for (const auto& f : it->files)
            {
                auto found = aggIndex.find(f.path);
                if (found == aggIndex.end())
                {
                    gf::git::bo::MultiCommitFileChange item;
                    item.path = f.path;
                    item.oldPath = f.oldPath;
                    item.status = f.status;
                    item.additions = f.additions;
                    item.deletions = f.deletions;
                    item.isBinary = f.isBinary;
                    item.commits = collectAttributions(item.path, item.oldPath);
                    aggIndex[item.path] = summary.aggregatedFiles.size();
                    summary.aggregatedFiles.push_back(std::move(item));
                }
                else
                {
                    auto& existing = summary.aggregatedFiles[found->second];
                    existing.additions += f.additions;
                    existing.deletions += f.deletions;
                    existing.isBinary = existing.isBinary || f.isBinary;
                    if (f.status == gf::git::bo::FileChangeStatus::Deleted)
                    {
                        existing.status = gf::git::bo::FileChangeStatus::Deleted;
                    }
                    else if (existing.status == gf::git::bo::FileChangeStatus::Deleted &&
                             f.status == gf::git::bo::FileChangeStatus::Added)
                    {
                        existing.status = gf::git::bo::FileChangeStatus::Modified;
                    }
                }
            }
        }

        std::ranges::sort(summary.aggregatedFiles, [](const auto& a, const auto& b)
        {
            return a.path < b.path;
        });
        for (const auto& f : summary.aggregatedFiles)
        {
            summary.totalAdditions += f.additions;
            summary.totalDeletions += f.deletions;
        }

        return summary;
    }

    std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getCommitRangeFileDiffWithContext(const std::filesystem::path& repoPath,
                                                              const std::string& baseRevision,
                                                              const std::string& headSha,
                                                              const std::string& filePath,
                                                              const int contextLines)
    {
        if (baseRevision.empty() || headSha.empty() || filePath.empty())
        {
            return gf::git::bo::FileDiff{.oldPath = filePath, .newPath = filePath};
        }
        const int effectiveContext = contextLines < 0 ? 999999 : contextLines;
        return parseSingleFileDiff(
            m_executor, repoPath,
            {
                "diff", "-M", "--no-ext-diff", "--no-textconv", "--no-color",
                "-U" + std::to_string(effectiveContext), baseRevision, headSha, "--", filePath
            },
            filePath, "git diff", "git diff");
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
        args.emplace_back("--");
        args.push_back(filePath);

        auto execResult = runGitChecked(
            m_executor, repoPath, std::move(args), "git blame", "git blame", defaultGitFailure,
            gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!execResult)
        {
            return std::unexpected(execResult.error());
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
            while (!line.empty() && line.back() == '\r')
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
                    const long long epoch = std::stoll(line.substr(12));
                    const std::time_t t = epoch;
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
                if (const auto firstSpace = line.find(' '); firstSpace == 40)
                {
                    // 40-character sha
                    currentSha = line.substr(0, 40);
                    if (const auto secondSpace = line.find(' ', firstSpace + 1);
                        secondSpace != std::string::npos)
                    {
                        auto thirdSpace = line.find(' ', secondSpace + 1);
                        std::string fLineStr = thirdSpace != std::string::npos
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
        const gf::git::bo::GitCommand cmd{
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
            std::string_view line = endPos == std::string_view::npos
                                        ? output.substr(startPos)
                                        : output.substr(startPos, endPos - startPos);
            startPos = endPos == std::string_view::npos ? output.size() : endPos + 1;

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
            b.isCurrent = headMark.find('*') != std::string_view::npos;
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
                b.shortName = slash != std::string::npos ? b.name.substr(slash + 1) : b.name;
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
                if (const auto aheadPos = track.find("ahead "); aheadPos != std::string_view::npos)
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

                if (const auto behindPos = track.find("behind "); behindPos != std::string_view::npos)
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
            const gf::git::bo::GitCommand branchCheckCmd{
                .args = {"rev-parse", "--abbrev-ref", "HEAD"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{10'000}
            };
            if (const auto bcRes = m_executor.execute(branchCheckCmd); bcRes && bcRes->exitCode == 0)
            {
                if (std::string currentHead = trimString(bcRes->stdoutBytes);
                    !currentHead.empty() && currentHead != "HEAD")
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
        std::vector<std::string> args = options.checkoutAfterCreate
                                            ? std::vector<std::string>{"checkout", "-b", options.name}
                                            : std::vector<std::string>{"branch", options.name};
        if (options.startPoint.has_value() && !options.startPoint->empty())
        {
            args.push_back(*options.startPoint);
        }

        auto res = runGitChecked(
            m_executor, repoPath, std::move(args), "creare il branch", "git branch",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                if (exec.stderrBytes.find("already exists") != std::string::npos ||
                    exec.stderrBytes.find("esiste già") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::BranchAlreadyExists;
                }
                else if (exec.stderrBytes.find("not found") != std::string::npos ||
                    exec.stderrBytes.find("non trovato") != std::string::npos ||
                    exec.stderrBytes.find("No branch named") != std::string::npos ||
                    exec.stderrBytes.find("Nessun branch denominato") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
                }
                return gf::git::ports::GitOpsError{kind, exec.exitCode, exec.stderrBytes, label};
            });
        if (!res) return std::unexpected(res.error());
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
                        checkoutTarget.compare(0, remote.size() + 1, remote + "/") == 0)
                    {
                        if (std::string candidate = checkoutTarget.substr(remote.size() + 1);
                            localName.empty() || candidate.size() < localName.size())
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

        auto res = runGitChecked(
            m_executor, repoPath, std::move(args), "il checkout", "git checkout",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                if (exec.stderrBytes.find("would be overwritten by checkout") != std::string::npos ||
                    exec.stderrBytes.find("sarebbero sovrascritte con il checkout") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::DirtyWorkingTree;
                }
                else if (exec.stderrBytes.find("did not match any") != std::string::npos ||
                    exec.stderrBytes.find("non corrisponde ad alcun") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
                }
                else if (exec.stderrBytes.find("already exists") != std::string::npos ||
                    exec.stderrBytes.find("esiste già") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::BranchAlreadyExists;
                }
                return gf::git::ports::GitOpsError{kind, exec.exitCode, exec.stderrBytes, label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::deleteBranch(const std::filesystem::path& repoPath,
                                         const std::string& branchName,
                                         const bool force)
    {
        auto res = runGitChecked(
            m_executor, repoPath, {"branch", force ? "-D" : "-d", branchName},
            "eliminare il branch", "git branch -d",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                if (exec.stderrBytes.find("cannot delete") != std::string::npos ||
                    exec.stderrBytes.find("impossibile eliminare") != std::string::npos ||
                    exec.stderrBytes.find("not fully merged") != std::string::npos ||
                    exec.stderrBytes.find("sottoposto a merge") != std::string::npos ||
                    exec.stderrBytes.find("used by worktree") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::InvalidOperation;
                }
                else if (exec.stderrBytes.find("not found") != std::string::npos ||
                    exec.stderrBytes.find("non trovato") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
                }
                return gf::git::ports::GitOpsError{kind, exec.exitCode, exec.stderrBytes, label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::renameBranch(const std::filesystem::path& repoPath,
                                         const std::string& oldName,
                                         const std::string& newName)
    {
        auto res = runGitChecked(
            m_executor, repoPath, {"branch", "-m", oldName, newName},
            "rinominare il branch", "git branch -m",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                if (exec.stderrBytes.find("already exists") != std::string::npos ||
                    exec.stderrBytes.find("esiste già") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::BranchAlreadyExists;
                }
                else if (exec.stderrBytes.find("not found") != std::string::npos ||
                    exec.stderrBytes.find("non trovato") != std::string::npos ||
                    exec.stderrBytes.find("No branch named") != std::string::npos ||
                    exec.stderrBytes.find("Nessun branch denominato") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
                }
                return gf::git::ports::GitOpsError{kind, exec.exitCode, exec.stderrBytes, label};
            });
        if (!res) return std::unexpected(res.error());
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

        std::unordered_map<std::string, std::string> stashSelectorsBySha;
        const bool includeAllStashes = options.revisionRange.has_value() &&
            *options.revisionRange == "--all" &&
            (!options.pathFilter.has_value() || options.pathFilter->empty());
        if (includeAllStashes)
        {
            if (auto stashesRes = listStashes(repoPath))
            {
                for (const auto& s : *stashesRes)
                {
                    if (!s.commitSha.empty())
                    {
                        stashSelectorsBySha.try_emplace(s.commitSha, s.selector);
                        if (s.index > 0)
                        {
                            args.push_back(s.commitSha);
                        }
                    }
                }
            }
        }

        if (options.pathFilter.has_value() && !options.pathFilter->empty())
        {
            args.emplace_back("--");
            args.push_back(*options.pathFilter);
        }

        const gf::git::bo::GitCommand cmd{
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

        auto commits = gf::git::bl::CommitLogParser::parse(execResult->stdoutBytes);
        if (!stashSelectorsBySha.empty())
        {
            for (auto& c : commits)
            {
                auto it = stashSelectorsBySha.find(c.sha);
                if (it == stashSelectorsBySha.end())
                {
                    continue;
                }
                bool replaced = false;
                for (auto& dec : c.refDecorations)
                {
                    if (dec == "refs/stash" || dec == "stash")
                    {
                        dec = it->second;
                        replaced = true;
                        break;
                    }
                }
                if (!replaced)
                {
                    c.refDecorations.insert(c.refDecorations.begin(), it->second);
                }
            }
        }
        return commits;
    }

    std::expected<std::vector<std::string>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::listTrackedFiles(const std::filesystem::path& repoPath)
    {
        auto execResult = runGitChecked(
            m_executor, repoPath, {"ls-files"}, "recuperare i file del repository", "git ls-files",
            defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks, 15'000);
        if (!execResult) return std::unexpected(execResult.error());

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
        auto execResult = runGitChecked(
            m_executor, repoPath,
            {
                "tag",
                "-l",
                "--format=%(refname:strip=2)%09%(objectname)%09%(*objectname)%09%(taggername)%09%(taggeremail)%09%(taggerdate:iso-strict)%09%(contents:subject)"
            },
            "recuperare i tag", "git tag -l",
            defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!execResult) return std::unexpected(execResult.error());

        std::vector<gf::git::bo::GitTag> tags;
        std::string_view out(execResult->stdoutBytes);

        while (!out.empty())
        {
            auto nextLine = out.find('\n');
            std::string_view line = nextLine == std::string_view::npos ? out : out.substr(0, nextLine);
            out = nextLine == std::string_view::npos ? std::string_view{} : out.substr(nextLine + 1);

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
        if (options.force) args.emplace_back("-f");
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

        auto res = runGitChecked(
            m_executor, repoPath, std::move(args), "creare il tag", "git tag",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                if (exec.stderrBytes.find("already exists") != std::string::npos ||
                    exec.stderrBytes.find("esiste già") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::TagAlreadyExists;
                }
                else if (exec.stderrBytes.find("not a valid object name") != std::string::npos ||
                    exec.stderrBytes.find("non è un nome di oggetto valido") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::BranchNotFound;
                }
                return gf::git::ports::GitOpsError{kind, exec.exitCode, exec.stderrBytes, label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::deleteTag(const std::filesystem::path& repoPath,
                                      const std::string& tagName)
    {
        auto res = runGitChecked(
            m_executor, repoPath, {"tag", "-d", tagName}, "eliminare il tag", "git tag -d",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                if (exec.stderrBytes.find("not found") != std::string::npos ||
                    exec.stderrBytes.find("non trovato") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::TagNotFound;
                }
                return gf::git::ports::GitOpsError{kind, exec.exitCode, exec.stderrBytes, label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<std::vector<gf::git::bo::GitRemote>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::listRemotes(const std::filesystem::path& repoPath)
    {
        auto execResult = runGitChecked(
            m_executor, repoPath, {"remote", "-v"}, "recuperare i remote", "git remote -v",
            defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!execResult) return std::unexpected(execResult.error());

        std::unordered_map<std::string, gf::git::bo::GitRemote> remotesMap;
        std::string_view out(execResult->stdoutBytes);

        while (!out.empty())
        {
            auto nextLine = out.find('\n');
            std::string_view line = nextLine == std::string_view::npos ? out : out.substr(0, nextLine);
            out = nextLine == std::string_view::npos ? std::string_view{} : out.substr(nextLine + 1);

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

            auto& [remoteName, fetchUrl, pushUrl] = remotesMap[name];
            remoteName = name;
            if (isPush)
            {
                pushUrl = url;
                if (fetchUrl.empty())
                {
                    fetchUrl = url;
                }
            }
            else
            {
                fetchUrl = url;
                if (pushUrl.empty())
                {
                    pushUrl = url;
                }
            }
        }

        std::vector<gf::git::bo::GitRemote> remotes;
        remotes.reserve(remotesMap.size());
        for (auto& r : remotesMap | std::views::values)
        {
            remotes.push_back(std::move(r));
        }
        std::ranges::sort(remotes, [](const auto& a, const auto& b)
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
        auto res = runGitChecked(
            m_executor, repoPath, {"remote", "add", name, url}, "aggiungere il remote", "git remote add",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                if (exec.stderrBytes.find("already exists") != std::string::npos ||
                    exec.stderrBytes.find("esiste già") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::RemoteAlreadyExists;
                }
                return gf::git::ports::GitOpsError{kind, exec.exitCode, exec.stderrBytes, label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::removeRemote(const std::filesystem::path& repoPath,
                                         const std::string& name)
    {
        auto res = runGitChecked(
            m_executor, repoPath, {"remote", "remove", name}, "rimuovere il remote", "git remote remove",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                if (exec.stderrBytes.find("No such remote") != std::string::npos ||
                    exec.stderrBytes.find("non trovato") != std::string::npos ||
                    exec.stderrBytes.find("non esiste") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::RemoteNotFound;
                }
                return gf::git::ports::GitOpsError{kind, exec.exitCode, exec.stderrBytes, label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::setRemoteUrl(const std::filesystem::path& repoPath,
                                         const std::string& name,
                                         const std::string& url)
    {
        return runGitVoid(m_executor, repoPath, {"remote", "set-url", name, url},
                          "aggiornare l'URL del remote", "git remote set-url");
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::fetchRemote(const std::filesystem::path& repoPath,
                                        const gf::git::bo::RemoteOperationOptions& options)
    {
        std::vector<std::string> args{"fetch"};
        if (options.prune) args.emplace_back("--prune");
        if (options.tags) args.emplace_back("--tags");
        if (options.force) args.emplace_back("--force");
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

        auto exec = runGitChecked(
            m_executor, repoPath, std::move(args), "il fetch", "git fetch",
            classifyRemoteFailure,
            gf::git::bo::GitLockPolicy::Standard, 60'000, options.httpsCredential);
        if (!exec) return std::unexpected(exec.error());

        gf::git::bo::RemoteOperationResult result;
        result.success = true;
        result.output = exec->stderrBytes.empty() ? exec->stdoutBytes : exec->stderrBytes;
        return result;
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::pullRemote(const std::filesystem::path& repoPath,
                                       const gf::git::bo::RemoteOperationOptions& options)
    {
        std::vector<std::string> args{"pull"};
        if (options.rebase) args.emplace_back("--rebase");
        if (options.fastForwardOnly) args.emplace_back("--ff-only");
        args.push_back(options.remoteName);
        if (options.branchName.has_value() && !options.branchName->empty())
        {
            args.push_back(*options.branchName);
        }

        auto exec = runGitChecked(
            m_executor, repoPath, std::move(args), "il pull", "git pull",
            [](const gf::git::bo::GitProcessResult& ex, const std::string& label)
            {
                if (ex.stderrBytes.find("Automatic merge failed") != std::string::npos ||
                    ex.stdoutBytes.find("Automatic merge failed") != std::string::npos ||
                    ex.stdoutBytes.find("CONFLICT") != std::string::npos ||
                    ex.stderrBytes.find("conflitti") != std::string::npos)
                {
                    return gf::git::ports::GitOpsError{
                        gf::git::ports::GitOpsError::Kind::MergeConflict,
                        ex.exitCode,
                        pickErrorMessage(ex),
                        label
                    };
                }
                return classifyRemoteFailure(ex, label);
            },
            gf::git::bo::GitLockPolicy::Standard, 60'000, options.httpsCredential);
        if (!exec) return std::unexpected(exec.error());

        gf::git::bo::RemoteOperationResult result;
        result.success = true;
        result.output = exec->stdoutBytes;
        return result;
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::pushRemote(const std::filesystem::path& repoPath,
                                       const gf::git::bo::RemoteOperationOptions& options)
    {
        std::vector<std::string> args{"push"};
        if (options.setUpstream) args.emplace_back("-u");
        if (options.forceUnconditional) args.emplace_back("--force");
        else if (options.force) args.emplace_back("--force-with-lease");
        if (options.tags) args.emplace_back("--tags");
        args.push_back(options.remoteName);
        if (options.branchName.has_value() && !options.branchName->empty())
        {
            args.push_back(*options.branchName);
        }

        auto exec = runGitChecked(
            m_executor, repoPath, std::move(args), "il push", "git push",
            classifyRemoteFailure,
            gf::git::bo::GitLockPolicy::Standard, 60'000, options.httpsCredential);
        if (!exec) return std::unexpected(exec.error());

        gf::git::bo::RemoteOperationResult result;
        result.success = true;
        result.output = exec->stderrBytes.empty() ? exec->stdoutBytes : exec->stderrBytes;
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
        else if (!options.squash)
        {
            args.emplace_back("--no-edit");
        }
        args.push_back(options.sourceBranchOrCommit);

        const gf::git::bo::GitCommand cmd{
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
            result.conflictedFiles = readConflictedFiles(m_executor, repoPath);
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
        auto res = runGitChecked(
            m_executor, repoPath, {"merge", "--abort"}, "annullare il merge", "git merge --abort",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                auto kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                if (exec.stderrBytes.find("no merge to abort") != std::string::npos ||
                    exec.stderrBytes.find("nessun merge da annullare") != std::string::npos ||
                    exec.stderrBytes.find("MERGE_HEAD missing") != std::string::npos)
                {
                    kind = gf::git::ports::GitOpsError::Kind::InvalidOperation;
                }
                return gf::git::ports::GitOpsError{kind, exec.exitCode, exec.stderrBytes, label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<std::vector<gf::git::bo::GitStashItem>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::listStashes(const std::filesystem::path& repoPath)
    {
        auto execResult = runGitChecked(
            m_executor, repoPath,
            {"stash", "list", "--format=%gd%x00%H%x00%aI%x00%gs%x1e"},
            "recuperare l'elenco degli stash", "git stash list",
            defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!execResult) return std::unexpected(execResult.error());

        std::vector<gf::git::bo::GitStashItem> stashes;
        std::string_view raw = execResult->stdoutBytes;

        std::size_t startPos = 0;
        while (startPos < raw.size())
        {
            auto endPos = raw.find('\x1e', startPos);
            std::string_view record = endPos == std::string_view::npos
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
                    item.selector = trimString(std::string(fields[0]));
                    item.commitSha = trimString(std::string(fields[1]));
                    item.dateIso = trimString(std::string(fields[2]));

                    auto openBrace = item.selector.find('{');
                    if (const auto closeBrace = item.selector.find('}', openBrace);
                        openBrace != std::string::npos && closeBrace != std::string::npos &&
                        closeBrace > openBrace + 1)
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
                        if (const auto colonPos = rest.find(':'); colonPos != std::string_view::npos)
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
                        if (const auto colonPos = rest.find(':'); colonPos != std::string_view::npos)
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

        auto exec = runGitRaw(m_executor, repoPath, std::move(args), "salvare lo stash", "git stash push");
        if (!exec) return std::unexpected(exec.error());

        if (const std::string combinedOutput = exec->stdoutBytes + "\n" + exec->stderrBytes;
            combinedOutput.find("No local changes to save") != std::string::npos ||
            combinedOutput.find("Nessuna modifica locale da salvare") != std::string::npos)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::NothingToStash,
                .exitCode = exec->exitCode,
                .message = "Nessuna modifica locale da salvare nello stash",
                .command = "git stash push"
            });
        }
        if (exec->exitCode != 0)
        {
            return std::unexpected(defaultGitFailure(*exec, "git stash push"));
        }
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::stashPop(const std::filesystem::path& repoPath,
                                     const gf::git::bo::StashApplyOptions& options)
    {
        return applyOrPopStash(m_executor, repoPath, "pop", options);
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::stashApply(const std::filesystem::path& repoPath,
                                       const gf::git::bo::StashApplyOptions& options)
    {
        return applyOrPopStash(m_executor, repoPath, "apply", options);
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::stashDrop(const std::filesystem::path& repoPath,
                                      const std::size_t stashIndex)
    {
        auto res = runGitChecked(
            m_executor, repoPath, {"stash", "drop", "stash@{" + std::to_string(stashIndex) + "}"},
            "eliminare lo stash", "git stash drop",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                const std::string combinedOutput = exec.stdoutBytes + "\n" + exec.stderrBytes;
                const auto kind = isStashNotFoundError(combinedOutput)
                                      ? gf::git::ports::GitOpsError::Kind::StashNotFound
                                      : gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                return gf::git::ports::GitOpsError{kind, exec.exitCode, pickErrorMessage(exec), label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::stashClear(const std::filesystem::path& repoPath)
    {
        return runGitVoid(m_executor, repoPath, {"stash", "clear"}, "svuotare gli stash", "git stash clear");
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
        if (options.autostash) args.emplace_back("--autostash");
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
        return executeRebaseStep(
            m_executor, repoPath, std::move(args), "avviare il rebase", "git rebase",
            false, true, gf::git::bo::RebaseStatus::UpToDate, [] { return false; });
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::continueRebase(const std::filesystem::path& repoPath)
    {
        return executeRebaseStep(
            m_executor, repoPath, {"-c", "core.editor=true", "rebase", "--continue"},
            "continuare il rebase", "git rebase --continue",
            true, false, gf::git::bo::RebaseStatus::Applied,
            [&] { const auto p = isRebaseInProgress(repoPath); return p && *p; });
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::skipRebase(const std::filesystem::path& repoPath)
    {
        return executeRebaseStep(
            m_executor, repoPath, {"-c", "core.editor=true", "rebase", "--skip"},
            "saltare il commit nel rebase", "git rebase --skip",
            true, false, gf::git::bo::RebaseStatus::Skipped,
            [&] { const auto p = isRebaseInProgress(repoPath); return p && *p; });
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::abortRebase(const std::filesystem::path& repoPath)
    {
        auto res = runGitChecked(
            m_executor, repoPath, {"rebase", "--abort"}, "annullare il rebase", "git rebase --abort",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                const std::string combined = exec.stdoutBytes + "\n" + exec.stderrBytes;
                const auto kind = (combined.find("no rebase in progress") != std::string::npos ||
                                   combined.find("nessun rebase in corso") != std::string::npos)
                                      ? gf::git::ports::GitOpsError::Kind::NoRebaseInProgress
                                      : gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                return gf::git::ports::GitOpsError{kind, exec.exitCode, pickErrorMessage(exec), label};
            });
        if (!res) return std::unexpected(res.error());
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

        const gf::git::bo::GitCommand cmd{
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
            result.conflictedFiles = readConflictedFiles(m_executor, repoPath);
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
        auto res = runGitChecked(
            m_executor, repoPath, {"cherry-pick", "--abort"}, "annullare il cherry-pick", "git cherry-pick --abort",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                const std::string combined = exec.stdoutBytes + "\n" + exec.stderrBytes;
                const auto kind = (combined.find("no cherry-pick or revert in progress") != std::string::npos ||
                                   combined.find("nessun cherry-pick") != std::string::npos)
                                      ? gf::git::ports::GitOpsError::Kind::InvalidOperation
                                      : gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                return gf::git::ports::GitOpsError{kind, exec.exitCode, pickErrorMessage(exec), label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<std::string, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getFileStageContent(const std::filesystem::path& repoPath,
                                                const std::string& filePath,
                                                const int stage)
    {
        const gf::git::bo::GitCommand cmd{
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
            if (const std::string& err = execResult->stderrBytes;
                err.find("does not exist") != std::string::npos ||
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
        if (std::error_code ec; !std::filesystem::exists(fullPath, ec))
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
            if (const auto stage1 = getFileStageContent(repoPath, filePath, 1); stage1 && !stage1->empty())
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
            return runGitVoid(m_executor, repoPath, {"add", "--", options.filePath},
                              "eseguire stage del file risolto", "git add",
                              gf::git::bo::GitLockPolicy::Standard, 15'000);
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
        return runGitVoid(m_executor, repoPath, {"reset", flag, commitSha}, "eseguire il reset", "git reset");
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::revertCommit(const std::filesystem::path& repoPath,
                                         const std::string& commitSha)
    {
        return runGitVoid(m_executor, repoPath, {"-c", "core.editor=true", "revert", "--no-edit", commitSha},
                          "eseguire revert", "git revert");
    }

    std::expected<std::string, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::rewordCommit(const std::filesystem::path& repoPath,
                                         const std::string& commitSha,
                                         const std::string& newMessage)
    {
        using gf::git::ports::GitOpsError;

        auto makeError = [](const GitOpsError::Kind kind, const int exitCode, std::string message, std::string command)
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
            const gf::git::bo::GitCommand cmd{
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
                if (auto line = trimString(out.substr(start, end - start)); !line.empty())
                {
                    descendants.push_back(std::move(line));
                }
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
        std::filesystem::path gitDir = core::utf8ToPath(trimString(*gitDirRes));
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
            const std::string headerBlock = sep == std::string::npos ? raw : raw.substr(0, sep + 1);
            const std::string oldMessage = sep == std::string::npos ? std::string{} : raw.substr(sep + 2);

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
                const std::string value = spacePos == std::string::npos ? std::string{} : line.substr(spacePos + 1);

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
            rebuilt += '\n';
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

            auto hashRes = runGit({"hash-object", "-t", "commit", "-w", core::pathToUtf8(scratchFile)});
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
        if (const auto headRef = runGitCapture(m_executor, repoPath, {"symbolic-ref", "-q", "HEAD"});
            headRef && *headRef == plan->targetRef)
        {
            if (auto merged = runGitCapture(m_executor, repoPath, {"merge", "--ff-only", plan->sourceSha});
                !merged)
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
        if (!commitOrBranch.empty()) args.push_back(commitOrBranch);
        return runGitVoid(m_executor, repoPath, std::move(args), "creare il worktree", "git worktree");
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

        const auto workingDir = !parentDir.empty() && std::filesystem::exists(parentDir, ec)
                                    ? parentDir
                                    : std::filesystem::temp_directory_path();

        std::string lineBuffer;
        auto parseProgressChunk = [&lineBuffer, &options](const std::string_view chunk)
        {
            if (!options.progressCallback) return;
            lineBuffer.append(chunk);

            size_t pos = 0;
            while (pos < lineBuffer.size())
            {
                const size_t delim = lineBuffer.find_first_of("\r\n", pos);
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
                    if (const auto pctPos = line.find('%'); pctPos != std::string_view::npos && pctPos > 0)
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
        if (options.httpsToken && !options.httpsToken->empty() && gf::git::bl::isHttpRemoteUrl(options.remoteUrl))
        {
            gf::git::bl::applyHttpsCredential(cmd, {
                                                  .username = options.httpsUsername.value_or("x-access-token"),
                                                  .password = *options.httpsToken,
                                                  .host = core::git::bl::extractRemoteHost(options.remoteUrl)
                                              });
        }

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
            return std::unexpected(classifyRemoteFailure(*execResult, "git clone"));
        }

        // Detect initial branch of cloned repository
        std::string branchName = "main";
        if (auto branchRes = runGitCapture(m_executor, options.destinationPath, {"branch", "--show-current"});
            branchRes && !branchRes->empty())
        {
            branchName = std::move(*branchRes);
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
        auto blob = getFileBlob(repoPath, revision, filePath, kMaxContentBytes);
        if (!blob)
        {
            return std::unexpected(blob.error());
        }

        const std::string commandLabel =
            (revision.empty() || revision == "WORKTREE") ? "read file" : "git show";
        if (blob->truncated || blob->bytes.size() > kMaxContentBytes)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "File troppo grande (> 2 MiB) per l'anteprima testuale",
                .command = commandLabel
            });
        }
        if (blob->bytes.find('\0') != std::string::npos)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il file contiene dati binari",
                .command = commandLabel
            });
        }

        core::stripUtf8Bom(blob->bytes);
        return std::move(blob->bytes);
    }

    std::expected<gf::git::bo::FileBlob, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getFileBlob(const std::filesystem::path& repoPath,
                                        const std::string& revision,
                                        const std::string& filePath,
                                        const std::size_t maxBytes)
    {
        if (filePath.empty())
        {
            return gf::git::bo::FileBlob{};
        }

        if (revision.empty() || revision == "WORKTREE")
        {
            const auto fullPath = repoPath / core::utf8ToPath(filePath);
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

            const std::uint64_t fileSize = std::filesystem::file_size(fullPath, ec);
            if (ec)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::StorageError,
                    .exitCode = -1,
                    .message = "Impossibile determinare la dimensione del file: " + filePath,
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

            gf::git::bo::FileBlob blob;
            blob.totalSize = fileSize;
            const std::size_t toRead = static_cast<std::size_t>(std::min<std::uint64_t>(fileSize, maxBytes));
            blob.truncated = fileSize > maxBytes;
            if (toRead > 0)
            {
                blob.bytes.resize(toRead);
                ifs.read(blob.bytes.data(), static_cast<std::streamsize>(toRead));
                const auto actuallyRead = static_cast<std::size_t>(ifs.gcount());
                blob.bytes.resize(actuallyRead);
            }
            return blob;
        }

        std::string normPath = filePath;
        std::ranges::replace(normPath, '\\', '/');
        const std::string targetRev = revision == "INDEX" ? std::string(":0") : revision;
        const std::string spec = targetRev + ":" + normPath;

        // First probe exact blob size with `git cat-file -s` so oversized objects never OOM.
        auto sizeRes = runGitChecked(
            m_executor, repoPath, {"cat-file", "-s", spec}, "git cat-file", "git cat-file",
            [](const gf::git::bo::GitProcessResult& res, const std::string& label)
            {
                const auto kind = isGitObjectPathMissing(res.stderrBytes)
                    ? gf::git::ports::GitOpsError::Kind::PathNotFound
                    : gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                return gf::git::ports::GitOpsError{kind, res.exitCode, pickErrorMessage(res), label};
            },
            gf::git::bo::GitLockPolicy::NoOptionalLocks, 10'000);
        if (!sizeRes) return std::unexpected(sizeRes.error());

        std::uint64_t objectSize = 0;
        try
        {
            if (const std::string trimmed = trimString(sizeRes->stdoutBytes); !trimmed.empty())
            {
                objectSize = std::stoull(trimmed);
            }
        }
        catch (...)
        {
            objectSize = 0;
        }

        if (objectSize > maxBytes)
        {
            return gf::git::bo::FileBlob{
                .bytes = {},
                .totalSize = objectSize,
                .truncated = true
            };
        }

        auto execResult = runGitChecked(
            m_executor, repoPath, {"show", spec}, "git show", "git show", defaultGitFailure,
            gf::git::bo::GitLockPolicy::NoOptionalLocks, 20'000);
        if (!execResult) return std::unexpected(execResult.error());

        gf::git::bo::FileBlob blob;
        blob.bytes = std::move(execResult->stdoutBytes);
        blob.totalSize = std::max<std::uint64_t>(objectSize, blob.bytes.size());
        blob.truncated = false;
        return blob;
    }
} // namespace gf::infra::git
