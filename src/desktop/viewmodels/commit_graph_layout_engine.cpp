#include "desktop/viewmodels/commit_graph_layout_engine.hpp"

#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <unordered_set>

namespace gf::desktop
{
    namespace
    {
        const QStringList kGraphPalette = {
            QStringLiteral("#06B6D4"), // Cyan (main)
            QStringLiteral("#A855F7"), // Purple
            QStringLiteral("#F97316"), // Orange
            QStringLiteral("#10B981"), // Emerald
            QStringLiteral("#EC4899"), // Pink
            QStringLiteral("#EAB308"), // Yellow
            QStringLiteral("#38BDF8"), // Sky Blue
            QStringLiteral("#8B5CF6")  // Violet
        };

        [[nodiscard]] QVariantMap makeGraphRoute(const int fromLane,
                                                 const int toLane,
                                                 const QString& color,
                                                 const int type,
                                                 const bool dashed = false)
        {
            QVariantMap r;
            r[QStringLiteral("fromLane")] = fromLane;
            r[QStringLiteral("toLane")] = toLane;
            r[QStringLiteral("color")] = color;
            r[QStringLiteral("type")] = type;
            r[QStringLiteral("dashed")] = dashed;
            return r;
        }

        [[nodiscard]] std::string resolveGraphHeadSha(const std::vector<git::bo::GitCommitLogItem>& commits)
        {
            for (const auto& c : commits)
            {
                if (c.sha == "WIP")
                {
                    if (!c.parents.empty()) return c.parents[0];
                    continue;
                }
                for (const auto& dec : c.refDecorations)
                {
                    if (dec.starts_with("HEAD -> ") || dec == "HEAD")
                    {
                        return c.sha;
                    }
                }
            }

            for (const auto& c : commits)
            {
                if (CommitGraphLayoutEngine::isStashCommit(c)) continue;
                const bool isPages = std::ranges::any_of(c.refDecorations, [](const std::string& dec)
                {
                    return dec.find("gh-pages") != std::string::npos;
                });
                if (!isPages) return c.sha;
            }
            return commits.empty() ? std::string{} : commits.front().sha;
        }

        [[nodiscard]] std::unordered_set<std::string> collectHeadReachableShas(
            const std::string& headSha,
            const std::unordered_map<std::string, const git::bo::GitCommitLogItem*>& commitMap)
        {
            std::unordered_set<std::string> reachable;
            std::vector<std::string> stack;
            if (!headSha.empty())
            {
                stack.push_back(headSha);
                reachable.insert(headSha);
            }
            while (!stack.empty())
            {
                std::string curr = std::move(stack.back());
                stack.pop_back();
                const auto it = commitMap.find(curr);
                if (it == commitMap.end()) continue;
                for (const auto& p : it->second->parents)
                {
                    if (reachable.insert(p).second) stack.push_back(p);
                }
            }
            return reachable;
        }

        [[nodiscard]] std::unordered_set<std::string> collectOrphanShas(
            const std::vector<git::bo::GitCommitLogItem>& commits,
            const std::unordered_set<std::string>& headReachable)
        {
            std::unordered_set<std::string> orphanShas;
            for (const auto& c : commits)
            {
                for (const auto& dec : c.refDecorations)
                {
                    if (dec.find("gh-pages") != std::string::npos)
                    {
                        orphanShas.insert(c.sha);
                        break;
                    }
                }
            }

            bool expanded = true;
            while (expanded)
            {
                expanded = false;
                for (const auto& c : commits)
                {
                    if (orphanShas.contains(c.sha))
                    {
                        for (const auto& p : c.parents)
                        {
                            if (!headReachable.contains(p) && orphanShas.insert(p).second)
                            {
                                expanded = true;
                            }
                        }
                    }
                }
            }
            return orphanShas;
        }

        struct LaneGraphState
        {
            std::vector<std::string> activeLanes;
            std::vector<QString> laneColors;
            std::vector<bool> laneIsStash;
            int mainTrunkLane = 0;

