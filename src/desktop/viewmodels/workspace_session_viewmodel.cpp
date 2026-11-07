#include "desktop/viewmodels/workspace_session_viewmodel.hpp"

#include <QMetaObject>
#include <algorithm>

namespace gf::desktop {

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
    , m_workingTree(statusService, stagingService, this)
    , m_commitBox(stagingService, this)
    , m_diffViewer(diffService, this)
    , m_commitDiffViewer(diffService, this)
    , m_branchManager(branchService, historyService, remoteMergeService, this)
    , m_stashManager(stashService, this)
    , m_conflictResolver(conflictService, this) {

    connect(&m_commitBox, &CommitBoxViewModel::commitSucceeded,
            this, &WorkspaceSessionViewModel::onCommitSucceeded);

    connect(&m_branchManager, &BranchManagerViewModel::branchSwitched,
            this, [this](const QString&) { refreshActiveTab(); });

    connect(&m_conflictResolver, &ConflictResolverViewModel::resolutionApplied,
            this, [this](const QString&) { refreshActiveTab(); });

    if (m_watcherService) {
        m_watcherService->setBatchCallback([this](const gf::git::bo::FsDebouncedBatch& batch) {
            onFsDebouncedBatch(batch);
        });
    }
}

QVariantList WorkspaceSessionViewModel::tabs() const {
    QVariantList list;
    list.reserve(static_cast<qsizetype>(m_tabs.size()));
    for (const auto& item : m_tabs) {
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

void WorkspaceSessionViewModel::setActiveTabIndex(int index) {
    if (m_tabs.empty()) {
        m_activeTabIndex = -1;
        m_workingTree.refresh({});
        m_diffViewer.clear();
        m_branchManager.setRepositoryPath({});
        emit activeTabChanged();
        return;
    }

    int clamped = std::clamp(index, 0, static_cast<int>(m_tabs.size() - 1));
    if (m_activeTabIndex != clamped) {
        m_activeTabIndex = clamped;
        refreshActiveTab();
        emit activeTabChanged();
    }
}

QString WorkspaceSessionViewModel::activeRepoId() const {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        return m_tabs[static_cast<std::size_t>(m_activeTabIndex)].repoId;
    }
    return {};
}

QString WorkspaceSessionViewModel::activeRepoName() const {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        return m_tabs[static_cast<std::size_t>(m_activeTabIndex)].name;
    }
    return {};
}

QString WorkspaceSessionViewModel::activeRepoPath() const {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        return m_tabs[static_cast<std::size_t>(m_activeTabIndex)].path;
    }
    return {};
}

void WorkspaceSessionViewModel::openRepository(const QString& repoId,
                                              const QString& name,
                                              const QString& path) {
    // Look up display name or path if missing
    QString resolvedName = name;
    QString resolvedPath = path;

    if (resolvedName.isEmpty() && !repoId.isEmpty()) {
        auto repoRes = m_repoService.getRepository(repositories::bo::RepositoryId{repoId.toStdString()});
        if (repoRes && repoRes->has_value()) {
            resolvedName = QString::fromStdString((*repoRes)->displayName);
            if (resolvedPath.isEmpty()) {
                resolvedPath = QString::fromStdString((*repoRes)->localPath.string());
            }
        }
    }

    if (resolvedName.isEmpty() && !resolvedPath.isEmpty()) {
        resolvedName = QString::fromStdString(std::filesystem::path(resolvedPath.toStdString()).filename().string());
    }

    // Check if repository is already open in an existing tab
    for (std::size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].repoId == repoId || (!resolvedPath.isEmpty() && m_tabs[i].path == resolvedPath)) {
            setActiveTabIndex(static_cast<int>(i));
            return;
        }
    }

    SessionTabItem newItem{
        .repoId = repoId,
        .name = resolvedName.isEmpty() ? QStringLiteral("Repository") : resolvedName,
        .path = resolvedPath,
        .isDirty = false,
        .branch = {}
    };

    m_tabs.push_back(std::move(newItem));
    emit tabsChanged();

    if (m_watcherService && !resolvedPath.isEmpty()) {
        [[maybe_unused]] auto res = m_watcherService->startWatching(std::filesystem::path(resolvedPath.toStdString()));
    }

    setActiveTabIndex(static_cast<int>(m_tabs.size() - 1));
}

