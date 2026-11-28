#include "infrastructure/git/native/native_git_ops_internal.hpp"
#include "core/modules/git/bl/conflict_marker_parser.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace gf::infra::git
{
    namespace
    {
        [[nodiscard]] bool hasRebaseConflictOutput(const std::string_view combinedOutput) noexcept
        {
            return combinedOutput.find("CONFLICT") != std::string_view::npos ||
                   combinedOutput.find("Automatic merge failed") != std::string_view::npos ||
                   combinedOutput.find("conflitti") != std::string_view::npos ||
                   combinedOutput.find("Resolve all conflicts") != std::string_view::npos ||
                   combinedOutput.find("could not apply") != std::string_view::npos ||
                   combinedOutput.find("unmerged") != std::string_view::npos;
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

            result.conflictedFiles = detail::readConflictedFiles(executor, repoPath);
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
            auto res = detail::runGitChecked(
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
                    return gf::git::ports::GitOpsError{kind, exec.exitCode, detail::pickErrorMessage(exec), label};
                });
            if (!res) return std::unexpected(res.error());
            return {};
        }

        void parseStashSubject(std::string_view subject, gf::git::bo::GitStashItem& item)
        {
            for (const std::string_view prefix : {std::string_view{"WIP on "}, std::string_view{"On "}})
            {
                if (!subject.starts_with(prefix)) continue;
                const auto rest = subject.substr(prefix.size());
                if (const auto colonPos = rest.find(':'); colonPos != std::string_view::npos)
                {
                    item.branchName = detail::trimString(rest.substr(0, colonPos));
                    item.message = detail::trimString(rest.substr(colonPos + 1));
                }
                else
                {
                    item.branchName = detail::trimString(rest);
                    item.message = std::string(subject);
                }
                return;
            }
            item.message = std::string(subject);
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
            auto exec = detail::runGitRaw(executor, repoPath, std::move(args), errorVerb, commandLabel,
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

            return std::unexpected(detail::defaultGitFailure(*exec, std::string(commandLabel)));
        }
    } // namespace

    std::expected<std::vector<gf::git::bo::GitStashItem>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::listStashes(const std::filesystem::path& repoPath)
    {
        auto execResult = detail::runGitChecked(
            m_executor, repoPath,
            {"stash", "list", "--format=%gd%x00%H%x00%aI%x00%gs%x1e"},
            "recuperare l'elenco degli stash", "git stash list",
            detail::defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!execResult) return std::unexpected(execResult.error());

        std::vector<gf::git::bo::GitStashItem> stashes;
        std::string_view raw = execResult->stdoutBytes;

        std::size_t startPos = 0;
        while (startPos < raw.size())
        {
            const auto endPos = raw.find('\x1e', startPos);
            const std::string_view record = endPos == std::string_view::npos
                ? raw.substr(startPos)
                : raw.substr(startPos, endPos - startPos);

            if (!record.empty())
            {
                std::vector<std::string_view> fields;
                std::size_t fStart = 0;
                while (fStart < record.size())
                {
                    const auto fEnd = record.find('\0', fStart);
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
                    item.selector = detail::trimString(fields[0]);
                    item.commitSha = detail::trimString(fields[1]);
                    item.dateIso = detail::trimString(fields[2]);

                    const auto openBrace = item.selector.find('{');
                    if (const auto closeBrace = item.selector.find('}', openBrace);
                        openBrace != std::string::npos && closeBrace != std::string::npos &&
                        closeBrace > openBrace + 1)
                    {
                        try
                        {
                            item.index = std::stoul(item.selector.substr(openBrace + 1, closeBrace - openBrace - 1));
                        }
                        catch (...)
                        {
                            item.index = 0;
                        }
                    }

                    parseStashSubject(fields[3], item);
                    stashes.push_back(std::move(item));
                }
            }

            if (endPos == std::string_view::npos) break;
            startPos = endPos + 1;
        }

        return stashes;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::stashPush(const std::filesystem::path& repoPath,
                                      const gf::git::bo::StashPushOptions& options)
    {
        std::vector<std::string> args = {"stash", "push"};
        if (options.includeUntracked) args.emplace_back("-u");
        if (options.keepIndex) args.emplace_back("--keep-index");
        if (options.message && !options.message->empty())
        {
            args.emplace_back("-m");
            args.emplace_back(*options.message);
        }
        if (!options.paths.empty())
        {
            args.emplace_back("--");
            for (const auto& path : options.paths) args.emplace_back(path);
        }

        auto exec = detail::runGitRaw(m_executor, repoPath, std::move(args), "salvare lo stash", "git stash push");
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
            return std::unexpected(detail::defaultGitFailure(*exec, "git stash push"));
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
        auto res = detail::runGitChecked(
            m_executor, repoPath, {"stash", "drop", "stash@{" + std::to_string(stashIndex) + "}"},
            "eliminare lo stash", "git stash drop",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                const std::string combinedOutput = exec.stdoutBytes + "\n" + exec.stderrBytes;
                const auto kind = isStashNotFoundError(combinedOutput)
                    ? gf::git::ports::GitOpsError::Kind::StashNotFound
                    : gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                return gf::git::ports::GitOpsError{kind, exec.exitCode, detail::pickErrorMessage(exec), label};
            });
        if (!res) return std::unexpected(res.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::stashClear(const std::filesystem::path& repoPath)
    {
        return detail::runGitVoid(m_executor, repoPath, {"stash", "clear"}, "svuotare gli stash", "git stash clear");
    }

    std::expected<bool, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::isRebaseInProgress(const std::filesystem::path& repoPath)
    {
        std::error_code ec;
        return std::filesystem::exists(repoPath / ".git" / "rebase-merge", ec) ||
               std::filesystem::exists(repoPath / ".git" / "rebase-apply", ec);
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
        auto res = detail::runGitChecked(
            m_executor, repoPath, {"rebase", "--abort"}, "annullare il rebase", "git rebase --abort",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                const std::string combined = exec.stdoutBytes + "\n" + exec.stderrBytes;
                const auto kind = (combined.find("no rebase in progress") != std::string::npos ||
                                   combined.find("nessun rebase in corso") != std::string::npos)
                    ? gf::git::ports::GitOpsError::Kind::NoRebaseInProgress
                    : gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                return gf::git::ports::GitOpsError{kind, exec.exitCode, detail::pickErrorMessage(exec), label};
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

        const auto fullPath = repoPath / filePath;
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
        const auto fullPath = repoPath / options.filePath;
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
            return detail::runGitVoid(m_executor, repoPath, {"add", "--", options.filePath},
                                      "eseguire stage del file risolto", "git add",
                                      gf::git::bo::GitLockPolicy::Standard, 15'000);
        }

        return {};
    }
} // namespace gf::infra::git
