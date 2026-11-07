#include "desktop/viewmodels/branch_manager_viewmodel.hpp"

namespace gf::desktop {

BranchManagerViewModel::BranchManagerViewModel(git::bl::GitBranchService& branchService,
                                               git::bl::GitHistoryService& historyService,
                                               git::bl::GitRemoteMergeService& remoteMergeService,
                                               QObject* parent)
    : QObject(parent)
    , m_branchService(branchService)
    , m_historyService(historyService)
    , m_remoteMergeService(remoteMergeService)
    , m_branchListModel(this)
    , m_commitHistoryModel(this) {}

void BranchManagerViewModel::setRepositoryPath(const QString& path) {
    if (path.isEmpty()) {
        m_repoPath.clear();
        m_currentBranch.clear();
        m_isDetached = false;
        m_branchListModel.setBranches({});
        m_commitHistoryModel.setCommits({});
        emit currentBranchChanged();
        return;
    }

    m_repoPath = std::filesystem::path(path.toStdString());
    refresh();
}

void BranchManagerViewModel::setBusy(bool busy) {
    if (m_isBusy != busy) {
        m_isBusy = busy;
        emit busyChanged();
    }
}

void BranchManagerViewModel::setStatusMessage(const QString& message) {
    if (m_statusMessage != message) {
        m_statusMessage = message;
        emit statusMessageChanged();
    }
}

void BranchManagerViewModel::refresh() {
    if (m_repoPath.empty()) {
        return;
    }

    setBusy(true);

    // 1. Refresh branches
    auto branchRes = m_branchService.listBranches(m_repoPath);
    if (branchRes) {
        m_branchListModel.setBranches(*branchRes);

        QString newCurrent;
        bool detached = false;
        for (const auto& b : *branchRes) {
            if (b.isCurrent) {
                newCurrent = QString::fromStdString(b.name);
                detached = b.isDetached;
                break;
            }
        }

        if (m_currentBranch != newCurrent || m_isDetached != detached) {
            m_currentBranch = newCurrent;
            m_isDetached = detached;
            emit currentBranchChanged();
        }
    }

    // 2. Refresh commit history
    git::bo::CommitLogOptions logOpts;
    logOpts.maxCount = 100;
    logOpts.revisionRange = std::string("--all");
    auto logRes = m_historyService.getCommitHistory(m_repoPath, logOpts);
    if (logRes) {
        m_commitHistoryModel.setCommits(std::move(*logRes));
    }

    setBusy(false);
}

QVariantList BranchManagerViewModel::getCommitFiles(const QString& commitSha) {
    if (m_repoPath.empty() || commitSha.isEmpty()) {
        return {};
    }

    auto res = m_historyService.getCommitFiles(m_repoPath, commitSha.toStdString());
    if (!res) {
        return {};
    }

    QVariantList list;
    list.reserve(static_cast<qsizetype>(res->size()));
    for (const auto& item : *res) {
        QVariantMap map;
        map[QStringLiteral("path")] = QString::fromStdString(item.path);
        map[QStringLiteral("oldPath")] = QString::fromStdString(item.oldPath);
        map[QStringLiteral("additions")] = item.additions;
        map[QStringLiteral("deletions")] = item.deletions;
        map[QStringLiteral("isBinary")] = item.isBinary;

        QString statusChar;
        QString statusLabel;
        switch (item.status) {
        case git::bo::FileChangeStatus::Added:
            statusChar = QStringLiteral("A");
            statusLabel = QStringLiteral("Added");
            break;
        case git::bo::FileChangeStatus::Deleted:
            statusChar = QStringLiteral("D");
            statusLabel = QStringLiteral("Deleted");
            break;
        case git::bo::FileChangeStatus::Renamed:
            statusChar = QStringLiteral("R");
            statusLabel = QStringLiteral("Renamed");
            break;
        case git::bo::FileChangeStatus::Copied:
            statusChar = QStringLiteral("C");
            statusLabel = QStringLiteral("Copied");
            break;
        default:
            statusChar = QStringLiteral("M");
            statusLabel = QStringLiteral("Modified");
            break;
        }
        map[QStringLiteral("status")] = statusChar;
        map[QStringLiteral("statusLabel")] = statusLabel;
        list.append(map);
    }
    return list;
}

bool BranchManagerViewModel::checkoutBranch(const QString& branchName) {
    if (m_repoPath.empty() || branchName.isEmpty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Checkout di %1 in corso...").arg(branchName));

    git::bo::CheckoutOptions opts{
        .branchOrCommit = branchName.toStdString()
    };

    auto res = m_branchService.checkoutBranch(m_repoPath, opts);
    setBusy(false);

    if (!res) {
        setStatusMessage(QString::fromStdString(res.error().message));
        emit operationCompleted(false, QString::fromStdString(res.error().message));
        return false;
    }

    setStatusMessage(QStringLiteral("Passato al branch %1").arg(branchName));
    refresh();
    emit branchSwitched(branchName);
    emit operationCompleted(true, QStringLiteral("Passato al branch %1").arg(branchName));
    return true;
}

bool BranchManagerViewModel::createBranch(const QString& branchName, bool checkoutAfterCreate) {
    if (m_repoPath.empty() || branchName.isEmpty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Creazione del branch %1...").arg(branchName));

    git::bo::CreateBranchOptions opts{
        .name = branchName.toStdString(),
        .checkoutAfterCreate = checkoutAfterCreate
    };

    auto res = m_branchService.createBranch(m_repoPath, opts);
    setBusy(false);

    if (!res) {
        setStatusMessage(QString::fromStdString(res.error().message));
        emit operationCompleted(false, QString::fromStdString(res.error().message));
        return false;
    }

    setStatusMessage(QStringLiteral("Branch %1 creato con successo").arg(branchName));
    refresh();
    if (checkoutAfterCreate) {
        emit branchSwitched(branchName);
    }
    emit operationCompleted(true, QStringLiteral("Branch %1 creato").arg(branchName));
    return true;
}

bool BranchManagerViewModel::deleteBranch(const QString& branchName, bool force) {
    if (m_repoPath.empty() || branchName.isEmpty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Eliminazione del branch %1...").arg(branchName));

    auto res = m_branchService.deleteBranch(m_repoPath, branchName.toStdString(), force);
    setBusy(false);

    if (!res) {
        setStatusMessage(QString::fromStdString(res.error().message));
        emit operationCompleted(false, QString::fromStdString(res.error().message));
        return false;
    }

    setStatusMessage(QStringLiteral("Branch %1 eliminato").arg(branchName));
    refresh();
    emit operationCompleted(true, QStringLiteral("Branch %1 eliminato").arg(branchName));
    return true;
}

bool BranchManagerViewModel::renameBranch(const QString& oldName, const QString& newName) {
    if (m_repoPath.empty() || oldName.isEmpty() || newName.isEmpty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Ridenominazione di %1 in %2...").arg(oldName, newName));

    auto res = m_branchService.renameBranch(m_repoPath, oldName.toStdString(), newName.toStdString());
    setBusy(false);

    if (!res) {
        setStatusMessage(QString::fromStdString(res.error().message));
        emit operationCompleted(false, QString::fromStdString(res.error().message));
        return false;
    }

    setStatusMessage(QStringLiteral("Branch rinominato in %1").arg(newName));
    refresh();
    emit operationCompleted(true, QStringLiteral("Branch rinominato in %1").arg(newName));
    return true;
}

bool BranchManagerViewModel::mergeBranch(const QString& sourceBranch, bool noFf) {
    if (m_repoPath.empty() || sourceBranch.isEmpty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Merge di %1 in corso...").arg(sourceBranch));

    git::bo::MergeOptions opts{
        .sourceBranchOrCommit = sourceBranch.toStdString(),
        .noFastForward = noFf
    };

    auto res = m_remoteMergeService.mergeBranch(m_repoPath, opts);
    setBusy(false);

    if (!res) {
        setStatusMessage(QString::fromStdString(res.error().message));
        emit operationCompleted(false, QString::fromStdString(res.error().message));
        return false;
    }

    if (res->status == git::bo::MergeStatus::Conflict) {
        m_conflictedFiles.clear();
        for (const auto& f : res->conflictedFiles) {
            m_conflictedFiles.append(QString::fromStdString(f));
        }
        emit conflictsChanged();
        setStatusMessage(QStringLiteral("Conflitti di merge rilevati in %1 file").arg(m_conflictedFiles.size()));
        emit operationCompleted(false, QStringLiteral("Conflitti di merge rilevati"));
        return false;
    }

    m_conflictedFiles.clear();
    emit conflictsChanged();
    setStatusMessage(QStringLiteral("Merge di %1 completato con successo").arg(sourceBranch));
    refresh();
    emit operationCompleted(true, QStringLiteral("Merge completato"));
    return true;
}

bool BranchManagerViewModel::abortMerge() {
    if (m_repoPath.empty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Annullamento del merge in corso..."));

    auto res = m_remoteMergeService.abortMerge(m_repoPath);
    setBusy(false);

    if (!res) {
        setStatusMessage(QString::fromStdString(res.error().message));
        emit operationCompleted(false, QString::fromStdString(res.error().message));
        return false;
    }

    m_conflictedFiles.clear();
    emit conflictsChanged();
    setStatusMessage(QStringLiteral("Merge annullato con successo"));
    refresh();
    emit operationCompleted(true, QStringLiteral("Merge annullato"));
    return true;
}

bool BranchManagerViewModel::fetchRemote(const QString& remoteName) {
    if (m_repoPath.empty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Fetch da %1...").arg(remoteName));

    git::bo::RemoteOperationOptions opts{
        .remoteName = remoteName.toStdString()
    };

    auto res = m_remoteMergeService.fetchRemote(m_repoPath, opts);
    setBusy(false);

    if (!res) {
        setStatusMessage(QString::fromStdString(res.error().message));
        emit operationCompleted(false, QString::fromStdString(res.error().message));
        return false;
    }

    setStatusMessage(QStringLiteral("Fetch da %1 completato").arg(remoteName));
    refresh();
    emit operationCompleted(true, QStringLiteral("Fetch completato"));
    return true;
}

bool BranchManagerViewModel::pullRemote(const QString& remoteName) {
    if (m_repoPath.empty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Pull da %1...").arg(remoteName));

    git::bo::RemoteOperationOptions opts{
        .remoteName = remoteName.toStdString()
    };

    auto res = m_remoteMergeService.pullRemote(m_repoPath, opts);
    setBusy(false);

    if (!res) {
        setStatusMessage(QString::fromStdString(res.error().message));
        emit operationCompleted(false, QString::fromStdString(res.error().message));
        return false;
    }

    setStatusMessage(QStringLiteral("Pull da %1 completato").arg(remoteName));
    refresh();
    emit operationCompleted(true, QStringLiteral("Pull completato"));
    return true;
}

bool BranchManagerViewModel::pushRemote(const QString& remoteName, bool setUpstream) {
    if (m_repoPath.empty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Push verso %1...").arg(remoteName));

    git::bo::RemoteOperationOptions opts{
        .remoteName = remoteName.toStdString(),
        .setUpstream = setUpstream
    };

    auto res = m_remoteMergeService.pushRemote(m_repoPath, opts);
    setBusy(false);

    if (!res) {
        setStatusMessage(QString::fromStdString(res.error().message));
        emit operationCompleted(false, QString::fromStdString(res.error().message));
        return false;
    }

    setStatusMessage(QStringLiteral("Push verso %1 completato").arg(remoteName));
    refresh();
    emit operationCompleted(true, QStringLiteral("Push completato"));
    return true;
}

} // namespace gf::desktop
