#include "desktop/viewmodels/branch_manager_viewmodel.hpp"

#include "core/modules/git/bl/git_branch_service.hpp"
#include "core/modules/git/bl/git_history_service.hpp"
#include "core/modules/git/bl/git_rebase_service.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "desktop/services/serial_git_executor.hpp"

#include <QClipboard>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>

#include <expected>
#include <optional>
#include <vector>

namespace gf::desktop
{
    /// Result of the git reads behind a refresh, produced on the worker thread and
    /// applied to the Qt models on the GUI thread.
    struct BranchManagerViewModel::RefreshSnapshot
    {
        std::optional<std::vector<git::bo::GitBranch>> branches;
        std::optional<std::vector<git::bo::GitCommitLogItem>> commits;
        std::optional<std::vector<git::bo::GitRemote>> remotes;
    };

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
          , m_ownedExecutor(std::make_unique<SerialGitExecutor>())
          , m_readExecutor(std::make_unique<SerialGitExecutor>())
    {
        // A single worker thread keeps background git work strictly ordered
        // (e.g. a checkout always completes before the refresh queued after it).
        m_executor = m_ownedExecutor.get();
    }

    BranchManagerViewModel::BranchManagerViewModel(git::bl::GitBranchService& branchService,
                                                   git::bl::GitHistoryService& historyService,
                                                   git::bl::GitRemoteMergeService& remoteMergeService,
                                                   git::bl::GitRebaseService* rebaseService,
                                                   SerialGitExecutor& executor,
                                                   QObject* parent)
        : QObject(parent)
          , m_branchService(branchService)
          , m_historyService(historyService)
          , m_remoteMergeService(remoteMergeService)
          , m_rebaseService(rebaseService)
          , m_branchListModel(this)
          , m_commitHistoryModel(this)
          , m_executor(&executor)
          , m_readExecutor(std::make_unique<SerialGitExecutor>())
    {
    }

    // The owned executor (declared last) is destroyed first and waits for the running tasks;
    // a shared executor is destroyed by its owner before this object.
    BranchManagerViewModel::~BranchManagerViewModel() = default;

    template <typename Work, typename Finish>
    int BranchManagerViewModel::runOperation(const QString& startMessage, Work work, Finish finish)
    {
        const int id = ++m_lastOperationId;
        setStatusMessage(startMessage);
        ++m_pendingOperations;
        updateBusy();

        const auto repoPath = m_repoPath;
        m_executor->run(this, std::move(work),
                        [this, id, repoPath, finish](auto result) mutable
                        {
                            --m_pendingOperations;
                            finish(id, repoPath == m_repoPath, std::move(result));
                            updateBusy();
                        });
        return id;
    }

    void BranchManagerViewModel::completeOperation(int operationId, bool success, const QString& message,
                                                   const QString& statusMessage)
    {
        setStatusMessage(statusMessage.isEmpty() ? message : statusMessage);
        emit operationCompleted(success, message);
        emit operationFinished(operationId, success, message);
    }

    void BranchManagerViewModel::setConflicts(const std::vector<std::string>& files)
    {
        m_conflictedFiles.clear();
        for (const auto& f : files)
        {
            m_conflictedFiles.append(QString::fromStdString(f));
        }
        emit conflictsChanged();
    }

    void BranchManagerViewModel::setRepositoryPath(const QString& path)
    {
        if (path.isEmpty())
        {
            ++m_generation;
            m_repoPath.clear();
            m_currentBranch.clear();
            m_headSha.clear();
            m_followHeadPending = false;
            m_remoteUrl.clear();
            m_isDetached = false;
            m_branchListModel.setBranches({});
            m_commitHistoryModel.setCommits({});
            emit currentBranchChanged();
            emit remoteUrlChanged();
            return;
        }

        const std::filesystem::path newPath(path.toStdString());
        if (newPath != m_repoPath)
        {
            // Results still in flight for the previous repository must be discarded.
            ++m_generation;
            m_repoPath = newPath;
            m_commitFilesRequest.invalidate();
            m_fileHistoryRequest.invalidate();
            m_repositoryFilesRequest.invalidate();
            m_headSha.clear(); // another repository's HEAD is not a "move"
            m_followHeadPending = false;
        }
        refresh();
    }

