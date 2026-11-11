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
#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"

#include <QMetaObject>
#include <algorithm>

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

        if (m_watcherService)
        {
            m_watcherService->setBatchCallback([this](const gf::git::bo::FsDebouncedBatch& batch)
            {
                onFsDebouncedBatch(batch);
            });
        }
    }

    QVariantList WorkspaceSessionViewModel::tabs() const
    {
        QVariantList list;
        list.reserve(static_cast<qsizetype>(m_tabs.size()));
        for (const auto& item : m_tabs)
        {
            QVariantMap map;
            map[QStringLiteral("repoId")] = item.repoId;
            map[QStringLiteral("name")] = item.name;
            map[QStringLiteral("path")] = item.path;
            map[QStringLiteral("isDirty")] = item.isDirty;
            map[QStringLiteral("branch")] = item.branch;
            list.append(map);
        }
        return list;
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
            emit activeTabChanged();
        }
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

        m_tabs.push_back(SessionTabItem{
            .repoId = repoId,
            .name = resolvedName.isEmpty() ? QStringLiteral("Repository") : resolvedName,
            .path = resolvedPath,
            .isDirty = false,
            .branch = {}
        });

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

    void WorkspaceSessionViewModel::openGroup(const QString& groupName, const QStringList& repoIds)
    {
        closeAllTabs();
        m_currentGroupName = groupName;
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

        closeAllTabs();

        m_currentGroupName = QString::fromStdString((*wsOpt)->name);
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
            setActiveTabIndex(0);
        }
    }

    void WorkspaceSessionViewModel::closeTab(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_tabs.size()))
        {
            return;
        }

        QString closedPath = m_tabs[static_cast<std::size_t>(index)].path;
        if (m_watcherService && !closedPath.isEmpty())
        {
            [[maybe_unused]] auto res = m_watcherService->stopWatching(std::filesystem::path(closedPath.toStdString()));
        }

        m_tabs.erase(m_tabs.begin() + index);
        emit tabsChanged();

        if (m_tabs.empty())
        {
            if (!m_currentGroupName.isEmpty())
            {
                m_currentGroupName.clear();
                emit currentGroupNameChanged();
            }
            m_activeTabIndex = -1;
            m_workingTree.refresh({});
            m_diffViewer.clear();
            m_branchManager.setRepositoryPath({});
            emit activeTabChanged();
        }
        else if (m_activeTabIndex >= static_cast<int>(m_tabs.size()))
        {
            setActiveTabIndex(static_cast<int>(m_tabs.size() - 1));
        }
        else if (m_activeTabIndex == index)
        {
            refreshActiveTab();
            emit activeTabChanged();
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
        m_activeTabIndex = -1;
        if (!m_currentGroupName.isEmpty())
        {
            m_currentGroupName.clear();
            emit currentGroupNameChanged();
        }
        m_workingTree.refresh({});
        m_diffViewer.clear();
        m_commitDiffViewer.clear();
        m_branchManager.setRepositoryPath({});
        m_stashManager.setRepoPath({});
        m_conflictResolver.setRepoPath({});
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
        m_stashManager.setRepoPath(path);
        m_conflictResolver.setRepoPath(path);
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
                                  emit rebaseFailed(QString::fromStdString(res.error().message));
                                  return;
                              }
                              if (path == activeRepoPath())
                              {
                                  refreshActiveTab();
                              }
                              if constexpr (requires { res->status; })
                              {
                                  if (reportOutcome)
                                  {
                                      if (res->status == gf::git::bo::RebaseStatus::Conflict)
                                      {
                                          emit rebaseConflictDetected();
                                      }
                                      else
                                      {
                                          emit rebaseSucceeded(QString::fromStdString(res->message));
                                      }
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
                }
            }
        }, Qt::QueuedConnection);
    }
} // namespace gf::desktop