            void ensureSize(const int targetLane)
            {
                while (static_cast<int>(activeLanes.size()) <= targetLane)
                {
                    const int newIdx = static_cast<int>(activeLanes.size());
                    activeLanes.emplace_back();
                    laneColors.push_back(kGraphPalette[newIdx % kGraphPalette.size()]);
                    laneIsStash.push_back(false);
                }
                while (static_cast<int>(laneColors.size()) <= targetLane)
                {
                    const int newIdx = static_cast<int>(laneColors.size());
                    laneColors.push_back(kGraphPalette[newIdx % kGraphPalette.size()]);
                }
                while (static_cast<int>(laneIsStash.size()) <= targetLane)
                {
                    laneIsStash.push_back(false);
                }
            }

            [[nodiscard]] int findExpectedLane(const std::string& sha, const bool isOrphan) const
            {
                for (int l = 0; l < static_cast<int>(activeLanes.size()); ++l)
                {
                    if (laneIsStash[l] || activeLanes[l] != sha) continue;
                    if (isOrphan && l >= mainTrunkLane) continue;
                    if (!isOrphan && l < mainTrunkLane) continue;
                    return l;
                }
                return -1;
            }

            [[nodiscard]] int findParentLane(const std::string& sha) const
            {
                for (int l = 0; l < static_cast<int>(activeLanes.size()); ++l)
                {
                    if (!laneIsStash[l] && activeLanes[l] == sha) return l;
                }
                return -1;
            }

            [[nodiscard]] int findFreeLane(const int beginLane, const int endLane) const
            {
                const int limit = std::min(endLane, static_cast<int>(activeLanes.size()));
                for (int l = beginLane; l < limit; ++l)
                {
                    if (activeLanes[l].empty()) return l;
                }
                return -1;
            }
        };

        [[nodiscard]] int assignCommitLane(const git::bo::GitCommitLogItem& commit,
                                           const bool isStash,
                                           const bool isOrphan,
                                           const std::string& headSha,
                                           const std::unordered_set<std::string>& headReachable,
                                           const LaneGraphState& state,
                                           bool& wasExpectedFromAbove)
        {
            int commitLane = isStash ? -1 : state.findExpectedLane(commit.sha, isOrphan);
            wasExpectedFromAbove = (commitLane != -1);
            if (commitLane != -1) return commitLane;

            if (isStash)
            {
                commitLane = state.findFreeLane(state.mainTrunkLane + 1, static_cast<int>(state.activeLanes.size()));
                return commitLane != -1 ? commitLane
                    : std::max(state.mainTrunkLane + 1, static_cast<int>(state.activeLanes.size()));
            }
            if (isOrphan)
            {
                commitLane = state.findFreeLane(0, state.mainTrunkLane);
                return commitLane != -1 ? commitLane : 0;
            }

            const bool preferMain = (commit.sha == headSha || headReachable.contains(commit.sha) || commit.sha == "WIP") &&
                                    state.activeLanes[state.mainTrunkLane].empty();
            if (preferMain) return state.mainTrunkLane;

            commitLane = state.findFreeLane(state.mainTrunkLane, static_cast<int>(state.activeLanes.size()));
            return commitLane != -1 ? commitLane
                : std::max(state.mainTrunkLane, static_cast<int>(state.activeLanes.size()));
        }

        void emitPassThroughAndInboundRoutes(const git::bo::GitCommitLogItem& commit,
                                             const int commitLane,
                                             const QString& nodeColor,
                                             const bool wasExpectedFromAbove,
                                             LaneGraphState& state,
                                             QVariantList& routes)
        {
            for (int l = 0; l < static_cast<int>(state.activeLanes.size()); ++l)
            {
                if (l == commitLane || state.activeLanes[l].empty()) continue;
                if (state.laneIsStash[l] && state.activeLanes[l] == commit.sha)
                {
                    routes.append(makeGraphRoute(l, commitLane, QStringLiteral("#F59E0B"), 2, true));
                    state.activeLanes[l].clear();
                    state.laneIsStash[l] = false;
                }
                else
                {
                    const QString routeColor = state.laneIsStash[l] ? QStringLiteral("#F59E0B") : state.laneColors[l];
                    routes.append(makeGraphRoute(l, l, routeColor, 0, state.laneIsStash[l]));
                }
            }

            if (wasExpectedFromAbove)
            {
                routes.append(makeGraphRoute(commitLane, commitLane, nodeColor, 2, false));
            }
        }