    void BranchManagerViewModel::refresh()
    {
        // Coalesce: every refresh request issued during the same event-loop iteration
        // (operation result, branchSwitched -> refreshActiveTab, fs-watcher batch, ...)
        // results in a single reload of branches, history and remotes.
        if (m_repoPath.empty() || m_refreshScheduled)
        {
            return;
        }
        m_refreshScheduled = true;
        QMetaObject::invokeMethod(this, [this]()
        {
            m_refreshScheduled = false;
            startBackgroundRefresh();
        }, Qt::QueuedConnection);
    }

    void BranchManagerViewModel::startBackgroundRefresh()
    {
        if (m_repoPath.empty())
        {
            return;
        }
        if (m_refreshRunning)
        {
            // State may have changed after the running reload started: run once more when it ends.
            m_refreshAgain = true;
            return;
        }

        m_refreshRunning = true;
        updateBusy();

        const auto repoPath = m_repoPath;
        const auto fileHistoryPath = m_fileHistoryPath.toStdString();
        const auto generation = m_generation;

        m_executor->run(this,
                        [this, repoPath, fileHistoryPath]()
                        {
                            // Worker thread: loadSnapshot only reads through the services.
                            return loadSnapshot(repoPath, fileHistoryPath);
                        },
                        [this, generation](RefreshSnapshot snapshot)
                        {
                            m_refreshRunning = false;
                            if (generation == m_generation)
                            {
                                applySnapshot(snapshot);
                            }
                            if (m_refreshAgain)
                            {
                                m_refreshAgain = false;
                                startBackgroundRefresh();
                            }
                            updateBusy();
                        });
    }

    BranchManagerViewModel::RefreshSnapshot
    BranchManagerViewModel::loadSnapshot(const std::filesystem::path& repoPath,
                                         const std::string& fileHistoryPath) const
    {
        // Runs on the worker thread: only git reads here, no Qt model access.
        RefreshSnapshot snapshot;

        if (auto branchRes = m_branchService.listBranches(repoPath))
        {
            snapshot.branches = std::move(*branchRes);
        }

        // Complete commit history across all branches with date ordering (keeps commits at their creation row)
        git::bo::CommitLogOptions logOpts;
        logOpts.maxCount = 0; // 0 = unlimited (complete history)
        logOpts.revisionRange = std::string("--all");
        logOpts.dateOrder = true;
        if (!fileHistoryPath.empty())
        {
            logOpts.followRenames = true;
            logOpts.pathFilter = fileHistoryPath;
        }
        if (auto logRes = m_historyService.getCommitHistory(repoPath, logOpts))
        {
            snapshot.commits = std::move(*logRes);
        }

        if (auto remotesRes = m_remoteMergeService.listRemotes(repoPath))
        {
            snapshot.remotes = std::move(*remotesRes);
        }

        return snapshot;
    }