void WorkspaceSessionViewModel::openRepositories(const QStringList& repoIds) {
    for (const auto& id : repoIds) {
        auto repoRes = m_repoService.getRepository(repositories::bo::RepositoryId{id.toStdString()});
        if (repoRes && repoRes->has_value()) {
            openRepository(QString::fromStdString((*repoRes)->id.value()),
                           QString::fromStdString((*repoRes)->displayName),
                           QString::fromStdString((*repoRes)->localPath.string()));
        }
    }
}

void WorkspaceSessionViewModel::openGroup(const QString& groupName, const QStringList& repoIds) {
    closeAllTabs();
    m_currentGroupName = groupName;
    emit currentGroupNameChanged();
    openRepositories(repoIds);
}

void WorkspaceSessionViewModel::openWorkspace(const QString& workspaceId) {
    auto reposRes = m_workspaceService.resolveWorkspaceRepositories(
        workspaces::bo::WorkspaceId{workspaceId.toStdString()});
    if (!reposRes) {
        return;
    }

    for (const auto& repo : *reposRes) {
        openRepository(QString::fromStdString(repo.id.value()),
                       QString::fromStdString(repo.displayName),
                       QString::fromStdString(repo.localPath.string()));
    }
}

void WorkspaceSessionViewModel::closeTab(int index) {
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) {
        return;
    }

    QString closedPath = m_tabs[static_cast<std::size_t>(index)].path;
    if (m_watcherService && !closedPath.isEmpty()) {
        [[maybe_unused]] auto res = m_watcherService->stopWatching(std::filesystem::path(closedPath.toStdString()));
    }

    m_tabs.erase(m_tabs.begin() + index);
    emit tabsChanged();

    if (m_tabs.empty()) {
        if (!m_currentGroupName.isEmpty()) {
            m_currentGroupName.clear();
            emit currentGroupNameChanged();
        }
        m_activeTabIndex = -1;
        m_workingTree.refresh({});
        m_diffViewer.clear();
        m_branchManager.setRepositoryPath({});
        emit activeTabChanged();
    } else if (m_activeTabIndex >= static_cast<int>(m_tabs.size())) {
        setActiveTabIndex(static_cast<int>(m_tabs.size() - 1));
    } else if (m_activeTabIndex == index) {
        refreshActiveTab();
        emit activeTabChanged();
    }
}

