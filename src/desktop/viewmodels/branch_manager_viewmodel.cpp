#include "desktop/viewmodels/branch_manager_viewmodel.hpp"

#include "core/modules/git/bl/git_branch_service.hpp"
#include "core/modules/git/bl/git_history_service.hpp"
#include "core/modules/git/bl/git_rebase_service.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"

#include <QClipboard>
#include <QGuiApplication>
#include <QUrl>
#include <QUrlQuery>

namespace gf::desktop
{
    BranchManagerViewModel::BranchManagerViewModel(git::bl::GitBranchService& branchService,
                                                   git::bl::GitHistoryService& historyService,
                                                   git::bl::GitRemoteMergeService& remoteMergeService,
                                                   git::bl::GitRebaseService* rebaseService,
                                                   QObject* parent)
        : QObject(parent)
          , m_branchService(branchService)
          , m_historyService(historyService)
          , m_remoteMergeService(remoteMergeService)
          , m_rebaseService(rebaseService)
          , m_branchListModel(this)
          , m_commitHistoryModel(this)
    {
    }

    void BranchManagerViewModel::setRepositoryPath(const QString& path)
    {
        if (path.isEmpty())
        {
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

    void BranchManagerViewModel::setBusy(bool busy)
    {
        if (m_isBusy != busy)
        {
            m_isBusy = busy;
            emit busyChanged();
        }
    }

    void BranchManagerViewModel::setStatusMessage(const QString& message)
    {
        if (m_statusMessage != message)
        {
            m_statusMessage = message;
            emit statusMessageChanged();
        }
    }

    void BranchManagerViewModel::refresh()
    {
        if (m_repoPath.empty())
        {
            return;
        }

        setBusy(true);

        // 1. Refresh branches
        auto branchRes = m_branchService.listBranches(m_repoPath);
        if (branchRes)
        {
            m_branchListModel.setBranches(*branchRes);

            QString newCurrent;
            bool detached = false;
            for (const auto& b : *branchRes)
            {
                if (b.isCurrent)
                {
                    newCurrent = QString::fromStdString(b.name);
                    detached = b.isDetached;
                    break;
                }
            }

            if (m_currentBranch != newCurrent || m_isDetached != detached)
            {
                m_currentBranch = newCurrent;
                m_isDetached = detached;
                emit currentBranchChanged();
            }
        }

        // 2. Refresh commit history across all branches with date ordering (keeps commits at their creation row)
        git::bo::CommitLogOptions logOpts;
        logOpts.maxCount = 0; // 0 = unlimited (complete history)
        logOpts.revisionRange = std::string("--all");
        logOpts.dateOrder = true;
        if (!m_fileHistoryPath.isEmpty())
        {
            logOpts.followRenames = true;
            logOpts.pathFilter = m_fileHistoryPath.toStdString();
        }
        auto logRes = m_historyService.getCommitHistory(m_repoPath, logOpts);
        if (logRes)
        {
            m_commitHistoryModel.setCommits(std::move(*logRes));
        }

        // 3. Refresh remote URL
        auto remotesRes = m_remoteMergeService.listRemotes(m_repoPath);
        QString url;
        if (remotesRes && !remotesRes->empty())
        {
            for (const auto& r : *remotesRes)
            {
                if (r.name == "origin")
                {
                    url = QString::fromStdString(r.pushUrl.empty() ? r.fetchUrl : r.pushUrl);
                    break;
                }
            }
            if (url.isEmpty())
            {
                url = QString::fromStdString((*remotesRes)[0].pushUrl.empty()
                                                 ? (*remotesRes)[0].fetchUrl
                                                 : (*remotesRes)[0].pushUrl);
            }
        }
        if (m_remoteUrl != url)
        {
            m_remoteUrl = url;
            emit remoteUrlChanged();
        }

        setBusy(false);
    }

    QVariantList BranchManagerViewModel::getCommitFiles(const QString& commitSha)
    {
        if (m_repoPath.empty() || commitSha.isEmpty())
        {
            return {};
        }

        auto res = m_historyService.getCommitFiles(m_repoPath, commitSha.toStdString());
        if (!res)
        {
            return {};
        }

        QVariantList list;
        list.reserve(static_cast<qsizetype>(res->size()));
        for (const auto& item : *res)
        {
            QVariantMap map;
            map[QStringLiteral("path")] = QString::fromStdString(item.path);
            map[QStringLiteral("oldPath")] = QString::fromStdString(item.oldPath);
            map[QStringLiteral("additions")] = item.additions;
            map[QStringLiteral("deletions")] = item.deletions;
            map[QStringLiteral("isBinary")] = item.isBinary;

            QString statusChar;
            QString statusLabel;
            switch (item.status)
            {
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

    bool BranchManagerViewModel::checkoutBranch(const QString& branchName)
    {
        if (m_repoPath.empty() || branchName.isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Checkout di %1 in corso...").arg(branchName));

        git::bo::CheckoutOptions opts{
            .branchOrCommit = branchName.toStdString()
        };

        auto res = m_branchService.checkoutBranch(m_repoPath, opts);
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        refresh();

        // Report what git actually did: a remote name may have been resolved to (or created as)
        // a local branch, while tags and SHAs intentionally leave HEAD detached.
        const QString message = (m_isDetached || m_currentBranch.isEmpty())
                                    ? QStringLiteral("Checkout di %1 completato (HEAD detached)").arg(branchName)
                                    : QStringLiteral("Passato al branch %1").arg(m_currentBranch);
        setStatusMessage(message);
        emit branchSwitched(m_currentBranch.isEmpty() ? branchName : m_currentBranch);
        emit operationCompleted(true, message);
        return true;
    }

    bool BranchManagerViewModel::createBranch(const QString& branchName, bool checkoutAfterCreate,
                                              const QString& startPoint)
    {
        if (m_repoPath.empty() || branchName.isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Creazione del branch %1...").arg(branchName));

        git::bo::CreateBranchOptions opts{
            .name = branchName.toStdString(),
            .startPoint = startPoint.isEmpty() ? std::nullopt : std::make_optional(startPoint.toStdString()),
            .checkoutAfterCreate = checkoutAfterCreate
        };

        auto res = m_branchService.createBranch(m_repoPath, opts);
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        setStatusMessage(QStringLiteral("Branch %1 creato con successo").arg(branchName));
        refresh();
        if (checkoutAfterCreate)
        {
            emit branchSwitched(branchName);
        }
        emit operationCompleted(true, QStringLiteral("Branch %1 creato").arg(branchName));
        return true;
    }

    bool BranchManagerViewModel::deleteBranch(const QString& branchName, bool force)
    {
        if (m_repoPath.empty() || branchName.isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Eliminazione del branch %1...").arg(branchName));

        auto res = m_branchService.deleteBranch(m_repoPath, branchName.toStdString(), force);
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        setStatusMessage(QStringLiteral("Branch %1 eliminato").arg(branchName));
        refresh();
        emit operationCompleted(true, QStringLiteral("Branch %1 eliminato").arg(branchName));
        return true;
    }

    bool BranchManagerViewModel::renameBranch(const QString& oldName, const QString& newName)
    {
        if (m_repoPath.empty() || oldName.isEmpty() || newName.isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Ridenominazione di %1 in %2...").arg(oldName, newName));

        auto res = m_branchService.renameBranch(m_repoPath, oldName.toStdString(), newName.toStdString());
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        setStatusMessage(QStringLiteral("Branch rinominato in %1").arg(newName));
        refresh();
        emit operationCompleted(true, QStringLiteral("Branch rinominato in %1").arg(newName));
        return true;
    }

    bool BranchManagerViewModel::mergeBranch(const QString& sourceBranch, bool noFf)
    {
        if (m_repoPath.empty() || sourceBranch.isEmpty())
        {
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

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        if (res->status == git::bo::MergeStatus::Conflict)
        {
            m_conflictedFiles.clear();
            for (const auto& f : res->conflictedFiles)
            {
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

    bool BranchManagerViewModel::abortMerge()
    {
        if (m_repoPath.empty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Annullamento del merge in corso..."));

        auto res = m_remoteMergeService.abortMerge(m_repoPath);
        setBusy(false);

        if (!res)
        {
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

    static QString formatRemoteError(const std::string& rawMsg)
    {
        const QString msg = QString::fromStdString(rawMsg);
        if (msg.contains("could not read Username", Qt::CaseInsensitive) ||
            msg.contains("terminal prompts disabled", Qt::CaseInsensitive))
        {
            return QStringLiteral(
                "Autenticazione HTTPS non riuscita: i prompt da terminale sono disabilitati nell'app. "
                "Passa all'URL SSH del repository (es. git remote set-url origin git@github.com:...) "
                "oppure configura un credential helper (git config --global credential.helper store). "
                "Dettagli: %1").arg(msg.trimmed());
        }
        if (msg.contains("Permission denied (publickey)", Qt::CaseInsensitive))
        {
            return QStringLiteral(
                "Autenticazione SSH fallita. Verifica che la chiave SSH sia caricata nell'agente (ssh-add) "
                "e configurata su GitHub/GitLab. Dettagli: %1").arg(msg.trimmed());
        }
        return msg;
    }

    bool BranchManagerViewModel::fetchRemote(const QString& remoteName, bool allRemotes)
    {
        if (m_repoPath.empty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(allRemotes
                             ? QStringLiteral("Fetch da tutti i remote...")
                             : QStringLiteral("Fetch da %1...").arg(remoteName));

        git::bo::RemoteOperationOptions opts{
            .remoteName = remoteName.toStdString(),
            .prune = true,
            .allRemotes = allRemotes
        };

        auto res = m_remoteMergeService.fetchRemote(m_repoPath, opts);
        setBusy(false);

        if (!res)
        {
            QString err = formatRemoteError(res.error().message);
            setStatusMessage(err);
            emit operationCompleted(false, err);
            return false;
        }

        QString successMsg = allRemotes
                                 ? QStringLiteral("Fetch da tutti i remote completato")
                                 : QStringLiteral("Fetch da %1 completato").arg(remoteName);
        setStatusMessage(successMsg);
        refresh();
        emit operationCompleted(true, successMsg);
        return true;
    }

    void BranchManagerViewModel::setWipStatus(bool hasWip, int changedFilesCount)
    {
        if (!m_fileHistoryPath.isEmpty())
        {
            m_commitHistoryModel.setWip(false, 0);
        }
        else
        {
            m_commitHistoryModel.setWip(hasWip, changedFilesCount);
        }
    }

    bool BranchManagerViewModel::pullRemote(const QString& remoteName, const QString& strategy)
    {
        if (m_repoPath.empty())
        {
            return false;
        }

        setBusy(true);
        QString stratLabel = (strategy == "rebase")
                                 ? " con rebase"
                                 : ((strategy == "ff-only") ? " (solo fast-forward)" : "");
        setStatusMessage(QStringLiteral("Pull da %1%2 in corso...").arg(remoteName, stratLabel));

        git::bo::RemoteOperationOptions opts{
            .remoteName = remoteName.toStdString(),
            .branchName = m_currentBranch.isEmpty() ? std::nullopt : std::make_optional(m_currentBranch.toStdString()),
            .rebase = (strategy == "rebase"),
            .fastForwardOnly = (strategy == "ff-only")
        };

        auto res = m_remoteMergeService.pullRemote(m_repoPath, opts);
        setBusy(false);

        if (!res)
        {
            QString err = formatRemoteError(res.error().message);
            setStatusMessage(err);
            emit operationCompleted(false, err);
            return false;
        }

        QString successMsg = QStringLiteral("Pull da %1 completato con successo%2").arg(remoteName, stratLabel);
        setStatusMessage(successMsg);
        refresh();
        emit operationCompleted(true, successMsg);
        return true;
    }

    bool BranchManagerViewModel::pushRemote(const QString& remoteName, bool setUpstream, bool force, bool tags)
    {
        if (m_repoPath.empty())
        {
            return false;
        }

        setBusy(true);
        QString optsDesc = force
                               ? " (force-with-lease)"
                               : (tags ? " (con tag)" : (setUpstream ? " (set-upstream)" : ""));
        setStatusMessage(QStringLiteral("Push verso %1%2 in corso...").arg(remoteName, optsDesc));

        git::bo::RemoteOperationOptions opts{
            .remoteName = remoteName.toStdString(),
            .branchName = m_currentBranch.isEmpty() ? std::nullopt : std::make_optional(m_currentBranch.toStdString()),
            .setUpstream = setUpstream,
            .force = force,
            .tags = tags
        };

        auto res = m_remoteMergeService.pushRemote(m_repoPath, opts);

        // Auto-retry with setUpstream if branch has no remote tracking branch
        if (!res && !setUpstream && (res.error().message.find("no upstream branch") != std::string::npos ||
            res.error().message.find("set-upstream") != std::string::npos))
        {
            opts.setUpstream = true;
            res = m_remoteMergeService.pushRemote(m_repoPath, opts);
        }

        setBusy(false);

        if (!res)
        {
            QString err = formatRemoteError(res.error().message);
            setStatusMessage(err);
            emit operationCompleted(false, err);
            return false;
        }

        QString successMsg = QStringLiteral("Push verso %1 completato con successo%2").arg(remoteName, optsDesc);
        setStatusMessage(successMsg);
        refresh();
        emit operationCompleted(true, successMsg);
        return true;
    }

    bool BranchManagerViewModel::setRemoteUrl(const QString& remoteName, const QString& newUrl)
    {
        if (m_repoPath.empty())
        {
            return false;
        }

        setBusy(true);
        auto res = m_remoteMergeService.setRemoteUrl(m_repoPath, remoteName.toStdString(), newUrl.toStdString());
        setBusy(false);

        if (!res)
        {
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

    bool BranchManagerViewModel::convertRemoteToSsh(const QString& remoteName)
    {
        if (m_remoteUrl.isEmpty())
        {
            emit operationCompleted(false, QStringLiteral("Nessun URL remoto configurato"));
            return false;
        }

        QString url = m_remoteUrl.trimmed();
        if (url.startsWith("https://", Qt::CaseInsensitive) || url.startsWith("http://", Qt::CaseInsensitive))
        {
            int slashSlash = url.indexOf("://");
            QString withoutScheme = url.mid(slashSlash + 3);
            int slashHost = withoutScheme.indexOf('/');
            if (slashHost != -1)
            {
                QString host = withoutScheme.left(slashHost);
                QString path = withoutScheme.mid(slashHost + 1);
                if (!path.endsWith(".git", Qt::CaseInsensitive))
                {
                    path += ".git";
                }
                QString sshUrl = QStringLiteral("git@%1:%2").arg(host, path);
                return setRemoteUrl(remoteName, sshUrl);
            }
        }

        emit operationCompleted(
            false, QStringLiteral("L'URL remoto non è in formato HTTP/HTTPS valido da convertire: %1").arg(url));
        return false;
    }

    bool BranchManagerViewModel::checkoutCommit(const QString& commitSha)
    {
        return checkoutBranch(commitSha);
    }

    bool BranchManagerViewModel::resetCommit(const QString& commitSha, const QString& mode)
    {
        if (m_repoPath.empty() || commitSha.isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Reset al commit %1 (%2) in corso...").arg(commitSha.left(7), mode));

        auto res = m_branchService.resetCommit(m_repoPath, commitSha.toStdString(), mode.toStdString());
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        setStatusMessage(QStringLiteral("Reset al commit %1 (%2) completato").arg(commitSha.left(7), mode));
        refresh();
        emit branchSwitched(m_currentBranch);
        emit operationCompleted(
            true, QStringLiteral("Reset al commit %1 completato con successo").arg(commitSha.left(7)));
        return true;
    }

    bool BranchManagerViewModel::revertCommit(const QString& commitSha)
    {
        if (m_repoPath.empty() || commitSha.isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Revert del commit %1 in corso...").arg(commitSha.left(7)));

        auto res = m_branchService.revertCommit(m_repoPath, commitSha.toStdString());
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        setStatusMessage(QStringLiteral("Revert del commit %1 completato").arg(commitSha.left(7)));
        refresh();
        emit branchSwitched(m_currentBranch);
        emit operationCompleted(
            true, QStringLiteral("Revert del commit %1 completato con successo").arg(commitSha.left(7)));
        return true;
    }

    bool BranchManagerViewModel::rewordCommit(const QString& commitSha, const QString& newMessage)
    {
        if (m_repoPath.empty() || commitSha.isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Modifica del messaggio del commit %1 in corso...").arg(commitSha.left(7)));

        auto res = m_branchService.rewordCommit(m_repoPath, commitSha.toStdString(), newMessage.toStdString());
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        const QString newSha = QString::fromStdString(*res);
        setStatusMessage(QStringLiteral("Messaggio del commit aggiornato (%1 → %2)").arg(commitSha.left(7), newSha.left(7)));
        refresh();
        emit branchSwitched(m_currentBranch);
        emit commitReworded(commitSha, newSha);
        emit operationCompleted(
            true, QStringLiteral("Messaggio del commit %1 aggiornato con successo").arg(commitSha.left(7)));
        return true;
    }

    bool BranchManagerViewModel::cherryPickCommit(const QString& commitSha)
    {
        if (m_repoPath.empty() || commitSha.isEmpty())
        {
            return false;
        }

        if (!m_rebaseService)
        {
            emit operationCompleted(false, QStringLiteral("Servizio cherry-pick/rebase non disponibile"));
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Cherry-pick del commit %1 in corso...").arg(commitSha.left(7)));

        git::bo::CherryPickOptions opts{
            .commitSha = commitSha.toStdString()
        };

        auto res = m_rebaseService->cherryPick(m_repoPath, opts);
        setBusy(false);

        if (!res || !res->success)
        {
            QString err = res ? QString::fromStdString(res->message) : QString::fromStdString(res.error().message);
            setStatusMessage(err);
            emit operationCompleted(false, err);
            return false;
        }

        setStatusMessage(QStringLiteral("Cherry-pick del commit %1 completato con successo").arg(commitSha.left(7)));
        refresh();
        emit branchSwitched(m_currentBranch);
        emit operationCompleted(
            true, QStringLiteral("Cherry-pick del commit %1 completato con successo").arg(commitSha.left(7)));
        return true;
    }

    bool BranchManagerViewModel::createTag(const QString& tagName, const QString& targetCommit, const QString& message)
    {
        if (m_repoPath.empty() || tagName.trimmed().isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Creazione del tag %1 in corso...").arg(tagName));

        git::bo::CreateTagOptions opts;
        opts.name = tagName.trimmed().toStdString();
        if (!targetCommit.trimmed().isEmpty())
        {
            opts.targetCommit = targetCommit.trimmed().toStdString();
        }
        if (!message.trimmed().isEmpty())
        {
            opts.message = message.trimmed().toStdString();
        }

        auto res = m_historyService.createTag(m_repoPath, opts);
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        setStatusMessage(QStringLiteral("Tag %1 creato con successo").arg(tagName));
        refresh();
        emit operationCompleted(true, QStringLiteral("Tag %1 creato con successo").arg(tagName));
        return true;
    }

    bool BranchManagerViewModel::deleteTag(const QString& tagName)
    {
        if (m_repoPath.empty() || tagName.trimmed().isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Eliminazione del tag %1 in corso...").arg(tagName));

        auto res = m_historyService.deleteTag(m_repoPath, tagName.trimmed().toStdString());
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        setStatusMessage(QStringLiteral("Tag %1 eliminato").arg(tagName));
        refresh();
        emit operationCompleted(true, QStringLiteral("Tag %1 eliminato con successo").arg(tagName));
        return true;
    }

    bool BranchManagerViewModel::createWorktree(const QString& targetPath, const QString& commitOrBranch)
    {
        if (m_repoPath.empty() || targetPath.trimmed().isEmpty())
        {
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Creazione del worktree in %1...").arg(targetPath));

        auto res = m_branchService.createWorktree(m_repoPath, targetPath.toStdString(),
                                                  commitOrBranch.trimmed().toStdString());
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        setStatusMessage(QStringLiteral("Worktree creato con successo in %1").arg(targetPath));
        emit operationCompleted(true, QStringLiteral("Worktree creato con successo in %1").arg(targetPath));
        return true;
    }

    bool BranchManagerViewModel::rebaseOnto(const QString& targetBranch)
    {
        if (m_repoPath.empty() || targetBranch.isEmpty())
        {
            return false;
        }

        if (!m_rebaseService)
        {
            emit operationCompleted(false, QStringLiteral("Servizio rebase non disponibile"));
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Rebase su %1 in corso...").arg(targetBranch));

        git::bo::RebaseOptions opts{
            .upstream = targetBranch.toStdString(),
            .autostash = true
        };
        if (!m_currentBranch.isEmpty())
        {
            opts.branch = m_currentBranch.toStdString();
        }

        auto res = m_rebaseService->startRebase(m_repoPath, opts);
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        if (res->status == git::bo::RebaseStatus::Conflict)
        {
            m_conflictedFiles.clear();
            for (const auto& f : res->conflictedFiles)
            {
                m_conflictedFiles.append(QString::fromStdString(f));
            }
            emit conflictsChanged();
            setStatusMessage(QStringLiteral("Conflitti rilevati durante il rebase"));
            emit operationCompleted(false, QStringLiteral("Conflitti rilevati durante il rebase"));
            return false;
        }

        setStatusMessage(QStringLiteral("Rebase su %1 completato con successo").arg(targetBranch));
        refresh();
        emit branchSwitched(m_currentBranch);
        emit operationCompleted(true, QStringLiteral("Rebase su %1 completato con successo").arg(targetBranch));
        return true;
    }

    bool BranchManagerViewModel::rebaseBranch(const QString& sourceBranch, const QString& targetBranch)
    {
        if (m_repoPath.empty() || sourceBranch.isEmpty() || targetBranch.isEmpty())
        {
            return false;
        }

        if (!m_rebaseService)
        {
            emit operationCompleted(false, QStringLiteral("Servizio rebase non disponibile"));
            return false;
        }

        setBusy(true);
        setStatusMessage(QStringLiteral("Rebase di %1 su %2 in corso...").arg(sourceBranch, targetBranch));

        git::bo::RebaseOptions opts{
            .upstream = targetBranch.toStdString(),
            .branch = sourceBranch.toStdString(),
            .autostash = true
        };

        auto res = m_rebaseService->startRebase(m_repoPath, opts);
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        if (res->status == git::bo::RebaseStatus::Conflict)
        {
            m_conflictedFiles.clear();
            for (const auto& f : res->conflictedFiles)
            {
                m_conflictedFiles.append(QString::fromStdString(f));
            }
            emit conflictsChanged();
            setStatusMessage(QStringLiteral("Conflitti rilevati durante il rebase"));
            emit operationCompleted(false, QStringLiteral("Conflitti rilevati durante il rebase"));
            return false;
        }

        setStatusMessage(QStringLiteral("Rebase di %1 su %2 completato con successo").arg(sourceBranch, targetBranch));
        refresh();
        emit branchSwitched(sourceBranch);
        emit operationCompleted(
            true, QStringLiteral("Rebase di %1 su %2 completato con successo").arg(sourceBranch, targetBranch));
        return true;
    }

    bool BranchManagerViewModel::mergeBranchInto(const QString& sourceBranch, const QString& targetBranch)
    {
        if (m_repoPath.empty() || sourceBranch.isEmpty() || targetBranch.isEmpty())
        {
            return false;
        }

        QString effectiveTarget = targetBranch;
        if (effectiveTarget.startsWith(QLatin1String("origin/")))
        {
            effectiveTarget = effectiveTarget.mid(7);
        }

        if (m_currentBranch == effectiveTarget)
        {
            return mergeBranch(sourceBranch);
        }

        if (!checkoutBranch(effectiveTarget))
        {
            return false;
        }

        return mergeBranch(sourceBranch);
    }

    namespace
    {
        // Drop targets may be remote badges ("origin/foo"): fast-forward always applies to the local branch.
        QString localBranchName(const QString& name)
        {
            return name.startsWith(QLatin1String("origin/")) ? name.mid(7) : name;
        }
    } // namespace

    bool BranchManagerViewModel::canFastForwardBranch(const QString& targetBranch, const QString& sourceBranch)
    {
        if (m_repoPath.empty() || targetBranch.isEmpty() || sourceBranch.isEmpty())
        {
            return false;
        }
        auto res = m_branchService.canFastForwardBranch(m_repoPath, localBranchName(targetBranch).toStdString(),
                                                        sourceBranch.toStdString());
        return res && *res;
    }

    bool BranchManagerViewModel::fastForwardBranch(const QString& targetBranch, const QString& sourceBranch)
    {
        if (m_repoPath.empty() || targetBranch.isEmpty() || sourceBranch.isEmpty())
        {
            return false;
        }

        const QString target = localBranchName(targetBranch);
        setBusy(true);
        setStatusMessage(QStringLiteral("Fast-forward di '%1' a '%2' in corso...").arg(target, sourceBranch));

        auto res = m_branchService.fastForwardBranch(m_repoPath, target.toStdString(), sourceBranch.toStdString());
        setBusy(false);

        if (!res)
        {
            setStatusMessage(QString::fromStdString(res.error().message));
            emit operationCompleted(false, QString::fromStdString(res.error().message));
            return false;
        }

        setStatusMessage(QStringLiteral("'%1' aggiornato a '%2' (fast-forward)").arg(target, sourceBranch));
        refresh();
        if (m_currentBranch == target)
        {
            emit branchSwitched(m_currentBranch);
        }
        emit operationCompleted(
            true, QStringLiteral("Fast-forward di '%1' a '%2' completato con successo").arg(target, sourceBranch));
        return true;
    }

    void BranchManagerViewModel::copyToClipboard(const QString& text)
    {
        if (auto* cb = QGuiApplication::clipboard())
        {
            cb->setText(text);
        }
    }

    QString BranchManagerViewModel::buildPullRequestUrl(const QString& sourceBranch,
                                                        const QString& targetBranch,
                                                        const QString& title,
                                                        const QString& body) const
    {
        if (m_remoteUrl.isEmpty() || sourceBranch.isEmpty() || targetBranch.isEmpty())
        {
            return {};
        }

        QString cleanSource = sourceBranch;
        if (cleanSource.startsWith(QLatin1String("origin/")))
        {
            cleanSource = cleanSource.mid(7);
        }
        QString cleanTarget = targetBranch;
        if (cleanTarget.startsWith(QLatin1String("origin/")))
        {
            cleanTarget = cleanTarget.mid(7);
        }

        QString rawUrl = m_remoteUrl.trimmed();
        QString host;
        QString repoPath;

        if (rawUrl.startsWith(QLatin1String("git@")))
        {
            int colon = rawUrl.indexOf(':');
            if (colon != -1)
            {
                host = rawUrl.mid(4, colon - 4);
                repoPath = rawUrl.mid(colon + 1);
            }
        }
        else
        {
            QUrl parsed(rawUrl);
            host = parsed.host();
            repoPath = parsed.path();
            if (repoPath.startsWith('/'))
            {
                repoPath = repoPath.mid(1);
            }
        }

        if (repoPath.endsWith(QLatin1String(".git"), Qt::CaseInsensitive))
        {
            repoPath.chop(4);
        }

        if (host.isEmpty() || repoPath.isEmpty())
        {
            return {};
        }

        QString finalUrl;
        if (host.contains(QLatin1String("github.com"), Qt::CaseInsensitive))
        {
            QUrl url(QStringLiteral("https://%1/%2/compare/%3...%4").arg(host, repoPath, cleanTarget, cleanSource));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("expand"), QStringLiteral("1"));
            if (!title.isEmpty())
            {
                query.addQueryItem(QStringLiteral("title"), title);
            }
            if (!body.isEmpty())
            {
                query.addQueryItem(QStringLiteral("body"), body);
            }
            url.setQuery(query);
            finalUrl = url.toString();
        }
        else if (host.contains(QLatin1String("gitlab.com"), Qt::CaseInsensitive))
        {
            QUrl url(QStringLiteral("https://%1/%2/-/merge_requests/new").arg(host, repoPath));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("merge_request[source_branch]"), cleanSource);
            query.addQueryItem(QStringLiteral("merge_request[target_branch]"), cleanTarget);
            if (!title.isEmpty())
            {
                query.addQueryItem(QStringLiteral("merge_request[title]"), title);
            }
            if (!body.isEmpty())
            {
                query.addQueryItem(QStringLiteral("merge_request[description]"), body);
            }
            url.setQuery(query);
            finalUrl = url.toString();
        }
        else if (host.contains(QLatin1String("bitbucket.org"), Qt::CaseInsensitive))
        {
            QUrl url(QStringLiteral("https://%1/%2/pull-requests/new").arg(host, repoPath));
            QUrlQuery query;
            query.addQueryItem(QStringLiteral("source"), cleanSource);
            query.addQueryItem(QStringLiteral("dest"), cleanTarget);
            url.setQuery(query);
            finalUrl = url.toString();
        }
        else
        {
            // Generic git server fallback
            finalUrl = QStringLiteral("https://%1/%2/compare/%3...%4").arg(host, repoPath, cleanTarget, cleanSource);
        }

        return finalUrl;
    }

    bool BranchManagerViewModel::loadFileHistory(const QString& filePath)
    {
        if (m_repoPath.empty() || filePath.trimmed().isEmpty())
        {
            clearFileHistory();
            return false;
        }

        QString cleanPath = filePath.trimmed();
        cleanPath.replace(QLatin1Char('\\'), QLatin1Char('/'));

        git::bo::CommitLogOptions logOpts;
        logOpts.maxCount = 0;
        logOpts.revisionRange = std::string("--all");
        logOpts.followRenames = true;
        logOpts.pathFilter = cleanPath.toStdString();
        logOpts.dateOrder = true;

        auto logRes = m_historyService.getCommitHistory(m_repoPath, logOpts);
        if (logRes)
        {
            m_fileHistoryPath = cleanPath;
            m_commitHistoryModel.setWip(false, 0);
            m_commitHistoryModel.setCommits(std::move(*logRes));
            emit fileHistoryPathChanged();
            return true;
        }

        return false;
    }

    void BranchManagerViewModel::clearFileHistory()
    {
        if (m_fileHistoryPath.isEmpty())
        {
            return;
        }
        m_fileHistoryPath.clear();
        emit fileHistoryPathChanged();
        refresh();
    }

    QStringList BranchManagerViewModel::listRepositoryFiles()
    {
        if (m_repoPath.empty())
        {
            return {};
        }

        auto res = m_historyService.listTrackedFiles(m_repoPath);
        if (!res)
        {
            return {};
        }

        QStringList list;
        list.reserve(static_cast<qsizetype>(res->size()));
        for (const auto& f : *res)
        {
            list.append(QString::fromStdString(f));
        }
        return list;
    }
} // namespace gf::desktop