    void BranchManagerViewModel::applySnapshot(RefreshSnapshot& snapshot)
    {
        // 1. Branches and current branch / detached state
        if (snapshot.branches)
        {
            m_branchListModel.setBranches(*snapshot.branches);

            QString newCurrent;
            bool detached = false;
            for (const auto& b : *snapshot.branches)
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

        // 2. Commit history
        if (snapshot.commits)
        {
            m_commitHistoryModel.setCommits(std::move(*snapshot.commits));
            snapshot.commits.reset();

            // A filtered history (file history mode) may not contain HEAD: keep the last known one
            // (and any pending follow request) until a reload that does.
            const QString newHead = m_commitHistoryModel.headSha();
            if (!newHead.isEmpty())
            {
                const bool moved = !m_headSha.isEmpty() && newHead != m_headSha;
                m_headSha = newHead;
                if (moved || m_followHeadPending)
                {
                    m_followHeadPending = false;
                    emit followHeadRequested(newHead);
                }
            }
        }

        // 3. Remote URL (prefer "origin", otherwise the first remote)
        if (snapshot.remotes)
        {
            QString url;
            for (const auto& r : *snapshot.remotes)
            {
                if (r.name == "origin")
                {
                    url = QString::fromStdString(r.pushUrl.empty() ? r.fetchUrl : r.pushUrl);
                    break;
                }
            }
            if (url.isEmpty() && !snapshot.remotes->empty())
            {
                const auto& first = snapshot.remotes->front();
                url = QString::fromStdString(first.pushUrl.empty() ? first.fetchUrl : first.pushUrl);
            }
            if (m_remoteUrl != url)
            {
                m_remoteUrl = url;
                emit remoteUrlChanged();
            }
        }
    }

    bool BranchManagerViewModel::waitForIdle(int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (m_refreshScheduled || m_refreshRunning || m_pendingOperations > 0)
        {
            if (timer.elapsed() > timeoutMs)
            {
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
        return true;
    }

    void BranchManagerViewModel::setBusy(bool busy)
    {
        m_syncBusy = busy;
        updateBusy();
    }

    void BranchManagerViewModel::updateBusy()
    {
        const bool busy = m_syncBusy || m_refreshRunning || m_pendingOperations > 0;
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

    namespace
    {
        // Worker-safe: QVariant containers are implicitly shared and safe to build off the GUI thread.
        template <typename Items>
        QVariantList commitFilesToVariantList(const Items& items)
        {
            QVariantList list;
            list.reserve(static_cast<qsizetype>(items.size()));
            for (const auto& item : items)
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
    } // namespace

    void BranchManagerViewModel::requestCommitFiles(const QString& commitSha)
    {
        if (m_repoPath.empty() || commitSha.isEmpty())
        {
            m_commitFilesRequest.invalidate();
            return;
        }

        const auto id = m_commitFilesRequest.next();
        auto& service = m_historyService;
        m_readExecutor->run(this,
                            [&service, isCurrent = m_commitFilesRequest.guard(id), repoPath = m_repoPath,
                                sha = commitSha.toStdString()]() -> std::optional<QVariantList>
                            {
                                if (!isCurrent())
                                {
                                    return std::nullopt; // another commit was selected meanwhile
                                }
                                auto res = service.getCommitFiles(repoPath, sha);
                                return res ? commitFilesToVariantList(*res) : QVariantList{};
                            },
                            [this, id, commitSha](std::optional<QVariantList> files)
                            {
                                if (m_commitFilesRequest.isCurrent(id) && files)
                                {
                                    emit commitFilesLoaded(commitSha, *files);
                                }
                            });
    }

    bool BranchManagerViewModel::checkoutBranch(const QString& branchName)
    {
        if (m_repoPath.empty() || branchName.isEmpty())
        {
            return false;
        }

        struct Outcome
        {
            QString error;                           // empty on success
            std::optional<RefreshSnapshot> snapshot; // reloaded state after a successful checkout
        };

        auto& branchService = m_branchService;
        const auto repoPath = m_repoPath;
        const auto fileHistoryPath = m_fileHistoryPath.toStdString();
        const auto generation = m_generation;
        const git::bo::CheckoutOptions opts{.branchOrCommit = branchName.toStdString()};

        // Checkout + reload run on the worker thread so the UI never blocks on git.
        runOperation(
            QStringLiteral("Checkout di %1 in corso...").arg(branchName),
            [this, &branchService, repoPath, fileHistoryPath, opts]()
            {
                Outcome outcome;
                if (auto res = branchService.checkoutBranch(repoPath, opts); !res)
                {
                    outcome.error = QString::fromStdString(res.error().message);
                }
                else
                {
                    outcome.snapshot = loadSnapshot(repoPath, fileHistoryPath);
                }
                return outcome;
            },
            [this, generation, branchName](int id, bool sameRepo, Outcome outcome)
            {
                if (!outcome.error.isEmpty())
                {
                    completeOperation(id, false, outcome.error);
                    return;
                }
                if (sameRepo && generation == m_generation && outcome.snapshot)
                {
                    m_followHeadPending = true;
                    applySnapshot(*outcome.snapshot);
                }

                // Report what git actually did: a remote name may have been resolved to (or created as)
                // a local branch, while tags and SHAs intentionally leave HEAD detached.
                const QString message = (m_isDetached || m_currentBranch.isEmpty())
                                            ? QStringLiteral("Checkout di %1 completato (HEAD detached)").arg(branchName)
                                            : QStringLiteral("Passato al branch %1").arg(m_currentBranch);
                emit branchSwitched(m_currentBranch.isEmpty() ? branchName : m_currentBranch);
                completeOperation(id, true, message);
            });
        return true;
    }

    bool BranchManagerViewModel::createBranch(const QString& branchName, bool checkoutAfterCreate,
                                              const QString& startPoint)
    {
        if (m_repoPath.empty() || branchName.isEmpty())
        {
            return false;
        }

        auto& service = m_branchService;
        const auto repoPath = m_repoPath;
        const git::bo::CreateBranchOptions opts{
            .name = branchName.toStdString(),
            .startPoint = startPoint.isEmpty() ? std::nullopt : std::make_optional(startPoint.toStdString()),
            .checkoutAfterCreate = checkoutAfterCreate
        };

        runOperation(
            QStringLiteral("Creazione del branch %1...").arg(branchName),
            [&service, repoPath, opts]() { return service.createBranch(repoPath, opts); },
            [this, branchName, checkoutAfterCreate](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    m_followHeadPending = m_followHeadPending || checkoutAfterCreate;
                    refresh();
                    if (checkoutAfterCreate)
                    {
                        emit branchSwitched(branchName);
                    }
                }
                completeOperation(id, true, QStringLiteral("Branch %1 creato").arg(branchName),
                                  QStringLiteral("Branch %1 creato con successo").arg(branchName));
            });
        return true;
    }

    bool BranchManagerViewModel::deleteBranch(const QString& branchName, bool force)
    {
        if (m_repoPath.empty() || branchName.isEmpty())
        {
            return false;
        }

        auto& service = m_branchService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Eliminazione del branch %1...").arg(branchName),
            [&service, repoPath, name = branchName.toStdString(), force]() { return service.deleteBranch(repoPath, name, force); },
            [this, branchName](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    refresh();
                }
                completeOperation(id, true, QStringLiteral("Branch %1 eliminato").arg(branchName),
                                  QStringLiteral("Branch %1 eliminato").arg(branchName));
            });
        return true;
    }

    bool BranchManagerViewModel::renameBranch(const QString& oldName, const QString& newName)
    {
        if (m_repoPath.empty() || oldName.isEmpty() || newName.isEmpty())
        {
            return false;
        }

        auto& service = m_branchService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Ridenominazione di %1 in %2...").arg(oldName, newName),
            [&service, repoPath, from = oldName.toStdString(), to = newName.toStdString()]() { return service.renameBranch(repoPath, from, to); },
            [this, newName](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    refresh();
                }
                completeOperation(id, true, QStringLiteral("Branch rinominato in %1").arg(newName),
                                  QStringLiteral("Branch rinominato in %1").arg(newName));
            });
        return true;
    }

