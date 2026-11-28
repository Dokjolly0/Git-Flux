#include "desktop/viewmodels/workspace_session_viewmodel.hpp"
#include <QThread>
#include <QElapsedTimer>
#include <QCoreApplication>

#include "core/modules/git/bl/git_conflict_service.hpp"
#include "core/modules/git/bl/git_repository_watcher_service.hpp"
#include "core/modules/git/bl/git_rebase_service.hpp"
#include "core/modules/git/bl/git_stash_service.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/modules/git/bl/git_diff_service.hpp"
#include "core/modules/git/bl/git_branch_service.hpp"
#include "core/modules/git/bl/git_history_service.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"

#include <QMetaObject>
#include <algorithm>
#include <chrono>
#include <unordered_map>

namespace gf::desktop
{
    WorkspaceSessionViewModel::WorkspaceSessionViewModel(repositories::bl::RepositoryService& repoService,
                                                         workspaces::bl::WorkspaceService& workspaceService,
                                                         gf::git::bl::GitWorkingTreeService& statusService,
                                                         gf::git::bl::GitStagingService& stagingService,
                                                         gf::git::bl::GitDiffService& diffService,
                                                         gf::git::bl::GitBranchService& branchService,
                                                         gf::git::bl::GitHistoryService& historyService,
                                                         gf::git::bl::GitRemoteMergeService& remoteMergeService,
                                                         gf::git::bl::GitStashService& stashService,
                                                         gf::git::bl::GitRebaseService& rebaseService,
                                                         gf::git::bl::GitConflictService& conflictService,
                                                         gf::git::bl::GitRepositoryWatcherService* watcherService,
                                                         QObject* parent)
        : QObject(parent)
          , m_repoService(repoService)
          , m_workspaceService(workspaceService)
          , m_rebaseService(rebaseService)
          , m_watcherService(watcherService)
          , m_workingTree(statusService, stagingService, m_gitExecutor, this)
          , m_commitBox(stagingService, m_gitExecutor, this)
          , m_diffViewer(diffService, this)
          , m_commitDiffViewer(diffService, this)
          , m_branchManager(branchService, historyService, remoteMergeService, &rebaseService, m_gitExecutor, this)
          , m_stashManager(stashService, m_gitExecutor, this)
          , m_conflictResolver(conflictService, m_gitExecutor, this)
    {
        connect(&m_commitBox, &CommitBoxViewModel::commitSucceeded,
                this, &WorkspaceSessionViewModel::onCommitSucceeded);

        // Working tree reads are asynchronous: everything that depends on the fresh status
        // (WIP row, HEAD moves, rebase state, tab badges) happens when a refresh lands.
        connect(&m_workingTree, &WorkingTreeViewModel::refreshed,
                this, &WorkspaceSessionViewModel::onWorkingTreeRefreshed);

        connect(&m_branchManager, &BranchManagerViewModel::branchSwitched,
                this, [this](const QString&) { refreshActiveTab(); });

        connect(&m_conflictResolver, &ConflictResolverViewModel::resolutionApplied,
                this, [this](const QString&) { refreshActiveTab(); });

        connect(&m_stashManager, &StashManagerViewModel::operationSucceeded,
                this, [this](const QString& op)
                {
                    if (op == QStringLiteral("push") || op == QStringLiteral("pop") || op == QStringLiteral("apply"))
                    {
                        const QString path = activeRepoPath();
                        if (!path.isEmpty())
                        {
                            m_workingTree.refresh(path);
                        }
                    }
                    if (op == QStringLiteral("push") || op == QStringLiteral("pop") ||
                        op == QStringLiteral("drop") || op == QStringLiteral("clear"))
                    {
                        m_branchManager.refresh();
                    }
                });

        if (m_watcherService)
        {
            m_watcherService->setBatchCallback([this](const gf::git::bo::FsDebouncedBatch& batch)
            {
                onFsDebouncedBatch(batch);
            });
        }
    }

