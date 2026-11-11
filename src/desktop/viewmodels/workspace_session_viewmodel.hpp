#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <memory>
#include <vector>

#include "desktop/services/serial_git_executor.hpp"
#include "desktop/viewmodels/branch_manager_viewmodel.hpp"
#include "desktop/viewmodels/commit_box_viewmodel.hpp"
#include "desktop/viewmodels/conflict_resolver_viewmodel.hpp"
#include "desktop/viewmodels/diff_viewmodel.hpp"
#include "desktop/viewmodels/stash_manager_viewmodel.hpp"
#include "desktop/viewmodels/working_tree_viewmodel.hpp"

namespace gf::repositories::bl
{
    class RepositoryService;
}

namespace gf::workspaces::bl
{
    class WorkspaceService;
}

namespace gf::git::bo
{
    struct FsDebouncedBatch;
}

namespace gf::git::bl
{
    class GitConflictService;
    class GitRepositoryWatcherService;
    class GitRebaseService;
    class GitStashService;
    class GitWorkingTreeService;
    class GitStagingService;
    class GitDiffService;
    class GitBranchService;
    class GitHistoryService;
    class GitRemoteMergeService;
}

namespace gf::desktop
{
    class WorkspaceSessionViewModel final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
        Q_PROPERTY(int activeTabIndex READ activeTabIndex WRITE setActiveTabIndex NOTIFY activeTabChanged)
        Q_PROPERTY(QString activeRepoId READ activeRepoId NOTIFY activeTabChanged)
        Q_PROPERTY(QString activeRepoName READ activeRepoName NOTIFY activeTabChanged)
        Q_PROPERTY(QString activeRepoPath READ activeRepoPath NOTIFY activeTabChanged)
        Q_PROPERTY(bool hasOpenTabs READ hasOpenTabs NOTIFY tabsChanged)
        Q_PROPERTY(QString currentGroupName READ currentGroupName NOTIFY currentGroupNameChanged)

        Q_PROPERTY(WorkingTreeViewModel* workingTree READ workingTree CONSTANT)
        Q_PROPERTY(CommitBoxViewModel* commitBox READ commitBox CONSTANT)
        Q_PROPERTY(DiffViewModel* diffViewer READ diffViewer CONSTANT)
        Q_PROPERTY(DiffViewModel* commitDiffViewer READ commitDiffViewer CONSTANT)
        Q_PROPERTY(BranchManagerViewModel* branchManager READ branchManager CONSTANT)
        Q_PROPERTY(StashManagerViewModel* stashManager READ stashManager CONSTANT)
        Q_PROPERTY(ConflictResolverViewModel* conflictResolver READ conflictResolver CONSTANT)
        Q_PROPERTY(bool isRebaseInProgress READ isRebaseInProgress NOTIFY rebaseStatusChanged)

    public:
        struct SessionTabItem
        {
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
                                           gf::git::bl::GitStashService& stashService,
                                           gf::git::bl::GitRebaseService& rebaseService,
                                           gf::git::bl::GitConflictService& conflictService,
                                           gf::git::bl::GitRepositoryWatcherService* watcherService = nullptr,
                                           QObject* parent = nullptr);
        ~WorkspaceSessionViewModel() override = default;

        [[nodiscard]] QVariantList tabs() const;
        [[nodiscard]] int activeTabIndex() const noexcept { return m_activeTabIndex; }
        void setActiveTabIndex(int index);

        [[nodiscard]] QString activeRepoId() const;
        [[nodiscard]] QString activeRepoName() const;
        [[nodiscard]] QString activeRepoPath() const;
        [[nodiscard]] bool hasOpenTabs() const noexcept { return !m_tabs.empty(); }
        [[nodiscard]] QString currentGroupName() const noexcept { return m_currentGroupName; }
        [[nodiscard]] bool isRebaseInProgress() const noexcept { return m_isRebaseInProgress; }

        [[nodiscard]] WorkingTreeViewModel* workingTree() noexcept { return &m_workingTree; }
        [[nodiscard]] CommitBoxViewModel* commitBox() noexcept { return &m_commitBox; }
        [[nodiscard]] DiffViewModel* diffViewer() noexcept { return &m_diffViewer; }
        [[nodiscard]] DiffViewModel* commitDiffViewer() noexcept { return &m_commitDiffViewer; }
        [[nodiscard]] BranchManagerViewModel* branchManager() noexcept { return &m_branchManager; }
        [[nodiscard]] StashManagerViewModel* stashManager() noexcept { return &m_stashManager; }
        [[nodiscard]] ConflictResolverViewModel* conflictResolver() noexcept { return &m_conflictResolver; }

    Q_INVOKABLE void openRepository(const QString& repoId, const QString& name, const QString& path);
    Q_INVOKABLE void openRepositories(const QStringList& repoIds);
    Q_INVOKABLE void openGroup(const QString& groupName, const QStringList& repoIds);
    Q_INVOKABLE void openWorkspace(const QString& workspaceId);
    Q_INVOKABLE void closeTab(int index);
    Q_INVOKABLE void closeAllTabs();
    Q_INVOKABLE void refreshActiveTab();
    Q_INVOKABLE void checkWorkingTreeUpdates();

        // Rebase
    Q_INVOKABLE bool startRebase(const QString& upstream, const QString& onto = {});
    Q_INVOKABLE bool continueRebase();
    Q_INVOKABLE bool skipRebase();
    Q_INVOKABLE bool abortRebase();
    Q_INVOKABLE void checkRebaseStatus();

        // Conflict Resolver
    Q_INVOKABLE bool openConflictResolver(const QString& relativePath);

        signals :

        void tabsChanged();
        void activeTabChanged();
        void currentGroupNameChanged();
        void rebaseStatusChanged();
        void rebaseSucceeded(const QString& message);
        void rebaseConflictDetected();

    private:
        void addTabInternal(const QString& repoId, const QString& name, const QString& path);
        void onCommitSucceeded(const QString& sha, const QString& summary);
        void onFsDebouncedBatch(const gf::git::bo::FsDebouncedBatch& batch);
        void onWorkingTreeRefreshed(const QString& repoPath);

        repositories::bl::RepositoryService& m_repoService;
        workspaces::bl::WorkspaceService& m_workspaceService;
        gf::git::bl::GitRebaseService& m_rebaseService;
        gf::git::bl::GitRepositoryWatcherService* m_watcherService{nullptr};

        WorkingTreeViewModel m_workingTree;
        CommitBoxViewModel m_commitBox;
        DiffViewModel m_diffViewer;
        DiffViewModel m_commitDiffViewer;
        BranchManagerViewModel m_branchManager;
        StashManagerViewModel m_stashManager;
        ConflictResolverViewModel m_conflictResolver;

        std::vector<SessionTabItem> m_tabs;
        int m_activeTabIndex = -1;
        bool m_isRebaseInProgress{false};
        QString m_currentGroupName;

        // HEAD/branch seen by the last working tree refresh of the active tab: a change means a
        // commit, checkout, reset... (from the app or an external git) and reloads the history.
        QString m_observedHeadOid;
        QString m_observedBranch;

        // Serial git queue shared by the working tree and the commit box (stage-then-commit
        // ordering). Declared last: destroyed first, waiting for its tasks while they are alive.
        SerialGitExecutor m_gitExecutor;
    };
} // namespace gf::desktop