    bool BranchManagerViewModel::mergeBranch(const QString& sourceBranch, bool noFf)
    {
        if (m_repoPath.empty() || sourceBranch.isEmpty())
        {
            return false;
        }

        auto& service = m_remoteMergeService;
        const auto repoPath = m_repoPath;
        const git::bo::MergeOptions opts{
            .sourceBranchOrCommit = sourceBranch.toStdString(),
            .noFastForward = noFf
        };
        runOperation(
            QStringLiteral("Merge di %1 in corso...").arg(sourceBranch),
            [&service, repoPath, opts]() { return service.mergeBranch(repoPath, opts); },
            [this, sourceBranch](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (res->status == git::bo::MergeStatus::Conflict)
                {
                    if (sameRepo)
                    {
                        setConflicts(res->conflictedFiles);
                        refresh(); // the target branch may have been checked out first
                    }
                    completeOperation(id, false, QStringLiteral("Conflitti di merge rilevati"),
                                      QStringLiteral("Conflitti di merge rilevati in %1 file")
                                      .arg(res->conflictedFiles.size()));
                    return;
                }
                if (sameRepo)
                {
                    setConflicts({});
                    m_followHeadPending = true; // select HEAD once the reload lands, even if it did not move
                    refresh();
                }
                completeOperation(id, true, QStringLiteral("Merge completato"),
                                  QStringLiteral("Merge di %1 completato con successo").arg(sourceBranch));
            });
        return true;
    }

