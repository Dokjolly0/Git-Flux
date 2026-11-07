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
        m_remoteUrl.clear();
        m_isDetached = false;
        m_branchListModel.setBranches({});
        m_commitHistoryModel.setCommits({});
        emit currentBranchChanged();
        emit remoteUrlChanged();
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

    // 2. Refresh commit history across all branches with full topological graph ordering
    git::bo::CommitLogOptions logOpts;
    logOpts.maxCount = 0; // 0 = unlimited (complete history)
    logOpts.revisionRange = std::string("--all");
    logOpts.topoOrder = true;
    auto logRes = m_historyService.getCommitHistory(m_repoPath, logOpts);
    if (logRes) {
        m_commitHistoryModel.setCommits(std::move(*logRes));
    }

    // 3. Refresh remote URL
    auto remotesRes = m_remoteMergeService.listRemotes(m_repoPath);
    QString url;
    if (remotesRes && !remotesRes->empty()) {
        for (const auto& r : *remotesRes) {
            if (r.name == "origin") {
                url = QString::fromStdString(r.pushUrl.empty() ? r.fetchUrl : r.pushUrl);
                break;
            }
        }
        if (url.isEmpty()) {
            url = QString::fromStdString((*remotesRes)[0].pushUrl.empty() ? (*remotesRes)[0].fetchUrl : (*remotesRes)[0].pushUrl);
        }
    }
    if (m_remoteUrl != url) {
        m_remoteUrl = url;
        emit remoteUrlChanged();
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

static QString formatRemoteError(const std::string& rawMsg) {
    const QString msg = QString::fromStdString(rawMsg);
    if (msg.contains("could not read Username", Qt::CaseInsensitive) ||
        msg.contains("terminal prompts disabled", Qt::CaseInsensitive)) {
        return QStringLiteral("Autenticazione HTTPS non riuscita: i prompt da terminale sono disabilitati nell'app. "
                              "Passa all'URL SSH del repository (es. git remote set-url origin git@github.com:...) "
                              "oppure configura un credential helper (git config --global credential.helper store). "
                              "Dettagli: %1").arg(msg.trimmed());
    }
    if (msg.contains("Permission denied (publickey)", Qt::CaseInsensitive)) {
        return QStringLiteral("Autenticazione SSH fallita. Verifica che la chiave SSH sia caricata nell'agente (ssh-add) "
                              "e configurata su GitHub/GitLab. Dettagli: %1").arg(msg.trimmed());
    }
    return msg;
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
        QString err = formatRemoteError(res.error().message);
        setStatusMessage(err);
        emit operationCompleted(false, err);
        return false;
    }

    setStatusMessage(QStringLiteral("Fetch da %1 completato").arg(remoteName));
    refresh();
    emit operationCompleted(true, QStringLiteral("Fetch completato"));
    return true;
}

void BranchManagerViewModel::setWipStatus(bool hasWip, int changedFilesCount) {
    m_commitHistoryModel.setWip(hasWip, changedFilesCount);
}

bool BranchManagerViewModel::pullRemote(const QString& remoteName) {
    if (m_repoPath.empty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Pull da %1 in corso...").arg(remoteName));

    git::bo::RemoteOperationOptions opts{
        .remoteName = remoteName.toStdString(),
        .branchName = m_currentBranch.isEmpty() ? std::nullopt : std::make_optional(m_currentBranch.toStdString())
    };

    auto res = m_remoteMergeService.pullRemote(m_repoPath, opts);
    setBusy(false);

    if (!res) {
        QString err = formatRemoteError(res.error().message);
        setStatusMessage(err);
        emit operationCompleted(false, err);
        return false;
    }

    setStatusMessage(QStringLiteral("Pull da %1 completato con successo").arg(remoteName));
    refresh();
    emit operationCompleted(true, QStringLiteral("Pull da %1 completato con successo").arg(remoteName));
    return true;
}

bool BranchManagerViewModel::pushRemote(const QString& remoteName, bool setUpstream) {
    if (m_repoPath.empty()) {
        return false;
    }

    setBusy(true);
    setStatusMessage(QStringLiteral("Push verso %1 in corso...").arg(remoteName));

    git::bo::RemoteOperationOptions opts{
        .remoteName = remoteName.toStdString(),
        .branchName = m_currentBranch.isEmpty() ? std::nullopt : std::make_optional(m_currentBranch.toStdString()),
        .setUpstream = setUpstream
    };

    auto res = m_remoteMergeService.pushRemote(m_repoPath, opts);

    // Auto-retry with setUpstream if branch has no remote tracking branch
    if (!res && !setUpstream && (res.error().message.find("no upstream branch") != std::string::npos ||
                                 res.error().message.find("set-upstream") != std::string::npos)) {
        opts.setUpstream = true;
        res = m_remoteMergeService.pushRemote(m_repoPath, opts);
    }

    setBusy(false);

    if (!res) {
        QString err = formatRemoteError(res.error().message);
        setStatusMessage(err);
        emit operationCompleted(false, err);
        return false;
    }

    setStatusMessage(QStringLiteral("Push verso %1 completato con successo").arg(remoteName));
    refresh();
    emit operationCompleted(true, QStringLiteral("Push verso %1 completato con successo").arg(remoteName));
    return true;
}

bool BranchManagerViewModel::setRemoteUrl(const QString& remoteName, const QString& newUrl) {
    if (m_repoPath.empty()) {
        return false;
    }

    setBusy(true);
    auto res = m_remoteMergeService.setRemoteUrl(m_repoPath, remoteName.toStdString(), newUrl.toStdString());
    setBusy(false);

    if (!res) {
        QString err = QString::fromStdString(res.error().message);
        setStatusMessage(err);
        emit operationCompleted(false, err);
        return false;
    }

    m_remoteUrl = newUrl;
    emit remoteUrlChanged();
    setStatusMessage(QStringLiteral("URL del remote %1 aggiornato").arg(remoteName));
    emit operationCompleted(true, QStringLiteral("URL del remote %1 aggiornato con successo").arg(remoteName));
    return true;
}

bool BranchManagerViewModel::convertRemoteToSsh(const QString& remoteName) {
    if (m_remoteUrl.isEmpty()) {
        emit operationCompleted(false, QStringLiteral("Nessun URL remoto configurato"));
        return false;
    }

    QString url = m_remoteUrl.trimmed();
    if (url.startsWith("https://", Qt::CaseInsensitive) || url.startsWith("http://", Qt::CaseInsensitive)) {
        int slashSlash = url.indexOf("://");
        QString withoutScheme = url.mid(slashSlash + 3);
        int slashHost = withoutScheme.indexOf('/');
        if (slashHost != -1) {
            QString host = withoutScheme.left(slashHost);
            QString path = withoutScheme.mid(slashHost + 1);
            if (!path.endsWith(".git", Qt::CaseInsensitive)) {
                path += ".git";
            }
            QString sshUrl = QStringLiteral("git@%1:%2").arg(host, path);
            return setRemoteUrl(remoteName, sshUrl);
        }
    }

    emit operationCompleted(false, QStringLiteral("L'URL remoto non è in formato HTTP/HTTPS valido da convertire: %1").arg(url));
    return false;
}

} // namespace gf::desktop
