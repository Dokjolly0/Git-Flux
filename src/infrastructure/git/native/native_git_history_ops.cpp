#include "infrastructure/git/native/native_git_ops_internal.hpp"
#include "core/modules/git/bl/commit_log_parser.hpp"

#include <ctime>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gf::infra::git
{
    std::expected<std::vector<gf::git::bo::CommitFileChange>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getCommitFiles(const std::filesystem::path& repoPath,
                                           const std::string& commitSha)
    {
        if (commitSha.empty())
        {
            return std::vector<gf::git::bo::CommitFileChange>{};
        }

        // 1. Run git show -m --first-parent --name-status --format="" <commitSha>
        auto diffTreeRes = detail::runGitChecked(
            m_executor, repoPath,
            {"show", "-m", "--first-parent", "--name-status", "--format=", commitSha},
            "git show", "git show", detail::defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!diffTreeRes)
        {
            return std::unexpected(diffTreeRes.error());
        }

        // 2. Run git show -m --first-parent --numstat --format="" <commitSha>
        auto numstatRes = detail::runGitRaw(
            m_executor, repoPath,
            {"show", "-m", "--first-parent", "--numstat", "--format=", commitSha},
            "git show", "git show", gf::git::bo::GitLockPolicy::NoOptionalLocks);

        std::unordered_map<std::string, detail::NumStatEntry> numstatMap;
        if (numstatRes && numstatRes->exitCode == 0)
        {
            std::istringstream nss(numstatRes->stdoutBytes);
            std::string nline;
            while (std::getline(nss, nline))
            {
                while (!nline.empty() && (nline.back() == '\r' || nline.back() == ' ')) nline.pop_back();
                if (nline.empty()) continue;
                std::string pathStr;
                detail::NumStatEntry entry;
                if (detail::parseNumstatLine(nline, pathStr, entry))
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
            if (!detail::parseNameStatusLine(dline, change)) continue;
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

        auto execResult = detail::runGitChecked(
            m_executor, repoPath, std::move(args), "git blame", "git blame", detail::defaultGitFailure,
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
            else if (const auto firstSpace = line.find(' '); firstSpace == 40)
            {
                currentSha = line.substr(0, 40);
                if (const auto secondSpace = line.find(' ', firstSpace + 1); secondSpace != std::string::npos)
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

        return result;
    }

    std::expected<std::vector<gf::git::bo::GitCommitLogItem>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getCommitLog(const std::filesystem::path& repoPath,
                                         const gf::git::bo::CommitLogOptions& options)
    {
        std::vector<std::string> args{
            "log",
            "--format=%H%x00%h%x00%P%x00%an%x00%ae%x00%aI%x00%cn%x00%ce%x00%cI%x00%D%x00%s%x00%b%x1e"
        };

        if (options.dateOrder) args.emplace_back("--date-order");
        else if (options.topoOrder) args.emplace_back("--topo-order");

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
                const auto it = stashSelectorsBySha.find(c.sha);
                if (it == stashSelectorsBySha.end()) continue;
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
        auto execResult = detail::runGitChecked(
            m_executor, repoPath, {"ls-files"}, "recuperare i file del repository", "git ls-files",
            detail::defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks, 15'000);
        if (!execResult) return std::unexpected(execResult.error());

        std::vector<std::string> files;
        std::string_view out = execResult->stdoutBytes;
        std::size_t start = 0;
        while (start < out.size())
        {
            std::size_t end = out.find('\n', start);
            if (end == std::string_view::npos) end = out.size();
            std::string_view line = out.substr(start, end - start);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (!line.empty()) files.emplace_back(line);
            start = end + 1;
        }
        return files;
    }

    std::expected<std::vector<gf::git::bo::GitTag>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::listTags(const std::filesystem::path& repoPath)
    {
        auto execResult = detail::runGitChecked(
            m_executor, repoPath,
            {
                "tag",
                "-l",
                "--format=%(refname:strip=2)%09%(objectname)%09%(*objectname)%09%(taggername)%09%(taggeremail)%09%(taggerdate:iso-strict)%09%(contents:subject)"
            },
            "recuperare i tag", "git tag -l",
            detail::defaultGitFailure, gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!execResult) return std::unexpected(execResult.error());

        std::vector<gf::git::bo::GitTag> tags;
        std::string_view out(execResult->stdoutBytes);

        while (!out.empty())
        {
            auto nextLine = out.find('\n');
            std::string_view line = nextLine == std::string_view::npos ? out : out.substr(0, nextLine);
            out = nextLine == std::string_view::npos ? std::string_view{} : out.substr(nextLine + 1);

            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (line.empty()) continue;

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
            if (fields.size() < 7) continue;

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

        auto res = detail::runGitChecked(
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
        auto res = detail::runGitChecked(
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
} // namespace gf::infra::git
