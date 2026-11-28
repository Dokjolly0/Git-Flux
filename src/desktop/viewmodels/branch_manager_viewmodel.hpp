#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>
#include <filesystem>
#include <functional>
#include <string>

#include "core/modules/git/bo/https_credential_models.hpp"
#include "core/modules/git/bo/remote_merge_models.hpp"
#include "desktop/services/serial_git_executor.hpp"
#include "desktop/viewmodels/branch_list_model.hpp"
#include "desktop/viewmodels/commit_history_list_model.hpp"

namespace gf::git::bl
{
    class GitBranchService;
    class GitHistoryService;
    class GitRebaseService;
    class GitRemoteMergeService;
}

namespace gf::desktop
{

    class BranchManagerViewModel final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(QString currentBranch READ currentBranch NOTIFY currentBranchChanged)
        Q_PROPERTY(bool isDetached READ isDetached NOTIFY currentBranchChanged)
        // Network operation in progress: "fetch", "pull", "push" or empty (isBusy also covers
        // local operations and background reloads).
        Q_PROPERTY(QString remoteOperation READ remoteOperation NOTIFY remoteOperationChanged)
        Q_PROPERTY(bool isBusy READ isBusy NOTIFY busyChanged)
        Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
        Q_PROPERTY(QString remoteUrl READ remoteUrl NOTIFY remoteUrlChanged)
        Q_PROPERTY(QString fileHistoryPath READ fileHistoryPath NOTIFY fileHistoryPathChanged)
        Q_PROPERTY(bool hasConflicts READ hasConflicts NOTIFY conflictsChanged)
        Q_PROPERTY(QStringList conflictedFiles READ conflictedFiles NOTIFY conflictsChanged)
        // Id of the operation started by the last call (read right after calling it; matched by
        // operationFinished). 0 when no operation was ever started.
        Q_PROPERTY(int lastOperationId READ lastOperationId)

        Q_PROPERTY(BranchListModel* branchList READ branchList CONSTANT)
        Q_PROPERTY(CommitHistoryListModel* commitHistory READ commitHistory CONSTANT)

        // Multi-commit selection state & scope flag (ready for future Settings persistence)
        Q_PROPERTY(bool allowCrossBranchMultiSelect READ allowCrossBranchMultiSelect WRITE setAllowCrossBranchMultiSelect NOTIFY multiSelectSettingsChanged)
        Q_PROPERTY(bool multiSelectBranchFilterActive READ multiSelectBranchFilterActive WRITE setMultiSelectBranchFilterActive NOTIFY multiSelectChanged)
        Q_PROPERTY(QStringList selectedCommitShas READ selectedCommitShas NOTIFY multiSelectChanged)
        Q_PROPERTY(QStringList effectiveSelectedCommitShas READ effectiveSelectedCommitShas NOTIFY multiSelectChanged)
        Q_PROPERTY(bool isMultiSelectActive READ isMultiSelectActive NOTIFY multiSelectChanged)
        Q_PROPERTY(bool hasRawMultiSelection READ hasRawMultiSelection NOTIFY multiSelectChanged)
        Q_PROPERTY(int selectedCommitsCount READ selectedCommitsCount NOTIFY multiSelectChanged)
        Q_PROPERTY(int effectiveSelectedCommitsCount READ effectiveSelectedCommitsCount NOTIFY multiSelectChanged)
        Q_PROPERTY(QString multiSelectAnchorSha READ multiSelectAnchorSha NOTIFY multiSelectChanged)
        Q_PROPERTY(QString multiSelectReferenceBranch READ multiSelectReferenceBranch NOTIFY multiSelectChanged)
        Q_PROPERTY(bool isMultiSelectContiguousRange READ isMultiSelectContiguousRange NOTIFY multiSelectChanged)
        Q_PROPERTY(QString multiSelectRangeBase READ multiSelectRangeBase NOTIFY multiSelectChanged)
        Q_PROPERTY(QString multiSelectRangeHead READ multiSelectRangeHead NOTIFY multiSelectChanged)
        Q_PROPERTY(QString multiSelectViewMode READ multiSelectViewMode WRITE setMultiSelectViewMode NOTIFY multiSelectViewModeChanged)
        Q_PROPERTY(bool isLoadingMultiCommitFiles READ isLoadingMultiCommitFiles NOTIFY multiSelectLoadingChanged)
        Q_PROPERTY(int multiSelectRevision READ multiSelectRevision NOTIFY multiSelectChanged)

