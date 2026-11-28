#include "infrastructure/git/native/native_git_ops_internal.hpp"
#include "core/shared/utf8_path.hpp"

#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gf::infra::git
{
    namespace
    {
        [[nodiscard]] int parseTrackingCount(const std::string_view track, const std::string_view prefix) noexcept
        {
            const auto pos = track.find(prefix);
            if (pos == std::string_view::npos) return 0;
            int val = 0;
            std::size_t p = pos + prefix.size();
            while (p < track.size() && track[p] >= '0' && track[p] <= '9')
            {
                val = val * 10 + (track[p] - '0');
                ++p;
            }
            return val;
        }

        [[nodiscard]] std::optional<gf::git::bo::GitBranch> parseBranchLine(std::string_view line)
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            {
                line.remove_suffix(1);
            }
            if (line.empty()) return std::nullopt;

            std::vector<std::string_view> fields;
            std::size_t fStart = 0;
            while (fStart < line.size())
            {
                const auto fEnd = line.find('\t', fStart);
                if (fEnd == std::string_view::npos)
                {
                    fields.push_back(line.substr(fStart));
                    break;
                }
                fields.push_back(line.substr(fStart, fEnd - fStart));
                fStart = fEnd + 1;
            }
            if (fields.size() < 2) return std::nullopt;

            const std::string_view refname = fields[0];
            const std::string_view objname = fields[1];
            const std::string_view headMark = fields.size() > 2 ? fields[2] : "";
            const std::string_view upstream = fields.size() > 3 ? fields[3] : "";
            const std::string_view track = fields.size() > 4 ? fields[4] : "";

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
                const auto slash = b.name.find('/');
                b.shortName = slash != std::string::npos ? b.name.substr(slash + 1) : b.name;
                if (b.shortName == "HEAD") return std::nullopt;
            }
            else
            {
                b.type = gf::git::bo::BranchType::Local;
                b.name = std::string(refname);
                b.shortName = b.name;
            }

            if (!track.empty())
            {
                b.ahead = parseTrackingCount(track, "ahead ");
                b.behind = parseTrackingCount(track, "behind ");
            }
            return b;
        }

        void appendUnbornHeadIfEmpty(gf::git::ports::IGitCommandExecutor& executor,
                                     const std::filesystem::path& repoPath,
                                     std::vector<gf::git::bo::GitBranch>& branches)
        {
            if (!branches.empty()) return;
            const gf::git::bo::GitCommand branchCheckCmd{
                .args = {"rev-parse", "--abbrev-ref", "HEAD"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{10'000}
            };
            const auto bcRes = executor.execute(branchCheckCmd);
            if (!bcRes || bcRes->exitCode != 0) return;
            if (std::string currentHead = detail::trimString(bcRes->stdoutBytes);
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

        [[nodiscard]] std::expected<std::string, gf::git::ports::GitOpsError>
        normalizeRewordMessage(const std::string_view newMessage)
        {
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
            message = detail::trimString(message);
            if (message.empty())
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::EmptyCommitMessage,
                    .exitCode = -1,
                    .message = "Il messaggio del commit non può essere vuoto",
                    .command = "reword"
                });
            }
            message.push_back('\n');
            return message;
        }

        [[nodiscard]] std::string rebuildCommitObjectBytes(
            const std::string& raw,
            const std::string& committerIdent,
            const std::unordered_map<std::string, std::string>& rewritten,
            const std::string* replacementMessage)
        {
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
                    if (!skipContinuation) rebuilt += line + "\n";
                    continue;
                }
                skipContinuation = false;

                const auto spacePos = line.find(' ');
                const std::string key = line.substr(0, spacePos);
                const std::string value = spacePos == std::string::npos ? std::string{} : line.substr(spacePos + 1);

                if (key == "parent")
                {
                    const auto it = rewritten.find(value);
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
                    // New message is always UTF-8; drop any legacy encoding header.
                }
                else
                {
                    rebuilt += line + "\n";
                }
            }
            rebuilt += '\n';
            rebuilt += replacementMessage ? *replacementMessage : oldMessage;
            return rebuilt;
        }

        [[nodiscard]] std::expected<std::vector<std::string>, gf::git::ports::GitOpsError>
        resolveRewordDescendants(gf::git::ports::IGitCommandExecutor& executor,
                                 const std::filesystem::path& repoPath,
                                 const std::string& targetSha,
                                 const std::string& headSha)
        {
            std::vector<std::string> descendants;
            if (targetSha == headSha) return descendants;

            if (!detail::runGitCapture(executor, repoPath, {"merge-base", "--is-ancestor", targetSha, headSha}))
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                    .exitCode = -1,
                    .message = "Il commit " + targetSha.substr(0, 7) +
                               " non appartiene al branch corrente: fai prima il checkout del branch che lo contiene",
                    .command = "reword"
                });
            }

            auto listRes = detail::runGitChecked(
                executor, repoPath,
                {"rev-list", "--topo-order", "--reverse", "--ancestry-path", targetSha + ".." + headSha},
                "git rev-list", "git rev-list", detail::defaultGitFailure);
            if (!listRes) return std::unexpected(listRes.error());

            std::size_t start = 0;
            const std::string& out = listRes->stdoutBytes;
            while (start < out.size())
            {
                auto end = out.find('\n', start);
                if (end == std::string::npos) end = out.size();
                if (auto line = detail::trimString(std::string_view(out).substr(start, end - start)); !line.empty())
                {
                    descendants.push_back(std::move(line));
                }
                start = end + 1;
            }
            return descendants;
        }

        struct FastForwardPlan
        {
            std::string targetRef;
            std::string targetSha;
            std::string sourceSha;
            bool possible = false;
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

            auto tgt = detail::runGitCapture(
                executor, repoPath, {"rev-parse", "--verify", "--quiet", plan.targetRef + "^{commit}"});
            if (!tgt)
            {
                return std::unexpected(GitOpsError{
                    .kind = GitOpsError::Kind::BranchNotFound,
                    .exitCode = tgt.error().exitCode,
                    .message = "Branch locale non trovato: " + targetBranch,
                    .command = "git rev-parse"
                });
            }
            auto src = detail::runGitCapture(
                executor, repoPath, {"rev-parse", "--verify", "--quiet", sourceRef + "^{commit}"});
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
                plan.possible = detail::runGitCapture(
                    executor, repoPath, {"merge-base", "--is-ancestor", plan.targetSha, plan.sourceSha}).has_value();
            }
            return plan;
        }
    } // namespace

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
        const std::string_view output = execResult->stdoutBytes;
        std::size_t startPos = 0;
        while (startPos < output.size())
        {
            const auto endPos = output.find('\n', startPos);
            const std::string_view line = endPos == std::string_view::npos
                ? output.substr(startPos)
                : output.substr(startPos, endPos - startPos);
            startPos = endPos == std::string_view::npos ? output.size() : endPos + 1;

            if (auto branch = parseBranchLine(line))
            {
                branches.push_back(std::move(*branch));
            }
        }

        appendUnbornHeadIfEmpty(m_executor, repoPath, branches);
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

        auto res = detail::runGitChecked(
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
            !detail::runGitCapture(m_executor, repoPath,
                                   {"rev-parse", "--verify", "--quiet", "refs/heads/" + checkoutTarget}) &&
            detail::runGitCapture(m_executor, repoPath,
                                  {"rev-parse", "--verify", "--quiet", "refs/remotes/" + checkoutTarget}))
        {
            if (auto remotes = detail::runGitCapture(m_executor, repoPath, {"remote"}))
            {
                std::string localName;
                std::size_t start = 0;
                const std::string& out = *remotes;
                while (start < out.size())
                {
                    auto end = out.find('\n', start);
                    if (end == std::string::npos) end = out.size();
                    const std::string remote = detail::trimString(std::string_view(out).substr(start, end - start));
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
                    if (detail::runGitCapture(m_executor, repoPath,
                                              {"rev-parse", "--verify", "--quiet", "refs/heads/" + localName}))
                    {
                        checkoutTarget = localName;
                    }
                    else
                    {
                        trackRemote = true;
                    }
                }
            }
        }

        std::vector<std::string> args{"checkout"};
        if (options.createNewBranch) args.emplace_back("-b");
        if (options.force) args.emplace_back("-f");
        if (trackRemote) args.emplace_back("--track");
        args.push_back(checkoutTarget);

        auto res = detail::runGitChecked(
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
        auto res = detail::runGitChecked(
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
        auto res = detail::runGitChecked(
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

    std::expected<gf::git::bo::CherryPickResult, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::cherryPick(const std::filesystem::path& repoPath,
                                       const gf::git::bo::CherryPickOptions& options)
    {
        std::vector<std::string> args = {"-c", "core.editor=true", "cherry-pick"};
        if (options.noCommit) args.emplace_back("-n");
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
                if (const auto headSha = detail::runGitCapture(m_executor, repoPath, {"rev-parse", "HEAD"}))
                {
                    result.newCommitSha = *headSha;
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
            result.conflictedFiles = detail::readConflictedFiles(m_executor, repoPath);
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
            .message = detail::pickErrorMessage(*execResult),
            .command = "git cherry-pick"
        });
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::abortCherryPick(const std::filesystem::path& repoPath)
    {
        auto res = detail::runGitChecked(
            m_executor, repoPath, {"cherry-pick", "--abort"}, "annullare il cherry-pick", "git cherry-pick --abort",
            [](const gf::git::bo::GitProcessResult& exec, const std::string& label)
            {
                const std::string combined = exec.stdoutBytes + "\n" + exec.stderrBytes;
                const auto kind = (combined.find("no cherry-pick or revert in progress") != std::string::npos ||
                                   combined.find("nessun cherry-pick") != std::string::npos)
                                      ? gf::git::ports::GitOpsError::Kind::InvalidOperation
                                      : gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                return gf::git::ports::GitOpsError{kind, exec.exitCode, detail::pickErrorMessage(exec), label};
            });
        if (!res) return std::unexpected(res.error());
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
        return detail::runGitVoid(m_executor, repoPath, {"reset", flag, commitSha}, "eseguire il reset", "git reset");
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::revertCommit(const std::filesystem::path& repoPath,
                                         const std::string& commitSha)
    {
        return detail::runGitVoid(m_executor, repoPath, {"-c", "core.editor=true", "revert", "--no-edit", commitSha},
                                  "eseguire revert", "git revert");
    }

    std::expected<std::string, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::rewordCommit(const std::filesystem::path& repoPath,
                                         const std::string& commitSha,
                                         const std::string& newMessage)
    {
        using gf::git::ports::GitOpsError;

        auto message = normalizeRewordMessage(newMessage);
        if (!message) return std::unexpected(message.error());

        if (auto rebasing = isRebaseInProgress(repoPath); rebasing && *rebasing)
        {
            return std::unexpected(GitOpsError{
                GitOpsError::Kind::RebaseInProgress, -1,
                "Impossibile modificare il messaggio: un rebase è in corso", "reword"
            });
        }
        for (const char* pseudoRef : {"MERGE_HEAD", "CHERRY_PICK_HEAD", "REVERT_HEAD"})
        {
            if (detail::runGitCapture(m_executor, repoPath, {"rev-parse", "-q", "--verify", pseudoRef}))
            {
                return std::unexpected(GitOpsError{
                    GitOpsError::Kind::InvalidOperation, -1,
                    std::string("Impossibile modificare il messaggio: operazione in corso (") + pseudoRef + ")",
                    "reword"
                });
            }
        }

        if (commitSha.empty() || commitSha.front() == '-')
        {
            return std::unexpected(GitOpsError{
                GitOpsError::Kind::CommitNotFound, -1, "SHA del commit non valido", "reword"
            });
        }

        auto targetSha = detail::runGitCapture(
            m_executor, repoPath, {"rev-parse", "--verify", "--quiet", commitSha + "^{commit}"});
        if (!targetSha)
        {
            return std::unexpected(GitOpsError{
                GitOpsError::Kind::CommitNotFound, targetSha.error().exitCode,
                "Commit non trovato: " + commitSha, "git rev-parse"
            });
        }
        auto headSha = detail::runGitCapture(m_executor, repoPath, {"rev-parse", "--verify", "HEAD"});
        if (!headSha) return std::unexpected(headSha.error());

        auto descendants = resolveRewordDescendants(m_executor, repoPath, *targetSha, *headSha);
        if (!descendants) return std::unexpected(descendants.error());

        auto identRes = detail::runGitCapture(m_executor, repoPath, {"var", "GIT_COMMITTER_IDENT"});
        if (!identRes)
        {
            return std::unexpected(GitOpsError{
                GitOpsError::Kind::InvalidOperation, identRes.error().exitCode,
                "Identità git non configurata (user.name / user.email): " + identRes.error().message, "git var"
            });
        }
        const std::string& committerIdent = *identRes;

        auto gitDirRes = detail::runGitCapture(m_executor, repoPath, {"rev-parse", "--git-dir"});
        if (!gitDirRes) return std::unexpected(gitDirRes.error());
        std::filesystem::path gitDir = core::utf8ToPath(*gitDirRes);
        if (gitDir.is_relative()) gitDir = repoPath / gitDir;

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

        std::unordered_map<std::string, std::string> rewritten;
        auto rewriteCommit = [&](const std::string& oldSha,
                                 const std::string* replacementMessage) -> std::expected<std::string, GitOpsError>
        {
            auto rawRes = detail::runGitChecked(
                m_executor, repoPath, {"cat-file", "commit", oldSha},
                "git cat-file", "git cat-file", detail::defaultGitFailure);
            if (!rawRes) return std::unexpected(rawRes.error());

            const std::string rebuilt = rebuildCommitObjectBytes(
                rawRes->stdoutBytes, committerIdent, rewritten, replacementMessage);
            {
                std::ofstream ofs(scratchFile, std::ios::binary | std::ios::trunc);
                if (!ofs)
                {
                    return std::unexpected(GitOpsError{
                        GitOpsError::Kind::StorageError, -1,
                        "Impossibile scrivere il file temporaneo per il commit riscritto", "reword"
                    });
                }
                ofs.write(rebuilt.data(), static_cast<std::streamsize>(rebuilt.size()));
            }
            return detail::runGitCapture(
                m_executor, repoPath, {"hash-object", "-t", "commit", "-w", core::pathToUtf8(scratchFile)});
        };

        auto newTarget = rewriteCommit(*targetSha, &*message);
        if (!newTarget) return std::unexpected(newTarget.error());
        rewritten[*targetSha] = *newTarget;

        std::string newHead = *newTarget;
        for (const auto& sha : *descendants)
        {
            auto newSha = rewriteCommit(sha, nullptr);
            if (!newSha) return std::unexpected(newSha.error());
            rewritten[sha] = *newSha;
            newHead = *newSha;
        }

        auto updateRes = detail::runGitVoid(
            m_executor, repoPath,
            {"update-ref", "-m", "GitFlux: reword " + targetSha->substr(0, 7), "HEAD", newHead, *headSha},
            "aggiornare HEAD", "git update-ref");
        if (!updateRes) return std::unexpected(updateRes.error());

        return *newTarget;
    }

    std::expected<bool, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::canFastForwardBranch(const std::filesystem::path& repoPath,
                                                 const std::string& targetBranch,
                                                 const std::string& sourceRef)
    {
        auto plan = planFastForward(m_executor, repoPath, targetBranch, sourceRef);
        if (!plan) return std::unexpected(plan.error());
        return plan->possible;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::fastForwardBranch(const std::filesystem::path& repoPath,
                                              const std::string& targetBranch,
                                              const std::string& sourceRef)
    {
        using gf::git::ports::GitOpsError;

        auto plan = planFastForward(m_executor, repoPath, targetBranch, sourceRef);
        if (!plan) return std::unexpected(plan.error());
        if (plan->targetSha == plan->sourceSha) return {};
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

        // Checked-out branch: update working tree too so local edits are protected.
        if (const auto headRef = detail::runGitCapture(m_executor, repoPath, {"symbolic-ref", "-q", "HEAD"});
            headRef && *headRef == plan->targetRef)
        {
            if (auto merged = detail::runGitCapture(m_executor, repoPath, {"merge", "--ff-only", plan->sourceSha});
                !merged)
            {
                auto err = merged.error();
                err.command = "git merge --ff-only";
                return std::unexpected(std::move(err));
            }
            return {};
        }

        // Branch checked out in another worktree: moving the ref would desync that worktree.
        if (auto worktrees = detail::runGitCapture(m_executor, repoPath, {"worktree", "list", "--porcelain"}))
        {
            const std::string needle = "branch " + plan->targetRef;
            std::size_t start = 0;
            const std::string& out = *worktrees;
            while (start < out.size())
            {
                auto end = out.find('\n', start);
                if (end == std::string::npos) end = out.size();
                if (detail::trimString(std::string_view(out).substr(start, end - start)) == needle)
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

        auto updated = detail::runGitCapture(
            m_executor, repoPath,
            {"update-ref", "-m", "GitFlux: fast-forward " + targetBranch + " to " + sourceRef,
             plan->targetRef, plan->sourceSha, plan->targetSha});
        if (!updated) return std::unexpected(updated.error());
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::createWorktree(const std::filesystem::path& repoPath,
                                           const std::filesystem::path& worktreePath,
                                           const std::string& commitOrBranch)
    {
        std::vector<std::string> args = {"worktree", "add", worktreePath.string()};
        if (!commitOrBranch.empty()) args.push_back(commitOrBranch);
        return detail::runGitVoid(m_executor, repoPath, std::move(args), "creare il worktree", "git worktree");
    }
} // namespace gf::infra::git
