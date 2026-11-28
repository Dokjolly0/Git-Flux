#include "infrastructure/git/native/native_git_ops_internal.hpp"
#include "core/modules/git/bl/remote_access_diagnostics.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gf::infra::git
{
    namespace
    {
        [[nodiscard]] std::optional<gf::git::ports::GitOpsError>
        validateCloneOptions(const gf::git::bo::CloneRepositoryOptions& options)
        {
            if (options.remoteUrl.empty())
            {
                return gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::InvalidRemoteUrl,
                    .exitCode = -1,
                    .message = "L'URL del repository remoto non può essere vuoto",
                    .command = "git clone"
                };
            }
            if (options.destinationPath.empty())
            {
                return gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath,
                    .exitCode = -1,
                    .message = "Il percorso di destinazione non può essere vuoto",
                    .command = "git clone"
                };
            }
            std::error_code ec;
            if (std::filesystem::exists(options.destinationPath, ec) &&
                !std::filesystem::is_empty(options.destinationPath, ec))
            {
                return gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::DestinationAlreadyExists,
                    .exitCode = -1,
                    .message = "La cartella di destinazione esiste già e non è vuota: " +
                               options.destinationPath.string(),
                    .command = "git clone"
                };
            }
            return std::nullopt;
        }

        [[nodiscard]] std::vector<std::string> buildCloneArgs(const gf::git::bo::CloneRepositoryOptions& options)
        {
            std::vector<std::string> args = {"clone", "--progress"};
            if (options.recursiveSubmodules) args.emplace_back("--recurse-submodules");
            if (options.branch.has_value() && !options.branch->empty())
            {
                args.emplace_back("-b");
                args.push_back(*options.branch);
            }
            args.push_back(options.remoteUrl);
            args.push_back(options.destinationPath.string());
            return args;
        }

        [[nodiscard]] int extractCloneProgressPercent(const std::string_view line) noexcept
        {
            const auto pctPos = line.find('%');
            if (pctPos == std::string_view::npos || pctPos == 0) return -1;
            std::size_t startDigits = pctPos;
            while (startDigits > 0 && std::isdigit(static_cast<unsigned char>(line[startDigits - 1])))
            {
                --startDigits;
            }
            if (startDigits >= pctPos) return -1;
            try
            {
                return std::stoi(std::string(line.substr(startDigits, pctPos - startDigits)));
            }
            catch (...)
            {
                return -1;
            }
        }

        void consumeCloneProgressChunk(const std::string_view chunk,
                                       std::string& lineBuffer,
                                       const decltype(gf::git::bo::CloneRepositoryOptions::progressCallback)& progressCallback)
        {
            if (!progressCallback) return;
            lineBuffer.append(chunk);

            std::size_t pos = 0;
            while (pos < lineBuffer.size())
            {
                const std::size_t delim = lineBuffer.find_first_of("\r\n", pos);
                if (delim == std::string::npos) break;

                std::string_view line(lineBuffer.data() + pos, delim - pos);
                while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.remove_prefix(1);
                while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.remove_suffix(1);

                if (!line.empty())
                {
                    progressCallback(extractCloneProgressPercent(line), line);
                }
                pos = delim + 1;
            }

            if (pos > 0) lineBuffer.erase(0, pos);
        }
    } // namespace

    std::expected<std::vector<gf::git::bo::GitRemote>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::listRemotes(const std::filesystem::path& repoPath)
    {
        auto execResult = detail::runGitChecked(
            m_executor, repoPath, {"remote", "-v"}, "recuperare i remote", "git remote -v",
            detail::defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!execResult) return std::unexpected(execResult.error());

        std::unordered_map<std::string, gf::git::bo::GitRemote> remotesMap;
        std::string_view out(execResult->stdoutBytes);

        while (!out.empty())
        {
            auto nextLine = out.find('\n');
            std::string_view line = nextLine == std::string_view::npos ? out : out.substr(0, nextLine);
            out = nextLine == std::string_view::npos ? std::string_view{} : out.substr(nextLine + 1);

            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (line.empty()) continue;

            const auto firstSep = line.find_first_of(" \t");
            if (firstSep == std::string_view::npos) continue;

            const auto name = std::string(line.substr(0, firstSep));
            auto rest = line.substr(firstSep);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) rest.remove_prefix(1);

            const auto lastSep = rest.rfind(" (");
            std::string url;
            bool isPush = false;
            if (lastSep != std::string_view::npos)
            {
                url = std::string(rest.substr(0, lastSep));
                isPush = rest.find("(push)") != std::string_view::npos;
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
                if (fetchUrl.empty()) fetchUrl = url;
            }
            else
            {
                fetchUrl = url;
                if (pushUrl.empty()) pushUrl = url;
            }
        }

        std::vector<gf::git::bo::GitRemote> remotes;
        remotes.reserve(remotesMap.size());
        for (auto& r : remotesMap | std::views::values)
        {
            remotes.push_back(std::move(r));
        }
        std::ranges::sort(remotes, [](const auto& a, const auto& b) { return a.name < b.name; });
        return remotes;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::addRemote(const std::filesystem::path& repoPath,
                                      const std::string& name,
                                      const std::string& url)
    {
        auto res = detail::runGitChecked(
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
        auto res = detail::runGitChecked(
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
        return detail::runGitVoid(m_executor, repoPath, {"remote", "set-url", name, url},
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

        auto exec = detail::runGitChecked(
            m_executor, repoPath, std::move(args), "il fetch", "git fetch",
            detail::classifyRemoteFailure,
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

        auto exec = detail::runGitChecked(
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
                        detail::pickErrorMessage(ex),
                        label
                    };
                }
                return detail::classifyRemoteFailure(ex, label);
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

        auto exec = detail::runGitChecked(
            m_executor, repoPath, std::move(args), "il push", "git push",
            detail::classifyRemoteFailure,
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
        if (options.noFastForward) args.emplace_back("--no-ff");
        if (options.squash) args.emplace_back("--squash");
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

            if (const auto rev = detail::runGitCapture(m_executor, repoPath, {"rev-parse", "HEAD"}))
            {
                result.targetCommitSha = *rev;
            }
            return result;
        }

        if (combinedOutput.find("CONFLICT") != std::string::npos ||
            combinedOutput.find("Automatic merge failed") != std::string::npos ||
            combinedOutput.find("conflitti") != std::string::npos)
        {
            result.status = gf::git::bo::MergeStatus::Conflict;
            result.summaryMessage = execResult->stdoutBytes;
            result.conflictedFiles = detail::readConflictedFiles(m_executor, repoPath);
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
        auto res = detail::runGitChecked(
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

    std::expected<gf::git::bo::CloneRepositoryResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::cloneRepository(const gf::git::bo::CloneRepositoryOptions& options)
    {
        if (const auto err = validateCloneOptions(options))
        {
            return std::unexpected(*err);
        }

        std::error_code ec;
        const auto parentDir = options.destinationPath.parent_path();
        if (!parentDir.empty() && !std::filesystem::exists(parentDir, ec))
        {
            std::filesystem::create_directories(parentDir, ec);
        }
        const auto workingDir = !parentDir.empty() && std::filesystem::exists(parentDir, ec)
            ? parentDir
            : std::filesystem::temp_directory_path();

        std::string lineBuffer;
        gf::git::bo::GitCommand cmd{
            .args = buildCloneArgs(options),
            .workingDirectory = workingDir,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard,
            .timeout = options.timeout,
            .onStderrChunk = [&lineBuffer, cb = options.progressCallback](const std::string_view chunk)
            {
                consumeCloneProgressChunk(chunk, lineBuffer, cb);
            }
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
            return std::unexpected(detail::classifyRemoteFailure(*execResult, "git clone"));
        }

        std::string branchName = "main";
        if (auto branchRes = detail::runGitCapture(m_executor, options.destinationPath, {"branch", "--show-current"});
            branchRes && !branchRes->empty())
        {
            branchName = std::move(*branchRes);
        }

        return gf::git::bo::CloneRepositoryResult{
            .repositoryPath = options.destinationPath,
            .displayName = options.destinationPath.filename().string(),
            .initialBranch = std::move(branchName)
        };
    }
} // namespace gf::infra::git