    public:
        /// Standalone: owns a private executor.
        explicit BranchManagerViewModel(git::bl::GitBranchService& branchService,
                                        git::bl::GitHistoryService& historyService,
                                        git::bl::GitRemoteMergeService& remoteMergeService,
                                        git::bl::GitRebaseService* rebaseService = nullptr,
                                        QObject* parent = nullptr);
        /// Shares `executor` (which must be destroyed before this viewmodel) with the other
        /// viewmodels of the session, so every git operation on the repository is ordered.
        BranchManagerViewModel(git::bl::GitBranchService& branchService,
                               git::bl::GitHistoryService& historyService,
                               git::bl::GitRemoteMergeService& remoteMergeService,
                               git::bl::GitRebaseService* rebaseService,
                               SerialGitExecutor& executor,
                               QObject* parent = nullptr);
        ~BranchManagerViewModel() override;

        /// Spins the event loop until no background git work (refresh, checkout) is queued or
        /// running. Intended for tests and shutdown paths; returns false on timeout.
        bool waitForIdle(int timeoutMs = 10000);

        [[nodiscard]] QString currentBranch() const noexcept { return m_currentBranch; }
        [[nodiscard]] bool isDetached() const noexcept { return m_isDetached; }
        [[nodiscard]] bool isBusy() const noexcept { return m_isBusy; }
        [[nodiscard]] QString statusMessage() const noexcept { return m_statusMessage; }
        [[nodiscard]] QString remoteOperation() const noexcept { return m_remoteOperation; }
        [[nodiscard]] QString remoteUrl() const noexcept { return m_remoteUrl; }

        /// Resolves the saved provider token for an HTTPS remote URL (GUI thread): returns a
        /// loader run on the git worker thread, or an empty function when there is none.
        using HttpsCredentialResolver = std::function<git::bo::HttpsCredentialLoader(const QString& remoteUrl)>;
        void setHttpsCredentialResolver(HttpsCredentialResolver resolver)
        {
            m_httpsCredentialResolver = std::move(resolver);
        }
        [[nodiscard]] QString fileHistoryPath() const noexcept { return m_fileHistoryPath; }
        [[nodiscard]] bool hasConflicts() const noexcept { return !m_conflictedFiles.isEmpty(); }
        [[nodiscard]] QStringList conflictedFiles() const noexcept { return m_conflictedFiles; }
        [[nodiscard]] int lastOperationId() const noexcept { return m_lastOperationId; }

        [[nodiscard]] BranchListModel* branchList() noexcept { return &m_branchListModel; }
        [[nodiscard]] CommitHistoryListModel* commitHistory() noexcept { return &m_commitHistoryModel; }