void WorkspaceSessionViewModel::closeAllTabs() {
    if (m_watcherService) {
        for (const auto& tab : m_tabs) {
            if (!tab.path.isEmpty()) {
                [[maybe_unused]] auto res = m_watcherService->stopWatching(std::filesystem::path(tab.path.toStdString()));
            }
        }
    }

    m_tabs.clear();
    m_activeTabIndex = -1;
    if (!m_currentGroupName.isEmpty()) {
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

void WorkspaceSessionViewModel::refreshActiveTab() {
    QString path = activeRepoPath();
    if (path.isEmpty()) {
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

    m_workingTree.refresh(path);
    m_diffViewer.clear();
    m_commitDiffViewer.clear();
    m_branchManager.setRepositoryPath(path);
    m_stashManager.setRepoPath(path);
    m_conflictResolver.setRepoPath(path);
    checkRebaseStatus();

    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        m_tabs[static_cast<std::size_t>(m_activeTabIndex)].branch = m_workingTree.branchName();
        m_tabs[static_cast<std::size_t>(m_activeTabIndex)].isDirty = !m_workingTree.isClean();
        emit tabsChanged();
    }
}

void WorkspaceSessionViewModel::checkRebaseStatus() {
    QString path = activeRepoPath();
    if (path.isEmpty()) {
        if (m_isRebaseInProgress) {
            m_isRebaseInProgress = false;
            emit rebaseStatusChanged();
        }
        return;
    }

    auto inProg = m_rebaseService.isRebaseInProgress(std::filesystem::path(path.toStdString()));
    bool status = inProg ? *inProg : false;
    if (m_isRebaseInProgress != status) {
        m_isRebaseInProgress = status;
        emit rebaseStatusChanged();
    }
}

bool WorkspaceSessionViewModel::startRebase(const QString& upstream, const QString& onto) {
    QString path = activeRepoPath();
    if (path.isEmpty() || upstream.trimmed().isEmpty()) {
        return false;
    }

    gf::git::bo::RebaseOptions opts{
        .upstream = upstream.trimmed().toStdString()
    };
    if (!onto.trimmed().isEmpty()) {
        opts.onto = onto.trimmed().toStdString();
    }

    auto res = m_rebaseService.startRebase(std::filesystem::path(path.toStdString()), opts);
    if (!res) {
        return false;
    }

    refreshActiveTab();
    if (res->status == gf::git::bo::RebaseStatus::Conflict) {
        emit rebaseConflictDetected();
    } else {
        emit rebaseSucceeded(QString::fromStdString(res->message));
    }
    return true;
}

bool WorkspaceSessionViewModel::continueRebase() {
    QString path = activeRepoPath();
    if (path.isEmpty()) {
        return false;
    }

    auto res = m_rebaseService.continueRebase(std::filesystem::path(path.toStdString()));
    if (!res) {
        return false;
    }

    refreshActiveTab();
    if (res->status == gf::git::bo::RebaseStatus::Conflict) {
        emit rebaseConflictDetected();
    } else {
        emit rebaseSucceeded(QString::fromStdString(res->message));
    }
    return true;
}

bool WorkspaceSessionViewModel::skipRebase() {
    QString path = activeRepoPath();
    if (path.isEmpty()) {
        return false;
    }

    auto res = m_rebaseService.skipRebase(std::filesystem::path(path.toStdString()));
    if (!res) {
        return false;
    }

    refreshActiveTab();
    if (res->status == gf::git::bo::RebaseStatus::Conflict) {
        emit rebaseConflictDetected();
    } else {
        emit rebaseSucceeded(QString::fromStdString(res->message));
    }
    return true;
}

bool WorkspaceSessionViewModel::abortRebase() {
    QString path = activeRepoPath();
    if (path.isEmpty()) {
        return false;
    }

    auto res = m_rebaseService.abortRebase(std::filesystem::path(path.toStdString()));
    if (!res) {
        return false;
    }

    refreshActiveTab();
    return true;
}

bool WorkspaceSessionViewModel::openConflictResolver(const QString& relativePath) {
    QString path = activeRepoPath();
    if (path.isEmpty() || relativePath.isEmpty()) {
        return false;
    }

    m_conflictResolver.setRepoPath(path);
    return m_conflictResolver.loadFile(relativePath);
}

void WorkspaceSessionViewModel::onCommitSucceeded(const QString& /*sha*/, const QString& /*summary*/) {
    refreshActiveTab();
}

void WorkspaceSessionViewModel::onFsDebouncedBatch(const gf::git::bo::FsDebouncedBatch& batch) {
    QMetaObject::invokeMethod(this, [this, batch]() {
        QString currentActive = activeRepoPath();
        if (currentActive.isEmpty()) {
            return;
        }

        if (std::filesystem::path(currentActive.toStdString()) == batch.repositoryPath) {
            if (batch.requiresWorkingTreeRefresh()) {
                m_workingTree.refresh(currentActive);
                checkRebaseStatus();
            }
            if (batch.requiresBranchRefresh()) {
                m_branchManager.setRepositoryPath(currentActive);
                if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
                    m_tabs[static_cast<std::size_t>(m_activeTabIndex)].branch = m_workingTree.branchName();
                    m_tabs[static_cast<std::size_t>(m_activeTabIndex)].isDirty = !m_workingTree.isClean();
                    emit tabsChanged();
                }
            }
        }
    }, Qt::QueuedConnection);
}

} // namespace gf::desktop