        void emitOutboundParentRoutes(const git::bo::GitCommitLogItem& commit,
                                      const bool isStash,
                                      const int commitLane,
                                      const QString& nodeColor,
                                      const std::unordered_set<std::string>& knownShas,
                                      LaneGraphState& state,
                                      QVariantList& routes)
        {
            state.activeLanes[commitLane].clear();
            state.laneIsStash[commitLane] = false;

            const auto& parents = commit.parents;
            if (isStash)
            {
                if (parents.empty()) return;
                const auto& baseSha = parents[0];
                const int baseLane = state.findParentLane(baseSha);
                if (baseLane != -1)
                {
                    routes.append(makeGraphRoute(commitLane, baseLane, QStringLiteral("#F59E0B"), 1, true));
                }
                else if (knownShas.contains(baseSha))
                {
                    state.activeLanes[commitLane] = baseSha;
                    state.laneIsStash[commitLane] = true;
                    routes.append(makeGraphRoute(commitLane, commitLane, QStringLiteral("#F59E0B"), 1, true));
                }
                else
                {
                    routes.append(makeGraphRoute(commitLane, commitLane, QStringLiteral("#F59E0B"), 1, true));
                }
                return;
            }

            if (parents.empty()) return;

            // First parent (never merge a real branch onto a stash lane)
            const auto& p0 = parents[0];
            if (knownShas.contains(p0))
            {
                if (const int p0Lane = state.findParentLane(p0); p0Lane != -1)
                {
                    routes.append(makeGraphRoute(commitLane, p0Lane, nodeColor, 1, false));
                }
                else
                {
                    state.activeLanes[commitLane] = p0;
                    state.laneIsStash[commitLane] = false;
                    routes.append(makeGraphRoute(commitLane, commitLane, nodeColor, 1, false));
                }
            }
            else
            {
                routes.append(makeGraphRoute(commitLane, commitLane, nodeColor, 1, false));
            }

            // Secondary parents (merge commits)
            for (std::size_t pIdx = 1; pIdx < parents.size(); ++pIdx)
            {
                const auto& pOther = parents[pIdx];
                if (!knownShas.contains(pOther)) continue;
                int pOtherLane = state.findParentLane(pOther);
                if (pOtherLane == -1)
                {
                    pOtherLane = state.findFreeLane(state.mainTrunkLane, static_cast<int>(state.activeLanes.size()));
                    if (pOtherLane == -1)
                    {
                        pOtherLane = std::max(state.mainTrunkLane, static_cast<int>(state.activeLanes.size()));
                        state.ensureSize(pOtherLane);
                    }
                    state.activeLanes[pOtherLane] = pOther;
                    state.laneIsStash[pOtherLane] = false;
                }
                routes.append(makeGraphRoute(commitLane, pOtherLane, state.laneColors[pOtherLane], 1, false));
            }
        }

        struct BranchSeed
        {
            int commitIdx = -1;
            int tier = 4;
            bool isRemote = false;
            bool isHead = false;
            bool isPages = false;
            InferredBranchInfo info;
        };