        [[nodiscard]] bool allowCrossBranchMultiSelect() const noexcept { return m_allowCrossBranchMultiSelect; }
        Q_INVOKABLE void setAllowCrossBranchMultiSelect(bool allow);
        [[nodiscard]] bool multiSelectBranchFilterActive() const noexcept
        {
            return !m_allowCrossBranchMultiSelect || m_multiSelectBranchFilterActive;
        }
        Q_INVOKABLE void setMultiSelectBranchFilterActive(bool active);
        [[nodiscard]] QStringList selectedCommitShas() const noexcept { return m_selectedCommitShas; }
        [[nodiscard]] QStringList effectiveSelectedCommitShas() const noexcept { return m_effectiveSelectedShas; }
        [[nodiscard]] bool isMultiSelectActive() const noexcept { return m_effectiveSelectedShas.size() > 1; }
        [[nodiscard]] bool hasRawMultiSelection() const noexcept { return m_selectedCommitShas.size() > 1; }
        [[nodiscard]] int selectedCommitsCount() const noexcept { return static_cast<int>(m_selectedCommitShas.size()); }
        [[nodiscard]] int effectiveSelectedCommitsCount() const noexcept { return static_cast<int>(m_effectiveSelectedShas.size()); }
        [[nodiscard]] QString multiSelectAnchorSha() const noexcept { return m_multiSelectAnchorSha; }
        [[nodiscard]] QString multiSelectReferenceBranch() const noexcept { return m_multiSelectReferenceBranch; }
        [[nodiscard]] bool isMultiSelectContiguousRange() const noexcept { return m_isMultiSelectContiguousRange; }
        [[nodiscard]] QString multiSelectRangeBase() const noexcept { return m_multiSelectRangeBase; }
        [[nodiscard]] QString multiSelectRangeHead() const noexcept { return m_multiSelectRangeHead; }
        [[nodiscard]] QString multiSelectViewMode() const noexcept { return m_multiSelectViewMode; }
        void setMultiSelectViewMode(const QString& mode);
        [[nodiscard]] bool isLoadingMultiCommitFiles() const noexcept { return m_isLoadingMultiCommitFiles; }
        [[nodiscard]] int multiSelectRevision() const noexcept { return m_multiSelectRevision; }

        void setRepositoryPath(const QString& path);
    /// Requests a reload of branches, history and remotes. Requests are coalesced and the git
    /// reads run on a background thread; models are updated on the GUI thread when done.
    Q_INVOKABLE void refresh();

    /// Starts the checkout on the background thread (returns false only for invalid input);
    /// the outcome is reported through branchSwitched / operationCompleted.
    Q_INVOKABLE bool checkoutBranch(const QString& branchName);
    Q_INVOKABLE bool checkoutCommit(const QString& commitSha);
    Q_INVOKABLE bool createBranch(const QString& branchName, bool checkoutAfterCreate = true,
                                  const QString& startPoint = QString());
    Q_INVOKABLE bool deleteBranch(const QString& branchName, bool force = false);
    Q_INVOKABLE bool renameBranch(const QString& oldName, const QString& newName);

    Q_INVOKABLE bool resetCommit(const QString& commitSha, const QString& mode = QStringLiteral("mixed"));
    Q_INVOKABLE bool revertCommit(const QString& commitSha);
    Q_INVOKABLE bool rewordCommit(const QString& commitSha, const QString& newMessage);
    Q_INVOKABLE bool cherryPickCommit(const QString& commitSha);
    Q_INVOKABLE bool createTag(const QString& tagName, const QString& targetCommit = QString(),
                               const QString& message = QString());
    Q_INVOKABLE bool deleteTag(const QString& tagName);
    Q_INVOKABLE bool createWorktree(const QString& targetPath, const QString& commitOrBranch = QString());

    Q_INVOKABLE bool mergeBranch(const QString& sourceBranch, bool noFf = true);
    Q_INVOKABLE bool mergeBranchInto(const QString& sourceBranch, const QString& targetBranch, bool noFf = true);
    /// Asynchronous: the answer arrives with fastForwardCheckReady(target, source, possible).
    /// A newer check supersedes a pending one.
    Q_INVOKABLE void requestFastForwardCheck(const QString& targetBranch, const QString& sourceBranch);
    Q_INVOKABLE bool fastForwardBranch(const QString& targetBranch, const QString& sourceBranch);
    Q_INVOKABLE bool rebaseOnto(const QString& targetBranch);
    Q_INVOKABLE bool rebaseBranch(const QString& sourceBranch, const QString& targetBranch);
    Q_INVOKABLE bool abortMerge();

    Q_INVOKABLE bool fetchRemote(const QString& remoteName = "origin", bool allRemotes = false);
    Q_INVOKABLE bool pullRemote(const QString& remoteName = "origin", const QString& strategy = "default");
    Q_INVOKABLE bool pushRemote(const QString& remoteName = "origin", bool setUpstream = false, bool force = false,
                                bool tags = false, bool forceUnconditional = false);
    Q_INVOKABLE bool setRemoteUrl(const QString& remoteName, const QString& newUrl);
    Q_INVOKABLE bool convertRemoteToSsh(const QString& remoteName = "origin");