    bool BranchManagerViewModel::abortMerge()
    {
        if (m_repoPath.empty())
        {
            return false;
        }

        auto& service = m_remoteMergeService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Annullamento del merge in corso..."),
            [&service, repoPath]() { return service.abortMerge(repoPath); },
            [this](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    setConflicts({});
                    m_followHeadPending = true; // select HEAD once the reload lands, even if it did not move
                    refresh();
                }
                completeOperation(id, true, QStringLiteral("Merge annullato"),
                                  QStringLiteral("Merge annullato con successo"));
            });
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
        m_followHeadPending = true; // select HEAD once the reload lands, even if it did not move
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

        auto& service = m_remoteMergeService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Aggiornamento dell'URL del remote %1...").arg(remoteName),
            [&service, repoPath, remote = remoteName.toStdString(), url = newUrl.toStdString()]()
            {
                return service.setRemoteUrl(repoPath, remote, url);
            },
            [this, remoteName, newUrl](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    m_remoteUrl = newUrl;
                    emit remoteUrlChanged();
                }
                completeOperation(id, true,
                                  QStringLiteral("URL del remote %1 aggiornato con successo").arg(remoteName),
                                  QStringLiteral("URL del remote %1 aggiornato").arg(remoteName));
            });
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

        auto& service = m_branchService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Reset al commit %1 (%2) in corso...").arg(commitSha.left(7), mode),
            [&service, repoPath, sha = commitSha.toStdString(), resetMode = mode.toStdString()]() { return service.resetCommit(repoPath, sha, resetMode); },
            [this, commitSha, mode](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    m_followHeadPending = true; // select HEAD once the reload lands, even if it did not move
                    refresh();
                    emit branchSwitched(m_currentBranch);
                }
                completeOperation(id, true, QStringLiteral("Reset al commit %1 completato con successo").arg(commitSha.left(7)),
                                  QStringLiteral("Reset al commit %1 (%2) completato").arg(commitSha.left(7), mode));
            });
        return true;
    }

    bool BranchManagerViewModel::revertCommit(const QString& commitSha)
    {
        if (m_repoPath.empty() || commitSha.isEmpty())
        {
            return false;
        }

        auto& service = m_branchService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Revert del commit %1 in corso...").arg(commitSha.left(7)),
            [&service, repoPath, sha = commitSha.toStdString()]() { return service.revertCommit(repoPath, sha); },
            [this, commitSha](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    m_followHeadPending = true; // select HEAD once the reload lands, even if it did not move
                    refresh();
                    emit branchSwitched(m_currentBranch);
                }
                completeOperation(id, true, QStringLiteral("Revert del commit %1 completato con successo").arg(commitSha.left(7)),
                                  QStringLiteral("Revert del commit %1 completato").arg(commitSha.left(7)));
            });
        return true;
    }

    bool BranchManagerViewModel::rewordCommit(const QString& commitSha, const QString& newMessage)
    {
        if (m_repoPath.empty() || commitSha.isEmpty())
        {
            return false;
        }

        auto& service = m_branchService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Modifica del messaggio del commit %1 in corso...").arg(commitSha.left(7)),
            [&service, repoPath, sha = commitSha.toStdString(), message = newMessage.toStdString()]()
            {
                return service.rewordCommit(repoPath, sha, message);
            },
            [this, commitSha](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                const QString newSha = QString::fromStdString(*res);
                if (sameRepo)
                {
                    refresh();
                    emit branchSwitched(m_currentBranch);
                    emit commitReworded(commitSha, newSha);
                }
                completeOperation(id, true,
                                  QStringLiteral("Messaggio del commit %1 aggiornato con successo").arg(commitSha.left(7)),
                                  QStringLiteral("Messaggio del commit aggiornato (%1 → %2)")
                                  .arg(commitSha.left(7), newSha.left(7)));
            });
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

        auto* service = m_rebaseService;
        const auto repoPath = m_repoPath;
        const git::bo::CherryPickOptions opts{.commitSha = commitSha.toStdString()};
        runOperation(
            QStringLiteral("Cherry-pick del commit %1 in corso...").arg(commitSha.left(7)),
            [service, repoPath, opts]() { return service->cherryPick(repoPath, opts); },
            [this, commitSha](int id, bool sameRepo, auto res)
            {
                if (!res || !res->success)
                {
                    completeOperation(id, false, res ? QString::fromStdString(res->message)
                                                     : QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    m_followHeadPending = true; // select HEAD once the reload lands, even if it did not move
                    refresh();
                    emit branchSwitched(m_currentBranch);
                }
                const QString message =
                    QStringLiteral("Cherry-pick del commit %1 completato con successo").arg(commitSha.left(7));
                completeOperation(id, true, message);
            });
        return true;
    }

    bool BranchManagerViewModel::createTag(const QString& tagName, const QString& targetCommit, const QString& message)
    {
        if (m_repoPath.empty() || tagName.trimmed().isEmpty())
        {
            return false;
        }

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

        auto& service = m_historyService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Creazione del tag %1 in corso...").arg(tagName),
            [&service, repoPath, opts]() { return service.createTag(repoPath, opts); },
            [this, tagName](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    refresh();
                }
                completeOperation(id, true, QStringLiteral("Tag %1 creato con successo").arg(tagName));
            });
        return true;
    }

    bool BranchManagerViewModel::deleteTag(const QString& tagName)
    {
        if (m_repoPath.empty() || tagName.trimmed().isEmpty())
        {
            return false;
        }

        auto& service = m_historyService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Eliminazione del tag %1 in corso...").arg(tagName),
            [&service, repoPath, tag = tagName.trimmed().toStdString()]() { return service.deleteTag(repoPath, tag); },
            [this, tagName](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    refresh();
                }
                completeOperation(id, true, QStringLiteral("Tag %1 eliminato con successo").arg(tagName),
                                  QStringLiteral("Tag %1 eliminato").arg(tagName));
            });
        return true;
    }

    bool BranchManagerViewModel::createWorktree(const QString& targetPath, const QString& commitOrBranch)
    {
        if (m_repoPath.empty() || targetPath.trimmed().isEmpty())
        {
            return false;
        }

        auto& service = m_branchService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Creazione del worktree in %1...").arg(targetPath),
            [&service, repoPath, path = targetPath.toStdString(), ref = commitOrBranch.trimmed().toStdString()]()
            {
                return service.createWorktree(repoPath, path, ref);
            },
            [this, targetPath](int id, bool, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                completeOperation(id, true, QStringLiteral("Worktree creato con successo in %1").arg(targetPath));
            });
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

        git::bo::RebaseOptions opts{
            .upstream = targetBranch.toStdString(),
            .autostash = true
        };
        if (!m_currentBranch.isEmpty())
        {
            opts.branch = m_currentBranch.toStdString();
        }

        auto* service = m_rebaseService;
        const auto repoPath = m_repoPath;
        const QString switchedTo; // the current branch
        const QString okMessage = QStringLiteral("Rebase su %1 completato con successo").arg(targetBranch);
        runOperation(
            QStringLiteral("Rebase su %1 in corso...").arg(targetBranch),
            [service, repoPath, opts]() { return service->startRebase(repoPath, opts); },
            [this, switchedTo, okMessage](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (res->status == git::bo::RebaseStatus::Conflict)
                {
                    if (sameRepo)
                    {
                        setConflicts(res->conflictedFiles);
                        refresh();
                    }
                    completeOperation(id, false, QStringLiteral("Conflitti rilevati durante il rebase"));
                    return;
                }
                if (sameRepo)
                {
                    m_followHeadPending = true; // select HEAD once the reload lands, even if it did not move
                    refresh();
                    emit branchSwitched(switchedTo.isEmpty() ? m_currentBranch : switchedTo);
                }
                completeOperation(id, true, okMessage);
            });
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

        const git::bo::RebaseOptions opts{
            .upstream = targetBranch.toStdString(),
            .branch = sourceBranch.toStdString(),
            .autostash = true
        };

        auto* service = m_rebaseService;
        const auto repoPath = m_repoPath;
        const QString switchedTo = sourceBranch;
        const QString okMessage =
            QStringLiteral("Rebase di %1 su %2 completato con successo").arg(sourceBranch, targetBranch);
        runOperation(
            QStringLiteral("Rebase di %1 su %2 in corso...").arg(sourceBranch, targetBranch),
            [service, repoPath, opts]() { return service->startRebase(repoPath, opts); },
            [this, switchedTo, okMessage](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (res->status == git::bo::RebaseStatus::Conflict)
                {
                    if (sameRepo)
                    {
                        setConflicts(res->conflictedFiles);
                        refresh();
                    }
                    completeOperation(id, false, QStringLiteral("Conflitti rilevati durante il rebase"));
                    return;
                }
                if (sameRepo)
                {
                    m_followHeadPending = true; // select HEAD once the reload lands, even if it did not move
                    refresh();
                    emit branchSwitched(switchedTo.isEmpty() ? m_currentBranch : switchedTo);
                }
                completeOperation(id, true, okMessage);
            });
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

        // Checkout of the target, then merge: one task, so nothing can run in between.
        using MergeOutcome = std::expected<git::bo::MergeResult, git::ports::GitOpsError>;
        auto& branchService = m_branchService;
        auto& mergeService = m_remoteMergeService;
        const auto repoPath = m_repoPath;
        const git::bo::CheckoutOptions checkout{.branchOrCommit = effectiveTarget.toStdString()};
        const git::bo::MergeOptions opts{.sourceBranchOrCommit = sourceBranch.toStdString()};
        runOperation(
            QStringLiteral("Checkout di %1 e merge di %2 in corso...").arg(effectiveTarget, sourceBranch),
            [&branchService, &mergeService, repoPath, checkout, opts]() -> MergeOutcome
            {
                if (auto res = branchService.checkoutBranch(repoPath, checkout); !res)
                {
                    return std::unexpected(res.error());
                }
                return mergeService.mergeBranch(repoPath, opts);
            },
            [this, sourceBranch](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (res->status == git::bo::MergeStatus::Conflict)
                {
                    if (sameRepo)
                    {
                        setConflicts(res->conflictedFiles);
                        refresh(); // the target branch may have been checked out first
                    }
                    completeOperation(id, false, QStringLiteral("Conflitti di merge rilevati"),
                                      QStringLiteral("Conflitti di merge rilevati in %1 file")
                                      .arg(res->conflictedFiles.size()));
                    return;
                }
                if (sameRepo)
                {
                    setConflicts({});
                    m_followHeadPending = true; // select HEAD once the reload lands, even if it did not move
                    refresh();
                }
                completeOperation(id, true, QStringLiteral("Merge completato"),
                                  QStringLiteral("Merge di %1 completato con successo").arg(sourceBranch));
            });
        return true;
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
        auto& service = m_branchService;
        const auto repoPath = m_repoPath;
        runOperation(
            QStringLiteral("Fast-forward di '%1' a '%2' in corso...").arg(target, sourceBranch),
            [&service, repoPath, t = target.toStdString(), src = sourceBranch.toStdString()]()
            {
                return service.fastForwardBranch(repoPath, t, src);
            },
            [this, target, sourceBranch](int id, bool sameRepo, auto res)
            {
                if (!res)
                {
                    completeOperation(id, false, QString::fromStdString(res.error().message));
                    return;
                }
                if (sameRepo)
                {
                    const bool isCurrent = m_currentBranch == target;
                    if (isCurrent)
                    {
                        m_followHeadPending = true;
                    }
                    refresh();
                    if (isCurrent)
                    {
                        emit branchSwitched(m_currentBranch);
                    }
                }
                completeOperation(id, true,
                                  QStringLiteral("Fast-forward di '%1' a '%2' completato con successo")
                                  .arg(target, sourceBranch),
                                  QStringLiteral("'%1' aggiornato a '%2' (fast-forward)").arg(target, sourceBranch));
            });
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

        // `git log --follow` over the whole history can take hundreds of ms on large repositories.
        const auto id = m_fileHistoryRequest.next();
        ++m_pendingOperations;
        updateBusy();
        auto& service = m_historyService;
        const auto repoPath = m_repoPath;
        m_readExecutor->run(this,
                            [&service, isCurrent = m_fileHistoryRequest.guard(id), repoPath, logOpts]()
                            {
                                using Result = decltype(service.getCommitHistory(repoPath, logOpts));
                                if (!isCurrent())
                                {
                                    return std::optional<Result>{}; // superseded before it ran
                                }
                                return std::optional<Result>{service.getCommitHistory(repoPath, logOpts)};
                            },
                            [this, id, repoPath, cleanPath](auto fetched)
                            {
                                --m_pendingOperations;
                                // Stale: another file was picked, history mode was left or the repo changed.
                                if (fetched && m_fileHistoryRequest.isCurrent(id) && repoPath == m_repoPath)
                                {
                                    auto& logRes = *fetched;
                                    if (logRes)
                                    {
                                        ++m_generation; // drop background reloads computed with the previous filter
                                        m_fileHistoryPath = cleanPath;
                                        m_commitHistoryModel.setWip(false, 0);
                                        m_commitHistoryModel.setCommits(std::move(*logRes));
                                        emit fileHistoryPathChanged();
                                    }
                                    else
                                    {
                                        setStatusMessage(QString::fromStdString(logRes.error().message));
                                    }
                                }
                                updateBusy();
                            });
        return true;
    }

    void BranchManagerViewModel::clearFileHistory()
    {
        m_fileHistoryRequest.invalidate(); // a file history still loading must not land afterwards
        if (m_fileHistoryPath.isEmpty())
        {
            return;
        }
        ++m_generation; // drop background reloads computed with the file filter
        m_fileHistoryPath.clear();
        emit fileHistoryPathChanged();
        refresh();
    }

    void BranchManagerViewModel::requestRepositoryFiles()
    {
        if (m_repoPath.empty())
        {
            emit repositoryFilesLoaded({});
            return;
        }

        const auto id = m_repositoryFilesRequest.next();
        auto& service = m_historyService;
        m_readExecutor->run(this,
                            [&service, repoPath = m_repoPath]()
                            {
                                QStringList list;
                                if (auto res = service.listTrackedFiles(repoPath))
                                {
                                    list.reserve(static_cast<qsizetype>(res->size()));
                                    for (const auto& f : *res)
                                    {
                                        list.append(QString::fromStdString(f));
                                    }
                                }
                                return list;
                            },
                            [this, id](QStringList files)
                            {
                                if (m_repositoryFilesRequest.isCurrent(id))
                                {
                                    emit repositoryFilesLoaded(files);
                                }
                            });
    }
} // namespace gf::desktop