        [[nodiscard]] std::vector<BranchSeed> collectBranchSeeds(const std::vector<git::bo::GitCommitLogItem>& rawCommits)
        {
            std::vector<BranchSeed> seeds;
            for (std::size_t i = 0; i < rawCommits.size(); ++i)
            {
                const auto& c = rawCommits[i];
                if (c.sha.empty() || c.sha == "WIP" || CommitGraphLayoutEngine::isStashCommit(c) ||
                    c.refDecorations.empty())
                {
                    continue;
                }

                struct ParsedRef
                {
                    QString name;
                    bool isHead = false;
                };
                QList<ParsedRef> locals;
                QList<ParsedRef> remotes;

                for (const auto& decStd : c.refDecorations)
                {
                    QString dec = QString::fromStdString(decStd).trimmed();
                    if (dec.isEmpty() || dec == QLatin1String("HEAD") || dec == QLatin1String("origin/HEAD") ||
                        dec.endsWith(QLatin1String("/HEAD")) || dec.startsWith(QLatin1String("tag: ")) ||
                        dec == QLatin1String("refs/stash") || dec.startsWith(QLatin1String("stash@{")))
                    {
                        continue;
                    }

                    bool isHead = false;
                    if (dec.startsWith(QLatin1String("HEAD -> ")))
                    {
                        isHead = true;
                        dec = dec.mid(8).trimmed();
                    }

                    const bool isRemote = !isHead && (dec.startsWith(QLatin1String("origin/")) ||
                        dec.startsWith(QLatin1String("upstream/")) || dec.startsWith(QLatin1String("nas/")));
                    (isRemote ? remotes : locals).append(ParsedRef{.name = dec, .isHead = isHead});
                }

                auto addSeed = [&](const QString& displayName, const bool isRemote, const bool isHead)
                {
                    const QString clean = displayName.startsWith(QLatin1String("origin/"))
                        ? displayName.mid(7) : displayName;
                    const bool isPages = (clean == QLatin1String("gh-pages") ||
                                          displayName.contains(QLatin1String("gh-pages")));

                    int tier = isRemote ? 4 : 3;
                    if (clean == QLatin1String("main") || clean == QLatin1String("master")) tier = 0;
                    else if (clean == QLatin1String("develop") || clean == QLatin1String("dev")) tier = 1;
                    else if (isHead) tier = 2;

                    InferredBranchInfo info;
                    info.name = displayName;
                    if (isPages)
                    {
                        info.borderColor = QStringLiteral("#A855F7");
                        info.bgColor = QStringLiteral("#2E1065CC");
                    }
                    else if (isRemote)
                    {
                        info.borderColor = QStringLiteral("#3B82F6");
                        info.bgColor = QStringLiteral("#1E293BCC");
                    }
                    else if (isHead)
                    {
                        info.borderColor = QStringLiteral("#06B6D4");
                        info.bgColor = QStringLiteral("#0E7490CC");
                    }
                    else
                    {
                        info.borderColor = QStringLiteral("#10B981");
                        info.bgColor = QStringLiteral("#064E3BCC");
                    }

                    seeds.push_back(BranchSeed{
                        .commitIdx = static_cast<int>(i),
                        .tier = tier,
                        .isRemote = isRemote,
                        .isHead = isHead,
                        .isPages = isPages,
                        .info = std::move(info)
                    });
                };

                for (const auto& lb : locals) addSeed(lb.name, false, lb.isHead);
                for (const auto& rb : remotes)
                {
                    const bool matchedLocal = std::ranges::any_of(locals, [&rb](const ParsedRef& lb)
                    {
                        return rb.name == (QLatin1String("origin/") + lb.name) ||
                               rb.name.endsWith(QLatin1String("/") + lb.name);
                    });
                    if (!matchedLocal) addSeed(rb.name, true, false);
                }
            }
            return seeds;
        }
    } // namespace

    bool CommitGraphLayoutEngine::isStashDecoration(const std::string& dec)
    {
        return dec == "refs/stash" || dec == "stash" || dec.starts_with("stash@{");
    }

    bool CommitGraphLayoutEngine::isStashCommit(const git::bo::GitCommitLogItem& commit)
    {
        return std::ranges::any_of(commit.refDecorations, [](const std::string& d)
        {
            return isStashDecoration(d);
        });
    }

    QString CommitGraphLayoutEngine::extractStashSelector(const std::vector<std::string>& decorations)
    {
        for (const auto& dec : decorations)
        {
            if (dec.starts_with("stash@{")) return QString::fromStdString(dec);
        }
        for (const auto& dec : decorations)
        {
            if (dec == "refs/stash" || dec == "stash") return QStringLiteral("stash@{0}");
        }
        return {};
    }