    Q_INVOKABLE void copyToClipboard(const QString& text);
        [[nodiscard]] Q_INVOKABLE QString buildPullRequestUrl(const QString& sourceBranch, const QString& targetBranch,
                                                              const QString& title = QString(),
                                                              const QString& body = QString()) const;

        void setWipStatus(bool hasWip, int changedFilesCount = 0);
    /// Asynchronous, latest wins: the files arrive with commitFilesLoaded(commitSha, files).
    Q_INVOKABLE void requestCommitFiles(const QString& commitSha);

    // Multi-commit selection operations
    Q_INVOKABLE void selectSingleCommit(int rowIndex);
    Q_INVOKABLE void setSelectionAnchorSha(const QString& sha);
    Q_INVOKABLE bool selectCommitRangeTo(int targetIndex);
    Q_INVOKABLE bool toggleCommitSelectionAt(int rowIndex);
    Q_INVOKABLE void clearMultiSelection();
    [[nodiscard]] Q_INVOKABLE bool isShaSelected(const QString& sha) const;
    [[nodiscard]] Q_INVOKABLE bool isShaEffectivelySelected(const QString& sha) const;

    /// Asynchronous: the filtered history is applied (and fileHistoryPathChanged emitted) when
    /// `git log --follow` completes; a newer request or clearFileHistory() supersedes it.
    Q_INVOKABLE bool loadFileHistory(const QString& filePath);
    Q_INVOKABLE void clearFileHistory();
    /// Asynchronous: the tracked files arrive with repositoryFilesLoaded(files).
    Q_INVOKABLE void requestRepositoryFiles();

        signals :

        void currentBranchChanged();
        void busyChanged();
        void remoteOperationChanged();
        void statusMessageChanged();
        void remoteUrlChanged();
        void fileHistoryPathChanged();
        void conflictsChanged();
        void branchSwitched(const QString& newBranch);
        void operationCompleted(bool success, const QString& message);
        // Same outcome, tagged with the id returned by lastOperationId when the operation was
        // started: lets a dialog react only to its own operation.
        void operationFinished(int operationId, bool success, const QString& message);
        void commitReworded(const QString& oldSha, const QString& newSha);
        // The history view should select the HEAD commit: HEAD moved to another commit of the same
        // repository (commit, external git command...), or an operation that targets the current
        // commit completed (checkout, pull, merge, reset, rebase...) even if HEAD did not move.
        // Emitted after the commit history has been reloaded, so the commit is in the model.
        void followHeadRequested(const QString& headSha);
        void commitFilesLoaded(const QString& commitSha, const QVariantList& files);
        void multiCommitFilesLoaded(const QVariantMap& summary);
        void multiSelectChanged();
        void multiSelectSettingsChanged();
        void multiSelectViewModeChanged();
        void multiSelectLoadingChanged();
        void repositoryFilesLoaded(const QStringList& files);
        void fastForwardCheckReady(const QString& targetBranch, const QString& sourceBranch, bool possible);
        // Emitted when a push is rejected because history diverged or remote refs conflict and
        // a force push can resolve it; the UI confirms with the user before retrying with force.
        void forcePushConfirmationRequested(const QString& remoteName,
                                            const QString& branchName,
                                            bool setUpstream,
                                            bool tags,
                                            bool requiresUnconditionalForce,
                                            const QString& reasonSummary,
                                            const QString& details);

    private:
        struct RefreshSnapshot;

        void startBackgroundRefresh();
        [[nodiscard]] RefreshSnapshot loadSnapshot(const std::filesystem::path& repoPath,
                                                   const std::string& fileHistoryPath) const;
        void applySnapshot(RefreshSnapshot& snapshot);
        void updateBusy();

