#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <filesystem>

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
        Q_PROPERTY(bool isBusy READ isBusy NOTIFY busyChanged)
        Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
        Q_PROPERTY(QString remoteUrl READ remoteUrl NOTIFY remoteUrlChanged)
        Q_PROPERTY(QString fileHistoryPath READ fileHistoryPath NOTIFY fileHistoryPathChanged)
        Q_PROPERTY(bool hasConflicts READ hasConflicts NOTIFY conflictsChanged)
        Q_PROPERTY(QStringList conflictedFiles READ conflictedFiles NOTIFY conflictsChanged)

        Q_PROPERTY(BranchListModel* branchList READ branchList CONSTANT)
        Q_PROPERTY(CommitHistoryListModel* commitHistory READ commitHistory CONSTANT)

    public:
        explicit BranchManagerViewModel(git::bl::GitBranchService& branchService,
                                        git::bl::GitHistoryService& historyService,
                                        git::bl::GitRemoteMergeService& remoteMergeService,
                                        git::bl::GitRebaseService* rebaseService = nullptr,
                                        QObject* parent = nullptr);
        ~BranchManagerViewModel() override = default;

        [[nodiscard]] QString currentBranch() const noexcept { return m_currentBranch; }
        [[nodiscard]] bool isDetached() const noexcept { return m_isDetached; }
        [[nodiscard]] bool isBusy() const noexcept { return m_isBusy; }
        [[nodiscard]] QString statusMessage() const noexcept { return m_statusMessage; }
        [[nodiscard]] QString remoteUrl() const noexcept { return m_remoteUrl; }
        [[nodiscard]] QString fileHistoryPath() const noexcept { return m_fileHistoryPath; }
        [[nodiscard]] bool hasConflicts() const noexcept { return !m_conflictedFiles.isEmpty(); }
        [[nodiscard]] QStringList conflictedFiles() const noexcept { return m_conflictedFiles; }

        [[nodiscard]] BranchListModel* branchList() noexcept { return &m_branchListModel; }
        [[nodiscard]] CommitHistoryListModel* commitHistory() noexcept { return &m_commitHistoryModel; }

    Q_INVOKABLE void setRepositoryPath(const QString& path);
    Q_INVOKABLE void refresh();

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

    Q_INVOKABLE bool mergeBranch(const QString& sourceBranch, bool noFf = false);
    Q_INVOKABLE bool mergeBranchInto(const QString& sourceBranch, const QString& targetBranch);
    Q_INVOKABLE bool rebaseOnto(const QString& targetBranch);
    Q_INVOKABLE bool rebaseBranch(const QString& sourceBranch, const QString& targetBranch);
    Q_INVOKABLE bool abortMerge();

    Q_INVOKABLE bool fetchRemote(const QString& remoteName = "origin", bool allRemotes = false);
    Q_INVOKABLE bool pullRemote(const QString& remoteName = "origin", const QString& strategy = "default");
    Q_INVOKABLE bool pushRemote(const QString& remoteName = "origin", bool setUpstream = false, bool force = false,
                                bool tags = false);
    Q_INVOKABLE bool setRemoteUrl(const QString& remoteName, const QString& newUrl);
    Q_INVOKABLE bool convertRemoteToSsh(const QString& remoteName = "origin");

    Q_INVOKABLE void copyToClipboard(const QString& text);
        [[nodiscard]] Q_INVOKABLE QString buildPullRequestUrl(const QString& sourceBranch, const QString& targetBranch,
                                                              const QString& title = QString(),
                                                              const QString& body = QString()) const;

    Q_INVOKABLE void setWipStatus(bool hasWip, int changedFilesCount = 0);
    Q_INVOKABLE QVariantList getCommitFiles(const QString& commitSha);

    Q_INVOKABLE bool loadFileHistory(const QString& filePath);
    Q_INVOKABLE void clearFileHistory();
    Q_INVOKABLE QStringList listRepositoryFiles();

        signals :

        void currentBranchChanged();
        void busyChanged();
        void statusMessageChanged();
        void remoteUrlChanged();
        void fileHistoryPathChanged();
        void conflictsChanged();
        void branchSwitched(const QString& newBranch);
        void operationCompleted(bool success, const QString& message);
        void commitReworded(const QString& oldSha, const QString& newSha);

    private:
        void setBusy(bool busy);
        void setStatusMessage(const QString& message);

        git::bl::GitBranchService& m_branchService;
        git::bl::GitHistoryService& m_historyService;
        git::bl::GitRemoteMergeService& m_remoteMergeService;
        git::bl::GitRebaseService* m_rebaseService = nullptr;

        std::filesystem::path m_repoPath;
        BranchListModel m_branchListModel;
        CommitHistoryListModel m_commitHistoryModel;

        QString m_currentBranch;
        QString m_fileHistoryPath;
        bool m_isDetached = false;
        bool m_isBusy = false;
        QString m_statusMessage;
        QString m_remoteUrl;
        QStringList m_conflictedFiles;
    };
} // namespace gf::desktop