    CommitGraphLayout CommitGraphLayoutEngine::buildGraphLayout(
        const std::vector<git::bo::GitCommitLogItem>& commits)
    {
        CommitGraphLayout layout;
        layout.items.resize(commits.size());
        if (commits.empty()) return layout;

        std::unordered_set<std::string> knownShas;
        knownShas.reserve(commits.size());
        std::unordered_map<std::string, const git::bo::GitCommitLogItem*> commitMap;
        commitMap.reserve(commits.size());
        for (const auto& c : commits)
        {
            knownShas.insert(c.sha);
            commitMap[c.sha] = &c;
        }

        const std::string headSha = resolveGraphHeadSha(commits);
        const auto headReachable = collectHeadReachableShas(headSha, commitMap);
        const auto orphanShas = collectOrphanShas(commits, headReachable);

        auto isOrphanCommit = [&](const git::bo::GitCommitLogItem& c) -> bool
        {
            if (isStashCommit(c)) return false;
            if (orphanShas.contains(c.sha)) return true;
            return c.sha != "WIP" && !headReachable.contains(c.sha) && c.parents.empty() && c.sha != headSha;
        };

        const bool hasOrphans = std::ranges::any_of(commits, isOrphanCommit);

        LaneGraphState state;
        state.mainTrunkLane = hasOrphans ? 1 : 0;
        if (hasOrphans)
        {
            state.activeLanes.resize(2);
            state.laneColors = {QStringLiteral("#A855F7"), QStringLiteral("#06B6D4")};
            state.laneIsStash.resize(2, false);
        }
        else
        {
            state.activeLanes.resize(1);
            state.laneColors = {QStringLiteral("#06B6D4")};
            state.laneIsStash.resize(1, false);
        }

        for (std::size_t i = 0; i < commits.size(); ++i)
        {
            const auto& commit = commits[i];
            const bool isStash = isStashCommit(commit);
            const QString stashSelector = isStash ? extractStashSelector(commit.refDecorations) : QString{};
            const bool isOrphan = !isStash && isOrphanCommit(commit);

            bool wasExpectedFromAbove = false;
            const int commitLane = assignCommitLane(
                commit, isStash, isOrphan, headSha, headReachable, state, wasExpectedFromAbove);
            state.ensureSize(commitLane);

            const QString nodeColor = (commit.sha == "WIP" || isStash)
                ? QStringLiteral("#F59E0B")
                : state.laneColors[commitLane];

            QVariantList routes;
            emitPassThroughAndInboundRoutes(commit, commitLane, nodeColor, wasExpectedFromAbove, state, routes);
            emitOutboundParentRoutes(commit, isStash, commitLane, nodeColor, knownShas, state, routes);

            while (state.activeLanes.size() > static_cast<std::size_t>(state.mainTrunkLane + 1) &&
                   state.activeLanes.back().empty())
            {
                state.activeLanes.pop_back();
                if (state.laneIsStash.size() > state.activeLanes.size()) state.laneIsStash.pop_back();
            }

            layout.maxLanes = std::max(layout.maxLanes, std::max(1, static_cast<int>(state.activeLanes.size())));
            layout.maxLanes = std::max(layout.maxLanes, commitLane + 1);
            layout.items[i] = CommitGraphNode{
                .lane = commitLane,
                .color = nodeColor,
                .routes = std::move(routes),
                .isOrphan = isOrphan,
                .isStash = isStash,
                .stashSelector = stashSelector
            };
        }

        return layout;
    }

    std::unordered_map<std::string, InferredBranchInfo> CommitGraphLayoutEngine::computeInferredBranches(
        const std::vector<git::bo::GitCommitLogItem>& rawCommits)
    {
        std::unordered_map<std::string, InferredBranchInfo> inferred;
        if (rawCommits.empty()) return inferred;

        const std::size_t n = rawCommits.size();
        std::unordered_map<std::string_view, int> shaToIdx;
        shaToIdx.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto& c = rawCommits[i];
            if (!c.sha.empty() && c.sha != "WIP" && !isStashCommit(c))
            {
                shaToIdx.try_emplace(c.sha, static_cast<int>(i));
            }
        }

        std::vector<std::vector<int>> parentsIdx(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto& c = rawCommits[i];
            if (c.sha.empty() || c.sha == "WIP" || isStashCommit(c)) continue;
            parentsIdx[i].reserve(c.parents.size());
            for (const auto& p : c.parents)
            {
                if (const auto it = shaToIdx.find(p); it != shaToIdx.end())
                {
                    parentsIdx[i].push_back(it->second);
                }
            }
        }

        auto seeds = collectBranchSeeds(rawCommits);
        if (seeds.empty()) return inferred;

        std::stable_sort(seeds.begin(), seeds.end(), [](const BranchSeed& a, const BranchSeed& b)
        {
            if (a.tier != b.tier) return a.tier < b.tier;
            if (a.isRemote != b.isRemote) return !a.isRemote;
            if (a.isHead != b.isHead) return a.isHead;
            if (a.isPages != b.isPages) return !a.isPages;
            return a.commitIdx < b.commitIdx;
        });

        std::vector<int> ownerSeed(n, -1);