        // Runs `work` (git calls only, on the shared executor) and then `finish(id, sameRepo,
        // result)` on the GUI thread. Shows `startMessage`, keeps isBusy set meanwhile and returns
        // the operation id (also exposed as lastOperationId). `sameRepo` is false when the view
        // moved to another repository meanwhile: report the outcome but leave the state alone.
        template <typename Work, typename Finish>
        int runOperation(const QString& startMessage, Work work, Finish finish);
        // Reports an operation outcome: status bar, operationCompleted and operationFinished.
        void completeOperation(int operationId, bool success, const QString& message,
                               const QString& statusMessage = QString());
        void handleMergeOutcome(int id, bool sameRepo, bool ok, const QString& errorMessage,
                                bool hasConflict, const std::vector<std::string>& conflictedFiles,
                                const QString& sourceBranch);
        void handleRebaseOutcome(int id, bool sameRepo, bool ok, const QString& errorMessage,
                                 bool hasConflict, const std::vector<std::string>& conflictedFiles,
                                 const QString& okMessage, const QString& switchedBranch);
        void setConflicts(const std::vector<std::string>& files);
        void setStatusMessage(const QString& message);
        void recomputeMultiSelectionState(bool triggerReload);
        void resetMultiSelectionState();
        void requestMultiCommitFiles();
        // Marks operation `id` as the running network operation `kind` (see remoteOperation).
        void beginRemoteOperation(int id, const QString& kind);
        void endRemoteOperation(int id);
        // Credential loader for `remoteName` (push URL when `forPush`), empty if none applies.
        [[nodiscard]] git::bo::HttpsCredentialLoader httpsCredentialFor(const QString& remoteName, bool forPush) const;

        git::bl::GitBranchService& m_branchService;
        git::bl::GitHistoryService& m_historyService;
        git::bl::GitRemoteMergeService& m_remoteMergeService;
        git::bl::GitRebaseService* m_rebaseService = nullptr;

        std::filesystem::path m_repoPath;
        BranchListModel m_branchListModel;
        CommitHistoryListModel m_commitHistoryModel;

        QString m_currentBranch;
        QString m_headSha;                 // HEAD commit of the last applied snapshot (this repository)
        bool m_followHeadPending = false;  // an operation asked to select HEAD after the next reload
        QString m_fileHistoryPath;
        bool m_isDetached = false;
        bool m_isBusy = false;
        QString m_statusMessage;
        QString m_remoteUrl;
        std::vector<git::bo::GitRemote> m_remotes; // last loaded `git remote -v`
        HttpsCredentialResolver m_httpsCredentialResolver;
        QStringList m_conflictedFiles;
        bool m_refreshScheduled = false;   // a coalesced refresh is queued on the event loop
        bool m_refreshRunning = false;     // a background reload is in flight
        bool m_refreshAgain = false;       // another reload was requested while one was running
        int m_pendingOperations = 0;       // background operations not yet completed
        int m_lastOperationId = 0;
        QString m_remoteOperation;
        int m_remoteOperationId = 0;
        quint64 m_generation = 0;          // bumped when repo/filter changes: stale results are dropped

        LatestRequest m_commitFilesRequest;
        LatestRequest m_multiCommitFilesRequest;
        LatestRequest m_fileHistoryRequest;
        LatestRequest m_repositoryFilesRequest;
        LatestRequest m_fastForwardRequest;

        bool m_allowCrossBranchMultiSelect = false; // Default: branch-only (--first-parent)
        bool m_multiSelectBranchFilterActive = true;
        QStringList m_selectedCommitShas;
        QStringList m_effectiveSelectedShas;
        QSet<QString> m_selectedShaSet;
        QSet<QString> m_effectiveShaSet;
        QString m_multiSelectAnchorSha;
        QString m_multiSelectReferenceBranch;
        bool m_isMultiSelectContiguousRange = false;
        QString m_multiSelectRangeBase;
        QString m_multiSelectRangeHead;
        QString m_multiSelectViewMode = QStringLiteral("total");
        bool m_isLoadingMultiCommitFiles = false;
        int m_multiSelectRevision = 0;

        SerialGitExecutor* m_executor = nullptr;
        // Declared last: destroyed first, waiting for their tasks while this object is alive.
        std::unique_ptr<SerialGitExecutor> m_ownedExecutor;
        // Selection reads (commit files, file history, file list) run on their own lane, so
        // they never wait behind a long mutation or reload of the session queue.
        std::unique_ptr<SerialGitExecutor> m_readExecutor;
    };
} // namespace gf::desktop
