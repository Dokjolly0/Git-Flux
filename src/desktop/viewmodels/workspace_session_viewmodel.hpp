#pragma once

#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"
#include "desktop/viewmodels/branch_manager_viewmodel.hpp"
#include "desktop/viewmodels/commit_box_viewmodel.hpp"
#include "desktop/viewmodels/diff_viewmodel.hpp"
#include "desktop/viewmodels/working_tree_viewmodel.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <memory>
#include <vector>

namespace gf::desktop {

class WorkspaceSessionViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(int activeTabIndex READ activeTabIndex WRITE setActiveTabIndex NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeRepoId READ activeRepoId NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeRepoName READ activeRepoName NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeRepoPath READ activeRepoPath NOTIFY activeTabChanged)
    Q_PROPERTY(bool hasOpenTabs READ hasOpenTabs NOTIFY tabsChanged)

    Q_PROPERTY(WorkingTreeViewModel* workingTree READ workingTree CONSTANT)
    Q_PROPERTY(CommitBoxViewModel* commitBox READ commitBox CONSTANT)
    Q_PROPERTY(DiffViewModel* diffViewer READ diffViewer CONSTANT)
    Q_PROPERTY(BranchManagerViewModel* branchManager READ branchManager CONSTANT)

public:
    struct SessionTabItem {
        QString repoId;
        QString name;
        QString path;
        bool isDirty = false;
        QString branch;
    };

    explicit WorkspaceSessionViewModel(repositories::bl::RepositoryService& repoService,
                                       workspaces::bl::WorkspaceService& workspaceService,
                                       gf::git::bl::GitWorkingTreeService& statusService,
                                       gf::git::bl::GitStagingService& stagingService,
                                       gf::git::bl::GitDiffService& diffService,
                                       gf::git::bl::GitBranchService& branchService,
                                       gf::git::bl::GitHistoryService& historyService,
                                       gf::git::bl::GitRemoteMergeService& remoteMergeService,
                                       QObject* parent = nullptr);
    ~WorkspaceSessionViewModel() override = default;

    [[nodiscard]] QVariantList tabs() const;
    [[nodiscard]] int activeTabIndex() const noexcept { return m_activeTabIndex; }
    void setActiveTabIndex(int index);

    [[nodiscard]] QString activeRepoId() const;
    [[nodiscard]] QString activeRepoName() const;
    [[nodiscard]] QString activeRepoPath() const;
    [[nodiscard]] bool hasOpenTabs() const noexcept { return !m_tabs.empty(); }

    [[nodiscard]] WorkingTreeViewModel* workingTree() noexcept { return &m_workingTree; }
    [[nodiscard]] CommitBoxViewModel* commitBox() noexcept { return &m_commitBox; }
    [[nodiscard]] DiffViewModel* diffViewer() noexcept { return &m_diffViewer; }
    [[nodiscard]] BranchManagerViewModel* branchManager() noexcept { return &m_branchManager; }

    Q_INVOKABLE void openRepository(const QString& repoId, const QString& name, const QString& path);
    Q_INVOKABLE void openRepositories(const QStringList& repoIds);
    Q_INVOKABLE void openWorkspace(const QString& workspaceId);
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void closeAllTabs();
    Q_INVOKABLE void refreshActiveTab();

signals:
    void tabsChanged();
    void activeTabChanged();

private:
    void onCommitSucceeded(const QString& sha, const QString& summary);

    repositories::bl::RepositoryService& m_repoService;
    workspaces::bl::WorkspaceService& m_workspaceService;

    WorkingTreeViewModel m_workingTree;
    CommitBoxViewModel m_commitBox;
    DiffViewModel m_diffViewer;
    BranchManagerViewModel m_branchManager;

    std::vector<SessionTabItem> m_tabs;
    int m_activeTabIndex = -1;
};

} // namespace gf::desktop