        // Pass 1: First-parent chains so each branch owns its direct lineage before merges claim side branches.
        for (int s = 0; s < static_cast<int>(seeds.size()); ++s)
        {
            int u = seeds[static_cast<std::size_t>(s)].commitIdx;
            while (u >= 0 && ownerSeed[static_cast<std::size_t>(u)] == -1)
            {
                ownerSeed[static_cast<std::size_t>(u)] = s;
                const auto& ps = parentsIdx[static_cast<std::size_t>(u)];
                u = ps.empty() ? -1 : ps[0];
            }
        }

        // Pass 2: Expand through merged/secondary parents for commits whose original branch tip no longer exists.
        std::vector<std::uint32_t> visitStamp(n, 0);
        std::uint32_t currentStamp = 0;
        std::vector<int> stack;
        stack.reserve(std::min<std::size_t>(n, 256));

        for (int s = 0; s < static_cast<int>(seeds.size()); ++s)
        {
            const int rootIdx = seeds[static_cast<std::size_t>(s)].commitIdx;
            if (ownerSeed[static_cast<std::size_t>(rootIdx)] != s) continue;

            if (++currentStamp == 0)
            {
                std::ranges::fill(visitStamp, 0);
                currentStamp = 1;
            }
            visitStamp[static_cast<std::size_t>(rootIdx)] = currentStamp;
            stack.clear();
            stack.push_back(rootIdx);

            while (!stack.empty())
            {
                const int u = stack.back();
                stack.pop_back();
                for (const int pIdx : parentsIdx[static_cast<std::size_t>(u)])
                {
                    if (visitStamp[static_cast<std::size_t>(pIdx)] == currentStamp) continue;
                    const int existingOwner = ownerSeed[static_cast<std::size_t>(pIdx)];
                    if (existingOwner == -1 || existingOwner == s)
                    {
                        if (existingOwner == -1) ownerSeed[static_cast<std::size_t>(pIdx)] = s;
                        visitStamp[static_cast<std::size_t>(pIdx)] = currentStamp;
                        stack.push_back(pIdx);
                    }
                }
            }
        }