    void WorkspaceSessionViewModel::setTerminalViewModel(TerminalSessionViewModel* terminalVm)
    {
        if (m_terminalVm == terminalVm)
        {
            return;
        }
        if (m_terminalVm)
        {
            disconnect(m_terminalVm, nullptr, this, nullptr);
        }
        m_terminalVm = terminalVm;
        if (m_terminalVm)
        {
            connect(m_terminalVm, &TerminalSessionViewModel::gitCommandFinished,
                    this, [this](const QString& repoPath, int /*exitCode*/)
                    {
                        if (!repoPath.isEmpty() && repoPath == activeRepoPath())
                        {
                            m_workingTree.refresh(repoPath);
                            m_branchManager.refresh();
                            m_stashManager.refresh();
                            checkRebaseStatus();
                        }
                    });
            if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size()))
            {
                const auto& tab = m_tabs[static_cast<std::size_t>(m_activeTabIndex)];
                m_terminalVm->setActiveRepository(tab.repoId, tab.name, tab.path, tab.branch);
            }
        }
        emit terminalChanged();
    }

    QVariantMap WorkspaceSessionViewModel::buildTabVariantMap(const SessionTabItem& item, const int tabIndex) const
    {
        QVariantMap map;
        map[QStringLiteral("tabIndex")] = tabIndex;
        map[QStringLiteral("repoId")] = item.repoId;
        map[QStringLiteral("name")] = item.name;
        map[QStringLiteral("path")] = item.path;
        map[QStringLiteral("isDirty")] = item.isDirty;
        map[QStringLiteral("branch")] = item.branch;
        map[QStringLiteral("primaryGroupId")] = item.primaryGroupId;
        map[QStringLiteral("primaryGroupName")] = item.primaryGroupName;
        map[QStringLiteral("primaryGroupColor")] = item.primaryGroupColor;

        QVariantList groupsList;
        groupsList.reserve(static_cast<qsizetype>(item.groups.size()));
        QStringList groupIds;
        groupIds.reserve(static_cast<qsizetype>(item.groups.size()));
        for (const auto& g : item.groups)
        {
            QVariantMap gMap;
            gMap[QStringLiteral("id")] = g.id;
            gMap[QStringLiteral("name")] = g.name;
            gMap[QStringLiteral("color")] = g.color;
            groupsList.append(gMap);
            groupIds.append(g.id);
        }
        map[QStringLiteral("groups")] = groupsList;
        map[QStringLiteral("groupIds")] = groupIds;
        return map;
    }

    void WorkspaceSessionViewModel::populateTabGroups(SessionTabItem& item) const
    {
        item.groups.clear();
        item.primaryGroupId.clear();
        item.primaryGroupName.clear();
        item.primaryGroupColor.clear();

        if (!m_groupService || item.repoId.isEmpty())
        {
            return;
        }

        const auto groupsRes = m_groupService->getGroupsForRepository(
            repositories::bo::RepositoryId{item.repoId.toStdString()});
        if (!groupsRes || groupsRes->empty())
        {
            return;
        }

        item.groups.reserve(groupsRes->size());
        for (const auto& g : *groupsRes)
        {
            const auto col = g.resolvedColor();
            item.groups.push_back(TabGroupBadge{
                .id = QString::fromStdString(g.id.value()),
                .name = QString::fromStdString(g.name),
                .color = QString::fromUtf8(col.data(), static_cast<qsizetype>(col.size()))
            });
        }

        auto orderRank = [this](const QString& groupId) -> std::size_t
        {
            const auto it = std::ranges::find(m_preferredGroupOrder, groupId);
            return it != m_preferredGroupOrder.end()
                       ? static_cast<std::size_t>(std::distance(m_preferredGroupOrder.begin(), it))
                       : m_preferredGroupOrder.size();
        };

        std::ranges::stable_sort(item.groups, [&](const TabGroupBadge& a, const TabGroupBadge& b)
        {
            const auto rankA = orderRank(a.id);
            const auto rankB = orderRank(b.id);
            if (rankA != rankB)
            {
                return rankA < rankB;
            }
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });

        item.primaryGroupId = item.groups.front().id;
        item.primaryGroupName = item.groups.front().name;
        item.primaryGroupColor = item.groups.front().color;
    }

    void WorkspaceSessionViewModel::rebuildAllTabGroups()
    {
        for (auto& tab : m_tabs)
        {
            populateTabGroups(tab);
        }
    }

    void WorkspaceSessionViewModel::refreshTabGroups()
    {
        if (m_tabs.empty())
        {
            return;
        }
        rebuildAllTabGroups();

        if (m_sessionKind == QStringLiteral("group") && m_groupService && !m_preferredGroupOrder.empty())
        {
            const auto gRes = m_groupService->getGroup(
                groups::bo::GroupId{m_preferredGroupOrder.front().toStdString()});
            if (gRes && gRes->has_value())
            {
                const auto col = (**gRes).resolvedColor();
                m_currentGroupName = QString::fromStdString((**gRes).name);
                m_currentGroupColor = QString::fromUtf8(col.data(), static_cast<qsizetype>(col.size()));
                emit currentGroupNameChanged();
            }
        }
        else if (m_sessionKind == QStringLiteral("workspace") && !m_currentWorkspaceId.isEmpty())
        {
            const auto wsRes = m_workspaceService.getWorkspace(
                workspaces::bo::WorkspaceId{m_currentWorkspaceId.toStdString()});
            if (wsRes && wsRes->has_value())
            {
                const auto col = (**wsRes).resolvedColor();
                m_currentGroupName = QString::fromStdString((**wsRes).name);
                m_currentGroupColor = QString::fromUtf8(col.data(), static_cast<qsizetype>(col.size()));
                emit currentGroupNameChanged();
            }
        }

        emit tabsChanged();
    }

    QVariantList WorkspaceSessionViewModel::tabs() const
    {
        QVariantList list;
        list.reserve(static_cast<qsizetype>(m_tabs.size()));
        for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
        {
            list.append(buildTabVariantMap(m_tabs[static_cast<std::size_t>(i)], i));
        }
        return list;
    }

    QVariantList WorkspaceSessionViewModel::tabClusters() const
    {
        struct ClusterBucket
        {
            QString groupId;
            QString groupName;
            QString groupColor;
            bool isUngrouped = false;
            std::vector<int> tabIndices;
        };

        std::vector<ClusterBucket> buckets;
        std::unordered_map<std::string, std::size_t> keyToBucket;

        for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
        {
            const auto& tab = m_tabs[static_cast<std::size_t>(i)];
            const std::string key = tab.primaryGroupId.toStdString();
            if (const auto it = keyToBucket.find(key); it != keyToBucket.end())
            {
                buckets[it->second].tabIndices.push_back(i);
            }
            else
            {
                keyToBucket.emplace(key, buckets.size());
                buckets.push_back(ClusterBucket{
                    .groupId = tab.primaryGroupId,
                    .groupName = tab.primaryGroupName,
                    .groupColor = tab.primaryGroupColor,
                    .isUngrouped = tab.primaryGroupId.isEmpty(),
                    .tabIndices = {i}
                });
            }
        }

        auto clusterRank = [this](const ClusterBucket& b) -> std::size_t
        {
            if (b.isUngrouped)
            {
                return m_preferredGroupOrder.size() + 2;
            }
            const auto it = std::ranges::find(m_preferredGroupOrder, b.groupId);
            if (it != m_preferredGroupOrder.end())
            {
                return static_cast<std::size_t>(std::distance(m_preferredGroupOrder.begin(), it));
            }
            return m_preferredGroupOrder.size() + 1;
        };

        std::ranges::stable_sort(buckets, [&](const ClusterBucket& a, const ClusterBucket& b)
        {
            return clusterRank(a) < clusterRank(b);
        });

        QVariantList result;
        result.reserve(static_cast<qsizetype>(buckets.size()));
        for (const auto& bucket : buckets)
        {
            QVariantMap cMap;
            cMap[QStringLiteral("groupId")] = bucket.groupId;
            cMap[QStringLiteral("groupName")] = bucket.groupName;
            cMap[QStringLiteral("groupColor")] = bucket.groupColor;
            cMap[QStringLiteral("isUngrouped")] = bucket.isUngrouped;

            QVariantList cTabs;
            cTabs.reserve(static_cast<qsizetype>(bucket.tabIndices.size()));
            for (const int idx : bucket.tabIndices)
            {
                cTabs.append(buildTabVariantMap(m_tabs[static_cast<std::size_t>(idx)], idx));
            }
            cMap[QStringLiteral("tabs")] = cTabs;
            result.append(cMap);
        }
        return result;
    }

    void WorkspaceSessionViewModel::setActiveTabIndex(int index)
    {
        if (m_tabs.empty())
        {
            m_activeTabIndex = -1;
            m_workingTree.refresh({});
            m_diffViewer.clear();
            m_branchManager.setRepositoryPath({});
            emit activeTabChanged();
            return;
        }

        int clamped = std::clamp(index, 0, static_cast<int>(m_tabs.size() - 1));
        if (m_activeTabIndex != clamped)
        {
            m_activeTabIndex = clamped;
            refreshActiveTab();
            persistWorkspaceSession();
            emit activeTabChanged();
        }
    }

    void WorkspaceSessionViewModel::persistWorkspaceSession() const
    {
        if (m_sessionKind != QStringLiteral("workspace") || m_currentWorkspaceId.isEmpty() || m_tabs.empty())
        {
            return;
        }

        const workspaces::bo::WorkspaceId wsId{m_currentWorkspaceId.toStdString()};
        auto sessionRes = m_workspaceService.loadOrCreateSession(wsId);
        workspaces::bo::WorkspaceSession session{};
        if (sessionRes)
        {
            session = *sessionRes;
        }
        else
        {
            session.schemaVersion = 1;
        }

        session.workspaceId = wsId;
        session.openedRepositoryIds.clear();
        session.openedRepositoryIds.reserve(m_tabs.size());
        for (const auto& tab : m_tabs)
        {
            if (!tab.repoId.isEmpty())
            {
                session.openedRepositoryIds.emplace_back(tab.repoId.toStdString());
            }
        }

        const QString currentActiveId = activeRepoId();
        if (!currentActiveId.isEmpty())
        {
            session.activeRepositoryId = repositories::bo::RepositoryId{currentActiveId.toStdString()};
        }
        else
        {
            session.activeRepositoryId.reset();
        }

        session.updatedAtUnixEpochMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        [[maybe_unused]] auto saveRes = m_workspaceService.saveSession(session);
    }

    QString WorkspaceSessionViewModel::activeRepoId() const
    {
        if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size()))
        {
            return m_tabs[static_cast<std::size_t>(m_activeTabIndex)].repoId;
        }
        return {};
    }

    QString WorkspaceSessionViewModel::activeRepoName() const
    {
        if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size()))
        {
            return m_tabs[static_cast<std::size_t>(m_activeTabIndex)].name;
        }
        return {};
    }

    QString WorkspaceSessionViewModel::activeRepoPath() const
    {
        if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size()))
        {
            return m_tabs[static_cast<std::size_t>(m_activeTabIndex)].path;
        }
        return {};
    }

    void WorkspaceSessionViewModel::addTabInternal(const QString& repoId,
                                                   const QString& name,
                                                   const QString& path)
    {
        QString resolvedName = name;
        QString resolvedPath = path;

        if (resolvedName.isEmpty() && !repoId.isEmpty())
        {
            auto repoRes = m_repoService.getRepository(repositories::bo::RepositoryId{repoId.toStdString()});
            if (repoRes && repoRes->has_value())
            {
                resolvedName = QString::fromStdString((*repoRes)->displayName);
                if (resolvedPath.isEmpty())
                {
                    resolvedPath = QString::fromStdString((*repoRes)->localPath.string());
                }
            }
        }

        if (resolvedName.isEmpty() && !resolvedPath.isEmpty())
        {
            resolvedName =
                QString::fromStdString(std::filesystem::path(resolvedPath.toStdString()).filename().string());
        }

        SessionTabItem newItem{
            .repoId = repoId,
            .name = resolvedName.isEmpty() ? QStringLiteral("Repository") : resolvedName,
            .path = resolvedPath,
            .isDirty = false,
            .branch = {}
        };
        populateTabGroups(newItem);
        m_tabs.push_back(std::move(newItem));

        if (m_watcherService && !resolvedPath.isEmpty())
        {
            [[maybe_unused]] auto res = m_watcherService->startWatching(
                std::filesystem::path(resolvedPath.toStdString()));
        }
    }

    void WorkspaceSessionViewModel::openRepository(const QString& repoId,
                                                   const QString& name,
                                                   const QString& path)
    {
        // Check if repository is already open in an existing tab
        for (std::size_t i = 0; i < m_tabs.size(); ++i)
        {
            if (m_tabs[i].repoId == repoId || (!path.isEmpty() && m_tabs[i].path == path))
            {
                setActiveTabIndex(static_cast<int>(i));
                return;
            }
        }

        addTabInternal(repoId, name, path);
        emit tabsChanged();
        setActiveTabIndex(static_cast<int>(m_tabs.size() - 1));
    }

    void WorkspaceSessionViewModel::openRepositories(const QStringList& repoIds)
    {
        if (repoIds.isEmpty())
        {
            return;
        }

        bool anyAdded = false;
        int firstNewIndex = -1;

        for (const auto& id : repoIds)
        {
            auto repoRes = m_repoService.getRepository(repositories::bo::RepositoryId{id.toStdString()});
            if (!repoRes || !repoRes->has_value())
            {
                continue;
            }

            QString rId = QString::fromStdString((*repoRes)->id.value());
            QString rPath = QString::fromStdString((*repoRes)->localPath.string());
            QString rName = QString::fromStdString((*repoRes)->displayName);

            bool alreadyOpen = false;
            for (std::size_t i = 0; i < m_tabs.size(); ++i)
            {
                if (m_tabs[i].repoId == rId || (!rPath.isEmpty() && m_tabs[i].path == rPath))
                {
                    alreadyOpen = true;
                    if (firstNewIndex < 0)
                    {
                        firstNewIndex = static_cast<int>(i);
                    }
                    break;
                }
            }

            if (!alreadyOpen)
            {
                if (firstNewIndex < 0)
                {
                    firstNewIndex = static_cast<int>(m_tabs.size());
                }
                addTabInternal(rId, rName, rPath);
                anyAdded = true;
            }
        }

        if (anyAdded)
        {
            emit tabsChanged();
        }

        if (!m_tabs.empty())
        {
            if (m_activeTabIndex < 0)
            {
                setActiveTabIndex(firstNewIndex >= 0 ? firstNewIndex : 0);
            }
            else if (firstNewIndex >= 0 && m_activeTabIndex != firstNewIndex)
            {
                setActiveTabIndex(firstNewIndex);
            }
        }
    }

    void WorkspaceSessionViewModel::openGroup(const QString& groupName,
                                              const QStringList& repoIds,
                                              const QString& groupId)
    {
        closeAllTabs();
        m_sessionKind = QStringLiteral("group");
        m_currentGroupName = groupName;
        m_currentGroupColor.clear();
        m_preferredGroupOrder.clear();

        if (!groupId.isEmpty())
        {
            m_preferredGroupOrder.push_back(groupId);
            if (m_groupService)
            {
                if (const auto gRes = m_groupService->getGroup(groups::bo::GroupId{groupId.toStdString()});
                    gRes && gRes->has_value())
                {
                    const auto col = (**gRes).resolvedColor();
                    m_currentGroupColor = QString::fromUtf8(col.data(), static_cast<qsizetype>(col.size()));
                }
            }
        }
        else if (m_groupService && !groupName.isEmpty())
        {
            if (const auto allRes = m_groupService->getAllGroups(); allRes)
            {
                for (const auto& g : *allRes)
                {
                    if (QString::fromStdString(g.name) == groupName)
                    {
                        m_preferredGroupOrder.push_back(QString::fromStdString(g.id.value()));
                        const auto col = g.resolvedColor();
                        m_currentGroupColor = QString::fromUtf8(col.data(), static_cast<qsizetype>(col.size()));
                        break;
                    }
                }
            }
        }

        emit currentGroupNameChanged();
        openRepositories(repoIds);
    }

    void WorkspaceSessionViewModel::openWorkspace(const QString& workspaceId)
    {
        auto wsOpt = m_workspaceService.getWorkspace(workspaces::bo::WorkspaceId{workspaceId.toStdString()});
        if (!wsOpt || !wsOpt->has_value())
        {
            return;
        }

        const auto savedSessionRes = m_workspaceService.loadOrCreateSession((*wsOpt)->id);

        closeAllTabs();

        m_sessionKind = QStringLiteral("workspace");
        m_currentWorkspaceId = workspaceId;
        m_currentGroupName = QString::fromStdString((*wsOpt)->name);
        const auto wsCol = (*wsOpt)->resolvedColor();
        m_currentGroupColor = QString::fromUtf8(wsCol.data(), static_cast<qsizetype>(wsCol.size()));
        m_preferredGroupOrder.clear();
        m_preferredGroupOrder.reserve((*wsOpt)->groupIds.size());
        for (const auto& gId : (*wsOpt)->groupIds)
        {
            m_preferredGroupOrder.push_back(QString::fromStdString(gId.value()));
        }
        emit currentGroupNameChanged();

        auto reposRes = m_workspaceService.resolveWorkspaceRepositories(
            workspaces::bo::WorkspaceId{workspaceId.toStdString()});
        if (!reposRes || reposRes->empty())
        {
            return;
        }

        for (const auto& repo : *reposRes)
        {
            QString rId = QString::fromStdString(repo.id.value());
            QString rPath = QString::fromStdString(repo.localPath.string());
            QString rName = QString::fromStdString(repo.displayName);
            addTabInternal(rId, rName, rPath);
        }

        emit tabsChanged();

        if (!m_tabs.empty())
        {
            int targetIndex = 0;
            if (savedSessionRes && savedSessionRes->activeRepositoryId.has_value())
            {
                const QString savedRepoId = QString::fromStdString(savedSessionRes->activeRepositoryId->value());
                for (int i = 0; i < static_cast<int>(m_tabs.size()); ++i)
                {
                    if (m_tabs[static_cast<std::size_t>(i)].repoId == savedRepoId)
                    {
                        targetIndex = i;
                        break;
                    }
                }
            }
            setActiveTabIndex(targetIndex);
        }
    }

    void WorkspaceSessionViewModel::closeTab(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_tabs.size()))
        {
            return;
        }

        const QString closedRepoId = m_tabs[static_cast<std::size_t>(index)].repoId;
        QString closedPath = m_tabs[static_cast<std::size_t>(index)].path;
        if (m_watcherService && !closedPath.isEmpty())
        {
            [[maybe_unused]] auto res = m_watcherService->stopWatching(std::filesystem::path(closedPath.toStdString()));
        }
        if (m_terminalVm)
        {
            m_terminalVm->closeRepositoryTerminals(closedRepoId, closedPath);
        }

        m_tabs.erase(m_tabs.begin() + index);
        emit tabsChanged();

        if (m_tabs.empty())
        {
            m_currentWorkspaceId.clear();
            m_preferredGroupOrder.clear();
            if (!m_currentGroupName.isEmpty() || !m_currentGroupColor.isEmpty() || m_sessionKind != QStringLiteral("single"))
            {
                m_currentGroupName.clear();
                m_currentGroupColor.clear();
                m_sessionKind = QStringLiteral("single");
                emit currentGroupNameChanged();
            }
            m_activeTabIndex = -1;
            m_workingTree.refresh({});
            m_diffViewer.clear();
            m_branchManager.setRepositoryPath({});
            emit activeTabChanged();
        }
        else if (index < m_activeTabIndex)
        {
            --m_activeTabIndex;
            persistWorkspaceSession();
            emit activeTabChanged();
        }
        else if (m_activeTabIndex >= static_cast<int>(m_tabs.size()))
        {
            setActiveTabIndex(static_cast<int>(m_tabs.size() - 1));
        }
        else if (m_activeTabIndex == index)
        {
            refreshActiveTab();
            persistWorkspaceSession();
            emit activeTabChanged();
        }
        else
        {
            persistWorkspaceSession();
        }
    }

    void WorkspaceSessionViewModel::closeAllTabs()
    {
        if (m_watcherService)
        {
            for (const auto& tab : m_tabs)
            {
                if (!tab.path.isEmpty())
                {
                    [[maybe_unused]] auto res = m_watcherService->stopWatching(
                        std::filesystem::path(tab.path.toStdString()));
                }
            }
        }

        m_tabs.clear();
        m_currentWorkspaceId.clear();
        m_preferredGroupOrder.clear();
        m_activeTabIndex = -1;
        if (!m_currentGroupName.isEmpty() || !m_currentGroupColor.isEmpty() || m_sessionKind != QStringLiteral("single"))
        {
            m_currentGroupName.clear();
            m_currentGroupColor.clear();
            m_sessionKind = QStringLiteral("single");
            emit currentGroupNameChanged();
        }
        m_workingTree.refresh({});
        m_diffViewer.clear();
        m_commitDiffViewer.clear();
        m_branchManager.setRepositoryPath({});
        m_stashManager.setRepoPath({});
        m_conflictResolver.setRepoPath({});
        if (m_terminalVm)
        {
            m_terminalVm->closeAllTerminals();
        }
        m_isRebaseInProgress = false;
        emit rebaseStatusChanged();
        emit tabsChanged();
        emit activeTabChanged();
    }

    void WorkspaceSessionViewModel::refreshActiveTab()
    {
        QString path = activeRepoPath();
        if (path.isEmpty())
        {
            m_workingTree.refresh({});
            m_diffViewer.clear();
            m_commitDiffViewer.clear();
            m_branchManager.setRepositoryPath({});
            m_stashManager.setRepoPath({});
            m_conflictResolver.setRepoPath({});
            m_isRebaseInProgress = false;
            emit rebaseStatusChanged();
            return;
        }

        // The history is reloaded below anyway: the first status of this tab is not a HEAD move.
        m_observedHeadOid.clear();
        m_observedBranch.clear();
        m_workingTree.refresh(path);
        m_diffViewer.clear();
        m_commitDiffViewer.clear();
        m_branchManager.setRepositoryPath(path);
        if (m_stashManager.repoPath() == path)
        {
            m_stashManager.refresh();
        }
        else
        {
            m_stashManager.setRepoPath(path);
        }
        m_conflictResolver.setRepoPath(path);
        if (m_terminalVm && m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size()))
        {
            const auto& tab = m_tabs[static_cast<std::size_t>(m_activeTabIndex)];
            m_terminalVm->setActiveRepository(tab.repoId, tab.name, tab.path, tab.branch);
        }
        checkRebaseStatus();
    }

    void WorkspaceSessionViewModel::checkWorkingTreeUpdates()
    {
        QString path = activeRepoPath();
        if (path.isEmpty())
        {
            return;
        }

        // Polled every 1.5 s and on window activation: asynchronous and coalesced, so a slow
        // `git status` never blocks the UI nor piles up. onWorkingTreeRefreshed() handles the result.
        m_workingTree.refresh(path);
    }

    void WorkspaceSessionViewModel::onWorkingTreeRefreshed(const QString& repoPath)
    {
        if (repoPath != activeRepoPath())
        {
            return; // a tab that is no longer active
        }

        const bool hasWip = !m_workingTree.isClean();
        const int count = m_workingTree.stagedCount() + m_workingTree.unstagedCount() + m_workingTree.untrackedCount();
        m_branchManager.setWipStatus(hasWip, count);

        // HEAD commit or branch changed (commit made here or externally, checkout, rebase, reset...):
        // reload the branches and the commit history.
        const QString head = m_workingTree.headOid();
        const QString branch = m_workingTree.branchName();
        const bool headMoved = (!m_observedHeadOid.isEmpty() && head != m_observedHeadOid)
            || (!m_observedBranch.isEmpty() && branch != m_observedBranch);
        m_observedHeadOid = head;
        m_observedBranch = branch;
        if (headMoved)
        {
            m_branchManager.refresh();
        }
        checkRebaseStatus();

        if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size()))
        {
            auto& tab = m_tabs[static_cast<std::size_t>(m_activeTabIndex)];
            if (tab.isDirty != hasWip || tab.branch != branch)
            {
                tab.isDirty = hasWip;
                tab.branch = branch;
                emit tabsChanged();
            }
        }
        if (m_terminalVm && !branch.isEmpty())
        {
            m_terminalVm->updateActiveBranch(repoPath, branch);
        }
    }

    void WorkspaceSessionViewModel::checkRebaseStatus()
    {
        QString path = activeRepoPath();
        if (path.isEmpty())
        {
            if (m_isRebaseInProgress)
            {
                m_isRebaseInProgress = false;
                emit rebaseStatusChanged();
            }
            return;
        }

        auto inProg = m_rebaseService.isRebaseInProgress(std::filesystem::path(path.toStdString()));
        bool status = inProg ? *inProg : false;
        if (m_isRebaseInProgress != status)
        {
            m_isRebaseInProgress = status;
            emit rebaseStatusChanged();
        }
    }

    template <typename Work>
    bool WorkspaceSessionViewModel::runRebaseStep(Work work, bool reportOutcome)
    {
        const QString path = activeRepoPath();
        if (path.isEmpty())
        {
            return false;
        }

        ++m_pendingOperations;
        m_gitExecutor.run(this,
                          [work, repoPath = std::filesystem::path(path.toStdString())]() { return work(repoPath); },
                          [this, path, reportOutcome](auto res)
                          {
                              --m_pendingOperations;
                              if (!res)
                              {
                                  return;
                              }
                              if (path == activeRepoPath())
                              {
                                  refreshActiveTab();
                              }
                              if constexpr (requires { res->status; })
                              {
                                  if (reportOutcome && res->status != gf::git::bo::RebaseStatus::Conflict)
                                  {
                                      emit rebaseSucceeded(QString::fromStdString(res->message));
                                  }
                              }
                          });
        return true;
    }

    bool WorkspaceSessionViewModel::startRebase(const QString& upstream, const QString& onto)
    {
        if (upstream.trimmed().isEmpty())
        {
            return false;
        }

        gf::git::bo::RebaseOptions opts{
            .upstream = upstream.trimmed().toStdString()
        };
        if (!onto.trimmed().isEmpty())
        {
            opts.onto = onto.trimmed().toStdString();
        }

        auto& service = m_rebaseService;
        return runRebaseStep([&service, opts](const std::filesystem::path& repo)
        {
            return service.startRebase(repo, opts);
        }, true);
    }

    bool WorkspaceSessionViewModel::continueRebase()
    {
        auto& service = m_rebaseService;
        return runRebaseStep([&service](const std::filesystem::path& repo) { return service.continueRebase(repo); },
                             true);
    }

    bool WorkspaceSessionViewModel::skipRebase()
    {
        auto& service = m_rebaseService;
        return runRebaseStep([&service](const std::filesystem::path& repo) { return service.skipRebase(repo); },
                             true);
    }

    bool WorkspaceSessionViewModel::abortRebase()
    {
        auto& service = m_rebaseService;
        return runRebaseStep([&service](const std::filesystem::path& repo) { return service.abortRebase(repo); },
                             false);
    }

    bool WorkspaceSessionViewModel::waitForIdle(int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        // A completion may queue follow-up work (an operation refreshes the working tree, which
        // reloads the history...): idle means everything is quiet at the same time.
        // waitForIdle(0) on a child is a non-blocking "is it idle?" check.
        while (true)
        {
            if (m_pendingOperations == 0 && m_workingTree.waitForIdle(0) && m_commitBox.waitForIdle(0)
                && m_branchManager.waitForIdle(0) && m_stashManager.waitForIdle(0)
                && m_conflictResolver.waitForIdle(0))
            {
                return true;
            }
            if (timer.elapsed() > timeoutMs)
            {
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
    }

    bool WorkspaceSessionViewModel::openConflictResolver(const QString& relativePath)
    {
        QString path = activeRepoPath();
        if (path.isEmpty() || relativePath.isEmpty())
        {
            return false;
        }

        m_conflictResolver.setRepoPath(path);
        return m_conflictResolver.loadFile(relativePath);
    }

    void WorkspaceSessionViewModel::onCommitSucceeded(const QString& /*sha*/, const QString& /*summary*/)
    {
        refreshActiveTab();
    }

    void WorkspaceSessionViewModel::onFsDebouncedBatch(const gf::git::bo::FsDebouncedBatch& batch)
    {
        QMetaObject::invokeMethod(this, [this, batch]()
        {
            QString currentActive = activeRepoPath();
            if (currentActive.isEmpty())
            {
                return;
            }

            if (std::filesystem::path(currentActive.toStdString()) == batch.repositoryPath)
            {
                // A ref change (.git/HEAD...) also changes the branch shown in the status and the
                // tab badge: refresh the working tree in both cases (asynchronous, coalesced).
                if (batch.requiresWorkingTreeRefresh() || batch.requiresBranchRefresh())
                {
                    m_workingTree.refresh(currentActive);
                }
                if (batch.requiresBranchRefresh())
                {
                    m_branchManager.setRepositoryPath(currentActive);
                    m_stashManager.refresh();
                }
            }
        }, Qt::QueuedConnection);
    }
} // namespace gf::desktop
