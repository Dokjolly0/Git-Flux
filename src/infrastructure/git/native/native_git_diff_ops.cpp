#include "infrastructure/git/native/native_git_ops_internal.hpp"
#include "core/shared/utf8_path.hpp"

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gf::infra::git
{
    namespace
    {
        using NumStatMap = std::unordered_map<std::string, detail::NumStatEntry>;
        using PerCommitNumstatMap = std::unordered_map<std::string, NumStatMap>;
        using AttributionsByPath = std::unordered_map<std::string, std::vector<gf::git::bo::CommitFileAttribution>>;

        constexpr std::string_view kCommitMarker = "__GF_COMMIT__";

        void trimTrailingLineWhitespace(std::string& line)
        {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            {
                line.pop_back();
            }
        }

        void insertNumstatEntry(NumStatMap& target, const std::string& rawPath, const detail::NumStatEntry& entry)
        {
            target[rawPath] = entry;
            const std::string norm = detail::normalizeNumstatPath(rawPath);
            if (norm != rawPath)
            {
                target[norm] = entry;
            }
        }

        [[nodiscard]] NumStatMap parseFlatNumstat(const std::string& stdoutBytes)
        {
            NumStatMap map;
            std::istringstream nss(stdoutBytes);
            std::string line;
            while (std::getline(nss, line))
            {
                trimTrailingLineWhitespace(line);
                if (line.empty()) continue;
                std::string rawPath;
                detail::NumStatEntry entry;
                if (detail::parseNumstatLine(line, rawPath, entry))
                {
                    insertNumstatEntry(map, rawPath, entry);
                }
            }
            return map;
        }

        [[nodiscard]] PerCommitNumstatMap parsePerCommitNumstat(const std::string& stdoutBytes)
        {
            PerCommitNumstatMap perCommit;
            std::istringstream nss(stdoutBytes);
            std::string line;
            std::string currentSha;
            while (std::getline(nss, line))
            {
                trimTrailingLineWhitespace(line);
                if (line.empty()) continue;
                if (line.starts_with(kCommitMarker))
                {
                    currentSha = line.substr(kCommitMarker.size());
                    continue;
                }
                if (currentSha.empty()) continue;
                std::string rawPath;
                detail::NumStatEntry entry;
                if (detail::parseNumstatLine(line, rawPath, entry))
                {
                    insertNumstatEntry(perCommit[currentSha], rawPath, entry);
                }
            }
            return perCommit;
        }

        [[nodiscard]] gf::git::bo::CommitChangesGroup parseCommitGroupHeader(const std::string_view payload)
        {
            std::vector<std::string> fields;
            std::size_t pos = 0;
            for (int f = 0; f < 4; ++f)
            {
                const auto sep = payload.find('\x1f', pos);
                if (sep == std::string_view::npos) break;
                fields.emplace_back(payload.substr(pos, sep - pos));
                pos = sep + 1;
            }
            fields.emplace_back(pos <= payload.size() ? payload.substr(pos) : std::string_view{});

            gf::git::bo::CommitChangesGroup grp;
            if (fields.size() >= 1) grp.commitSha = fields[0];
            if (fields.size() >= 2) grp.shortSha = fields[1];
            if (fields.size() >= 3) grp.authorName = fields[2];
            if (fields.size() >= 4) grp.authorDateIso = fields[3];
            if (fields.size() >= 5) grp.subject = fields[4];
            return grp;
        }

        void populatePerCommitGroups(const std::string& nameStatusStdout,
                                     const PerCommitNumstatMap& perCommitNumstat,
                                     std::vector<gf::git::bo::CommitChangesGroup>& outGroups)
        {
            std::istringstream dss(nameStatusStdout);
            std::string line;
            gf::git::bo::CommitChangesGroup* currentGroup = nullptr;
            while (std::getline(dss, line))
            {
                trimTrailingLineWhitespace(line);
                if (line.empty()) continue;
                if (line.starts_with(kCommitMarker))
                {
                    outGroups.push_back(parseCommitGroupHeader(std::string_view(line).substr(kCommitMarker.size())));
                    currentGroup = &outGroups.back();
                    continue;
                }
                if (!currentGroup) continue;
                gf::git::bo::CommitFileChange change;
                if (!detail::parseNameStatusLine(line, change)) continue;
                if (const auto cit = perCommitNumstat.find(currentGroup->commitSha); cit != perCommitNumstat.end())
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

        [[nodiscard]] AttributionsByPath buildAttributionsByPath(
            const std::vector<gf::git::bo::CommitChangesGroup>& groups)
        {
            AttributionsByPath byPath;
            for (const auto& grp : groups)
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
                    byPath[f.path].push_back(attr);
                    if (!f.oldPath.empty() && f.oldPath != f.path)
                    {
                        byPath[f.oldPath].push_back(std::move(attr));
                    }
                }
            }
            return byPath;
        }

        [[nodiscard]] std::vector<gf::git::bo::CommitFileAttribution> collectAttributions(
            const AttributionsByPath& byPath,
            const std::string& path,
            const std::string& oldPath)
        {
            std::vector<gf::git::bo::CommitFileAttribution> out;
            std::unordered_set<std::string> seenShas;
            auto appendFrom = [&](const std::string& key)
            {
                if (key.empty()) return;
                if (const auto it = byPath.find(key); it != byPath.end())
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
        }

        [[nodiscard]] bool tryPopulateContiguousRange(gf::git::ports::IGitCommandExecutor& executor,
                                                      const std::filesystem::path& repoPath,
                                                      const std::string& baseRevision,
                                                      const std::string& headSha,
                                                      const AttributionsByPath& attributionsByPath,
                                                      gf::git::bo::MultiCommitChangesSummary& summary)
        {
            auto rangeStatusRes = executor.execute(gf::git::bo::GitCommand{
                .args = {"diff", "--name-status", "-M", baseRevision, headSha},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{30'000}
            });
            if (!rangeStatusRes || rangeStatusRes->exitCode != 0)
            {
                return false;
            }

            auto rangeNumstatRes = executor.execute(gf::git::bo::GitCommand{
                .args = {"diff", "--numstat", "-M", baseRevision, headSha},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks,
                .timeout = std::chrono::milliseconds{30'000}
            });
            const NumStatMap rangeNumstat = (rangeNumstatRes && rangeNumstatRes->exitCode == 0)
                ? parseFlatNumstat(rangeNumstatRes->stdoutBytes)
                : NumStatMap{};

            std::istringstream dss(rangeStatusRes->stdoutBytes);
            std::string line;
            while (std::getline(dss, line))
            {
                trimTrailingLineWhitespace(line);
                if (line.empty()) continue;
                gf::git::bo::CommitFileChange base;
                if (!detail::parseNameStatusLine(line, base)) continue;

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
                item.commits = collectAttributions(attributionsByPath, item.path, item.oldPath);
                summary.totalAdditions += item.additions;
                summary.totalDeletions += item.deletions;
                summary.aggregatedFiles.push_back(std::move(item));
            }
            return true;
        }

        void populateSparseAggregatedFiles(const AttributionsByPath& attributionsByPath,
                                         gf::git::bo::MultiCommitChangesSummary& summary)
        {
            std::unordered_map<std::string, std::size_t> aggIndex;
            for (auto it = summary.perCommitGroups.rbegin(); it != summary.perCommitGroups.rend(); ++it)
            {
                for (const auto& f : it->files)
                {
                    const auto found = aggIndex.find(f.path);
                    if (found == aggIndex.end())
                    {
                        gf::git::bo::MultiCommitFileChange item;
                        item.path = f.path;
                        item.oldPath = f.oldPath;
                        item.status = f.status;
                        item.additions = f.additions;
                        item.deletions = f.deletions;
                        item.isBinary = f.isBinary;
                        item.commits = collectAttributions(attributionsByPath, item.path, item.oldPath);
                        aggIndex[item.path] = summary.aggregatedFiles.size();
                        summary.aggregatedFiles.push_back(std::move(item));
                        continue;
                    }

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

            std::ranges::sort(summary.aggregatedFiles, [](const auto& a, const auto& b)
            {
                return a.path < b.path;
            });
            for (const auto& f : summary.aggregatedFiles)
            {
                summary.totalAdditions += f.additions;
                summary.totalDeletions += f.deletions;
            }
        }
    } // namespace

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
        return detail::parseSingleFileDiff(m_executor, repoPath, std::move(args), filePath,
                                           "git diff", "git diff", true, false);
    }

    std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getAllDiffs(const std::filesystem::path& repoPath,
                                        const gf::git::bo::DiffScope scope)
    {
        if (scope == gf::git::bo::DiffScope::Untracked)
        {
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

        std::vector<std::string> args = scope == gf::git::bo::DiffScope::Staged
            ? std::vector<std::string>{"diff", "--cached", "--no-color", "-p"}
            : std::vector<std::string>{"diff", "--no-color", "-p"};

        auto execResult = detail::runGitRaw(
            m_executor, repoPath, std::move(args), "git diff", "git diff",
            gf::git::bo::GitLockPolicy::NoOptionalLocks);
        if (!execResult) return std::unexpected(execResult.error());

        if (execResult->exitCode > 1)
        {
            return std::unexpected(detail::defaultGitFailure(*execResult, "git diff"));
        }

        return gf::git::bl::UnifiedDiffParser::parse(execResult->stdoutBytes);
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
        return detail::parseSingleFileDiff(
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

        const PerCommitNumstatMap perCommitNumstat = (numstatRes && numstatRes->exitCode == 0)
            ? parsePerCommitNumstat(numstatRes->stdoutBytes)
            : PerCommitNumstatMap{};

        populatePerCommitGroups(nameStatusRes->stdoutBytes, perCommitNumstat, summary.perCommitGroups);
        const AttributionsByPath attributionsByPath = buildAttributionsByPath(summary.perCommitGroups);

        if (summary.isContiguousRange &&
            tryPopulateContiguousRange(m_executor, repoPath, baseRevision, headSha, attributionsByPath, summary))
        {
            return summary;
        }
        summary.isContiguousRange = false;
        summary.rangeBaseRevision.clear();
        summary.rangeHeadSha.clear();

        populateSparseAggregatedFiles(attributionsByPath, summary);
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
        return detail::parseSingleFileDiff(
            m_executor, repoPath,
            {
                "diff", "-M", "--no-ext-diff", "--no-textconv", "--no-color",
                "-U" + std::to_string(effectiveContext), baseRevision, headSha, "--", filePath
            },
            filePath, "git diff", "git diff");
    }
} // namespace gf::infra::git