        inferred.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const int s = ownerSeed[i];
            if (s >= 0 && !rawCommits[i].sha.empty())
            {
                inferred.emplace(rawCommits[i].sha, seeds[static_cast<std::size_t>(s)].info);
            }
        }
        return inferred;
    }

    QVariantList CommitGraphLayoutEngine::computeStructuredDecorations(const std::vector<std::string>& rawDecorations)
    {
        struct RawRef
        {
            QString name;
            bool isTag = false;
            bool isRemote = false;
            bool isHead = false;
        };

        QList<RawRef> tags;
        QList<RawRef> localBranches;
        QList<RawRef> remoteBranches;
        QStringList stashes;

        for (const auto& decStd : rawDecorations)
        {
            QString dec = QString::fromStdString(decStd).trimmed();
            if (dec.isEmpty()) continue;
            if (dec == QLatin1String("origin/HEAD") || dec.endsWith(QLatin1String("/HEAD")) ||
                dec == QLatin1String("HEAD"))
            {
                continue;
            }

            if (isStashDecoration(decStd))
            {
                const QString selector = dec.startsWith(QLatin1String("stash@{")) ? dec : QStringLiteral("stash@{0}");
                if (!stashes.contains(selector)) stashes.append(selector);
                continue;
            }
            if (dec.startsWith(QLatin1String("tag: ")))
            {
                tags.append(RawRef{.name = dec.mid(5).trimmed(), .isTag = true});
                continue;
            }

            bool isHead = false;
            if (dec.startsWith(QLatin1String("HEAD -> ")))
            {
                isHead = true;
                dec = dec.mid(8).trimmed();
            }
            if (dec.contains(QLatin1Char('/')))
            {
                remoteBranches.append(RawRef{.name = dec, .isRemote = true, .isHead = isHead});
            }
            else
            {
                localBranches.append(RawRef{.name = dec, .isHead = isHead});
            }
        }

        QVariantList result;

        // Suppress aligned local branch B when origin/B sits at the same commit; forward HEAD mark.
        for (const auto& lb : localBranches)
        {
            bool hasMatchingRemote = false;
            for (auto& rb : remoteBranches)
            {
                if (rb.name == (QLatin1String("origin/") + lb.name) ||
                    rb.name.endsWith(QLatin1String("/") + lb.name))
                {
                    hasMatchingRemote = true;
                    if (lb.isHead) rb.isHead = true;
                    break;
                }
            }
            if (hasMatchingRemote) continue;

            const bool isPages = (lb.name == QLatin1String("gh-pages") || lb.name.contains(QLatin1String("gh-pages")));
            QVariantMap badge;
            badge[QStringLiteral("name")] = lb.name;
            badge[QStringLiteral("type")] = QStringLiteral("local");
            badge[QStringLiteral("isHead")] = lb.isHead;
            badge[QStringLiteral("isOrphan")] = isPages;
            if (isPages)
            {
                badge[QStringLiteral("label")] = lb.isHead
                    ? QStringLiteral("✓ 📄 %1 💻").arg(lb.name)
                    : QStringLiteral("📄 %1 💻").arg(lb.name);
                badge[QStringLiteral("bgColor")] = QStringLiteral("#2E1065");
                badge[QStringLiteral("borderColor")] = QStringLiteral("#A855F7");
                badge[QStringLiteral("fgColor")] = QStringLiteral("#E9D5FF");
            }
            else
            {
                badge[QStringLiteral("label")] = lb.isHead
                    ? QStringLiteral("✓ %1 💻").arg(lb.name)
                    : QStringLiteral("%1 💻").arg(lb.name);
                badge[QStringLiteral("bgColor")] = lb.isHead ? QStringLiteral("#0E7490") : QStringLiteral("#064E3B");
                badge[QStringLiteral("borderColor")] = lb.isHead ? QStringLiteral("#06B6D4") : QStringLiteral("#10B981");
                badge[QStringLiteral("fgColor")] = QStringLiteral("#FFFFFF");
            }
            result.append(badge);
        }

        for (const auto& rb : remoteBranches)
        {
            QString cleanName = rb.name.startsWith(QLatin1String("origin/")) ? rb.name.mid(7) : rb.name;
            const bool isPages = (cleanName == QLatin1String("gh-pages") || rb.name.contains(QLatin1String("gh-pages")));
            QVariantMap badge;
            badge[QStringLiteral("name")] = rb.name;
            badge[QStringLiteral("type")] = QStringLiteral("remote");
            badge[QStringLiteral("isHead")] = rb.isHead;
            badge[QStringLiteral("isOrphan")] = isPages;
            if (isPages)
            {
                badge[QStringLiteral("label")] = QStringLiteral("📄 %1 ☁").arg(cleanName);
                badge[QStringLiteral("bgColor")] = QStringLiteral("#2E1065");
                badge[QStringLiteral("borderColor")] = QStringLiteral("#A855F7");
                badge[QStringLiteral("fgColor")] = QStringLiteral("#E9D5FF");
            }
            else
            {
                badge[QStringLiteral("label")] = QStringLiteral("%1 ☁").arg(cleanName);
                badge[QStringLiteral("bgColor")] = QStringLiteral("#1E293B");
                badge[QStringLiteral("borderColor")] = QStringLiteral("#3B82F6");
                badge[QStringLiteral("fgColor")] = QStringLiteral("#93C5FD");
            }
            result.append(badge);
        }

        for (const auto& selector : stashes)
        {
            QVariantMap badge;
            badge[QStringLiteral("name")] = selector;
            badge[QStringLiteral("type")] = QStringLiteral("stash");
            badge[QStringLiteral("isHead")] = false;
            badge[QStringLiteral("isOrphan")] = false;
            badge[QStringLiteral("label")] = QStringLiteral("📦 %1").arg(selector);
            badge[QStringLiteral("bgColor")] = QStringLiteral("#451A03");
            badge[QStringLiteral("borderColor")] = QStringLiteral("#F59E0B");
            badge[QStringLiteral("fgColor")] = QStringLiteral("#FDE68A");
            result.append(badge);
        }

        for (const auto& tb : tags)
        {
            QVariantMap badge;
            badge[QStringLiteral("name")] = tb.name;
            badge[QStringLiteral("type")] = QStringLiteral("tag");
            badge[QStringLiteral("isHead")] = false;
            badge[QStringLiteral("label")] = QStringLiteral("🏷 %1").arg(tb.name);
            badge[QStringLiteral("bgColor")] = QStringLiteral("#78350F");
            badge[QStringLiteral("borderColor")] = QStringLiteral("#F59E0B");
            badge[QStringLiteral("fgColor")] = QStringLiteral("#FEF3C7");
            result.append(badge);
        }

        return result;
    }
} // namespace gf::desktop
