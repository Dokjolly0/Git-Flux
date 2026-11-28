#pragma once

#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "core/modules/git/bl/https_credential.hpp"
#include "core/modules/git/bl/unified_diff_parser.hpp"
#include "core/modules/git/bo/git_command.hpp"
#include "core/shared/string_utils.hpp"

#include <algorithm>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gf::infra::git::detail
{
    [[nodiscard]] inline std::string trimString(const std::string_view str)
    {
        return std::string(core::trim(str));
    }

    [[nodiscard]] inline std::string pickErrorMessage(const gf::git::bo::GitProcessResult& res)
    {
        return res.stderrBytes.empty() ? res.stdoutBytes : res.stderrBytes;
    }

    [[nodiscard]] inline std::expected<gf::git::bo::GitProcessResult, gf::git::ports::GitOpsError>
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
    [[nodiscard]] inline std::expected<gf::git::bo::GitProcessResult, gf::git::ports::GitOpsError>
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

    [[nodiscard]] inline gf::git::ports::GitOpsError
    defaultGitFailure(const gf::git::bo::GitProcessResult& exec, const std::string& commandLabel)
    {
        return gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
            .exitCode = exec.exitCode,
            .message = pickErrorMessage(exec),
            .command = commandLabel
        };
    }

    [[nodiscard]] inline gf::git::ports::GitOpsError
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

    [[nodiscard]] inline std::expected<void, gf::git::ports::GitOpsError>
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
    [[nodiscard]] inline std::expected<std::string, gf::git::ports::GitOpsError>
    runGitCapture(gf::git::ports::IGitCommandExecutor& executor,
                  const std::filesystem::path& repoPath,
                  std::vector<std::string> args)
    {
        const std::string label = "git " + (args.empty() ? std::string{} : args.front());
        auto exec = runGitChecked(executor, repoPath, std::move(args), label, label, defaultGitFailure);
        if (!exec) return std::unexpected(exec.error());
        return trimString(exec->stdoutBytes);
    }

    [[nodiscard]] inline std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
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

    [[nodiscard]] inline gf::git::bo::FileChangeStatus parseFileChangeStatusChar(const std::string_view statusStr) noexcept
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

    [[nodiscard]] inline std::string normalizeNumstatPath(const std::string& raw)
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

    [[nodiscard]] inline bool parseNumstatLine(const std::string_view nline,
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

    [[nodiscard]] inline bool parseNameStatusLine(const std::string_view dline,
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

    [[nodiscard]] inline std::vector<std::string>
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
} // namespace gf::infra::git::detail
