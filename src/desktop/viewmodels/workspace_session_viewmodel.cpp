#include "desktop/viewmodels/workspace_session_viewmodel.hpp"

#include <algorithm>

namespace gf::desktop {

WorkspaceSessionViewModel::WorkspaceSessionViewModel(repositories::bl::RepositoryService& repoService,
                                                     workspaces::bl::WorkspaceService& workspaceService,
                                                     gf::git::bl::GitWorkingTreeService& statusService,
                                                     gf::git::bl::GitStagingService& stagingService,
                                                     gf::git::bl::GitDiffService& diffService,
                                                     QObject* parent)
    : QObject(parent)
    , m_repoService(repoService)
    , m_workspaceService(workspaceService)
    , m_workingTree(statusService, stagingService, this)
    , m_commitBox(stagingService, this)
    , m_diffViewer(diffService, this) {

    connect(&m_commitBox, &CommitBoxViewModel::commitSucceeded,
            this, &WorkspaceSessionViewModel::onCommitSucceeded);
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
    // Se il path non è fornito, cerchiamo nel service dei repository
    QString resolvedName = name;
    QString resolvedPath = path;

    if (resolvedPath.isEmpty() && !repoId.isEmpty()) {
        auto repoRes = m_repoService.getRepository(repositories::bo::RepositoryId{repoId.toStdString()});
        if (repoRes && repoRes->has_value()) {
            resolvedName = QString::fromStdString((*repoRes)->displayName);
            resolvedPath = QString::fromStdString((*repoRes)->localPath.string());
        }
    }

    // Controlla se il repository è già aperto in un tab
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

    m_tabs.erase(m_tabs.begin() + index);
    emit tabsChanged();

    if (m_tabs.empty()) {
        m_activeTabIndex = -1;
        m_workingTree.refresh({});
        m_diffViewer.clear();
        emit activeTabChanged();
    } else if (m_activeTabIndex >= static_cast<int>(m_tabs.size())) {
        setActiveTabIndex(static_cast<int>(m_tabs.size() - 1));
    } else if (m_activeTabIndex == index) {
        refreshActiveTab();
        emit activeTabChanged();
    }
}

void WorkspaceSessionViewModel::closeAllTabs() {
    m_tabs.clear();
    m_activeTabIndex = -1;
    m_workingTree.refresh({});
    m_diffViewer.clear();
    emit tabsChanged();
    emit activeTabChanged();
}

void WorkspaceSessionViewModel::refreshActiveTab() {
    QString path = activeRepoPath();
    if (path.isEmpty()) {
        m_workingTree.refresh({});
        m_diffViewer.clear();
        return;
    }

    m_workingTree.refresh(path);
    m_diffViewer.clear();

    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        m_tabs[static_cast<std::size_t>(m_activeTabIndex)].branch = m_workingTree.branchName();
        m_tabs[static_cast<std::size_t>(m_activeTabIndex)].isDirty = !m_workingTree.isClean();
        emit tabsChanged();
    }
}

void WorkspaceSessionViewModel::onCommitSucceeded(const QString& /*sha*/, const QString& /*summary*/) {
    refreshActiveTab();
}

} // namespace gf::desktop
