#pragma once

#include "core/modules/git/bl/git_branch_service.hpp"
#include "core/modules/git/bl/git_history_service.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "desktop/viewmodels/branch_list_model.hpp"
#include "desktop/viewmodels/commit_history_list_model.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <filesystem>

namespace gf::desktop {

class BranchManagerViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString currentBranch READ currentBranch NOTIFY currentBranchChanged)
    Q_PROPERTY(bool isDetached READ isDetached NOTIFY currentBranchChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString remoteUrl READ remoteUrl NOTIFY remoteUrlChanged)
    Q_PROPERTY(bool hasConflicts READ hasConflicts NOTIFY conflictsChanged)
    Q_PROPERTY(QStringList conflictedFiles READ conflictedFiles NOTIFY conflictsChanged)

    Q_PROPERTY(BranchListModel* branchList READ branchList CONSTANT)
    Q_PROPERTY(CommitHistoryListModel* commitHistory READ commitHistory CONSTANT)

public:
    explicit BranchManagerViewModel(git::bl::GitBranchService& branchService,
                                   git::bl::GitHistoryService& historyService,
                                   git::bl::GitRemoteMergeService& remoteMergeService,
                                   QObject* parent = nullptr);
    ~BranchManagerViewModel() override = default;

    [[nodiscard]] QString currentBranch() const noexcept { return m_currentBranch; }
    [[nodiscard]] bool isDetached() const noexcept { return m_isDetached; }
    [[nodiscard]] bool isBusy() const noexcept { return m_isBusy; }
    [[nodiscard]] QString statusMessage() const noexcept { return m_statusMessage; }
    [[nodiscard]] QString remoteUrl() const noexcept { return m_remoteUrl; }
    [[nodiscard]] bool hasConflicts() const noexcept { return !m_conflictedFiles.isEmpty(); }
    [[nodiscard]] QStringList conflictedFiles() const noexcept { return m_conflictedFiles; }

    [[nodiscard]] BranchListModel* branchList() noexcept { return &m_branchListModel; }
    [[nodiscard]] CommitHistoryListModel* commitHistory() noexcept { return &m_commitHistoryModel; }

    Q_INVOKABLE void setRepositoryPath(const QString& path);
    Q_INVOKABLE void refresh();

    Q_INVOKABLE bool checkoutBranch(const QString& branchName);
    Q_INVOKABLE bool createBranch(const QString& branchName, bool checkoutAfterCreate = true, const QString& startPoint = QString());
    Q_INVOKABLE bool deleteBranch(const QString& branchName, bool force = false);
    Q_INVOKABLE bool renameBranch(const QString& oldName, const QString& newName);

    Q_INVOKABLE bool mergeBranch(const QString& sourceBranch, bool noFf = false);
    Q_INVOKABLE bool abortMerge();

    Q_INVOKABLE bool fetchRemote(const QString& remoteName = "origin", bool allRemotes = false);
    Q_INVOKABLE bool pullRemote(const QString& remoteName = "origin", const QString& strategy = "default");
    Q_INVOKABLE bool pushRemote(const QString& remoteName = "origin", bool setUpstream = false, bool force = false, bool tags = false);
    Q_INVOKABLE bool setRemoteUrl(const QString& remoteName, const QString& newUrl);
    Q_INVOKABLE bool convertRemoteToSsh(const QString& remoteName = "origin");

    Q_INVOKABLE void setWipStatus(bool hasWip, int changedFilesCount = 0);
    Q_INVOKABLE QVariantList getCommitFiles(const QString& commitSha);

signals:
    void currentBranchChanged();
    void busyChanged();
    void statusMessageChanged();
    void remoteUrlChanged();
    void conflictsChanged();
    void branchSwitched(const QString& newBranch);
    void operationCompleted(bool success, const QString& message);

private:
    void setBusy(bool busy);
    void setStatusMessage(const QString& message);

    git::bl::GitBranchService& m_branchService;
    git::bl::GitHistoryService& m_historyService;
    git::bl::GitRemoteMergeService& m_remoteMergeService;

    std::filesystem::path m_repoPath;
    BranchListModel m_branchListModel;
    CommitHistoryListModel m_commitHistoryModel;

    QString m_currentBranch;
    bool m_isDetached = false;
    bool m_isBusy = false;
    QString m_statusMessage;
    QString m_remoteUrl;
    QStringList m_conflictedFiles;
};

} // namespace gf::desktop
