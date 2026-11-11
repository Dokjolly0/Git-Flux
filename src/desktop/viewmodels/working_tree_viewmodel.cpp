#include "desktop/viewmodels/working_tree_viewmodel.hpp"

#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/shared/utf8_path.hpp"
#include "desktop/services/serial_git_executor.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <expected>
#include <filesystem>

namespace gf::desktop
{
    WorkingTreeViewModel::WorkingTreeViewModel(gf::git::bl::GitWorkingTreeService& statusService,
                                               gf::git::bl::GitStagingService& stagingService,
                                               QObject* parent)
        : QObject(parent)
          , m_statusService(statusService)
          , m_stagingService(stagingService)
          , m_stagedModel(this)
          , m_unstagedModel(this)
          , m_untrackedModel(this)
          , m_conflictedModel(this)
          , m_ownedExecutor(std::make_unique<SerialGitExecutor>())
    {
        m_executor = m_ownedExecutor.get();
    }

    WorkingTreeViewModel::WorkingTreeViewModel(gf::git::bl::GitWorkingTreeService& statusService,
                                               gf::git::bl::GitStagingService& stagingService,
                                               SerialGitExecutor& executor,
                                               QObject* parent)
        : QObject(parent)
          , m_statusService(statusService)
          , m_stagingService(stagingService)
          , m_stagedModel(this)
          , m_unstagedModel(this)
          , m_untrackedModel(this)
          , m_conflictedModel(this)
          , m_executor(&executor)
    {
    }

    WorkingTreeViewModel::~WorkingTreeViewModel() = default;

    void WorkingTreeViewModel::clearState()
    {
        m_stagedModel.clear();
        m_unstagedModel.clear();
        m_untrackedModel.clear();
        m_conflictedModel.clear();
        m_stagedCount = 0;
        m_unstagedCount = 0;
        m_untrackedCount = 0;
        m_conflictedCount = 0;
        m_isClean = true;
        m_branchName.clear();
        m_headOid.clear();
        m_aheadCount = 0;
        m_behindCount = 0;
        m_isDetached = false;
        m_isUnborn = false;
        m_lastRepoPath.clear();
        m_lastStatus = {};
        m_hasLastStatus = false;
    }

    bool WorkingTreeViewModel::refresh(const QString& repoPath)
    {
        if (repoPath.trimmed().isEmpty())
        {
            ++m_generation; // drop any status still being read for the previous repository
            m_requestedPath.clear();
            m_refreshQueued = false;
            clearState();
            emit statusChanged();
            return false;
        }

        m_requestedPath = repoPath;
        if (m_refreshInFlight)
        {
            // Coalesce bursts (poll timer, fs watcher, tab switch): one more read when this ends.
            m_refreshQueued = true;
            return true;
        }
        startRefresh();
        return true;
    }

    void WorkingTreeViewModel::startRefresh()
    {
        m_refreshInFlight = true;
        const QString path = m_requestedPath;
        const quint64 generation = m_generation;
        auto& service = m_statusService;

        m_executor->run(this,
                        [&service, path]()
                        {
                            return service.getWorkingTreeStatus(gf::core::utf8ToPath(path.toStdString()));
                        },
                        [this, path, generation](
                        std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError> result)
                        {
                            m_refreshInFlight = false;
                            // Stale: the view cleared or moved to another repository meanwhile.
                            if (generation == m_generation && path == m_requestedPath)
                            {
                                if (result)
                                {
                                    applyStatus(path, *result);
                                }
                                else
                                {
                                    emit operationFailed(QString::fromStdString(result.error().message));
                                }
                            }
                            if (m_refreshQueued)
                            {
                                m_refreshQueued = false;
                                if (!m_requestedPath.isEmpty())
                                {
                                    startRefresh();
                                }
                            }
                        });
    }

    void WorkingTreeViewModel::applyStatus(const QString& repoPath, const gf::git::bo::RepoWorkingTreeStatus& status)
    {
        if (m_hasLastStatus && m_lastRepoPath == repoPath && m_lastStatus == status)
        {
            emit refreshed(repoPath);
            return;
        }

        const bool sameRepo = m_hasLastStatus && (m_lastRepoPath == repoPath);
        if (!sameRepo || m_lastStatus.stagedFiles != status.stagedFiles)
        {
            m_stagedModel.setItems(status.stagedFiles);
        }
        if (!sameRepo || m_lastStatus.unstagedFiles != status.unstagedFiles)
        {
            m_unstagedModel.setItems(status.unstagedFiles);
        }
        if (!sameRepo || m_lastStatus.untrackedFiles != status.untrackedFiles)
        {
            m_untrackedModel.setItems(status.untrackedFiles);
        }
        if (!sameRepo || m_lastStatus.conflictedFiles != status.conflictedFiles)
        {
            m_conflictedModel.setItems(status.conflictedFiles);
        }

        m_stagedCount = static_cast<int>(status.stagedCount());
        m_unstagedCount = static_cast<int>(status.unstagedCount());
        m_untrackedCount = static_cast<int>(status.untrackedCount());
        m_conflictedCount = static_cast<int>(status.conflictedCount());
        m_isClean = status.isClean();

        m_branchName = QString::fromStdString(status.branch.headName);
        m_headOid = QString::fromStdString(status.branch.oid);
        m_aheadCount = status.branch.ahead;
        m_behindCount = status.branch.behind;
        m_isDetached = status.branch.detached;
        m_isUnborn = status.branch.unborn;

        m_lastRepoPath = repoPath;
        m_lastStatus = status;
        m_hasLastStatus = true;

        emit statusChanged();
        emit refreshed(repoPath);
    }

    void WorkingTreeViewModel::setPendingOperations(int count)
    {
        const bool wasBusy = m_isBusy;
        m_pendingOperations = count;
        m_isBusy = m_pendingOperations > 0;
        if (m_isBusy != wasBusy)
        {
            emit isBusyChanged();
        }
    }

    bool WorkingTreeViewModel::runMutation(const QString& repoPath, Mutation mutation)
    {
        if (repoPath.trimmed().isEmpty())
        {
            return false;
        }

        setPendingOperations(m_pendingOperations + 1);
        const quint64 generation = m_generation;
        m_executor->run(this,
                        std::move(mutation),
                        [this, repoPath, generation](const std::string& error)
                        {
                            setPendingOperations(m_pendingOperations - 1);
                            if (!error.empty())
                            {
                                emit operationFailed(QString::fromStdString(error));
                            }
                            // Re-read even after a failure (a partial operation may have changed
                            // files), unless the view was cleared or moved to another repository.
                            const bool stillShown = generation == m_generation
                                && (m_requestedPath.isEmpty() || m_requestedPath == repoPath);
                            if (stillShown)
                            {
                                m_hasLastStatus = false;
                                refresh(repoPath);
                            }
                        });
        return true;
    }

    bool WorkingTreeViewModel::stageFile(const QString& repoPath, const QString& filePath)
    {
        auto& staging = m_stagingService;
        const auto rPath = gf::core::utf8ToPath(repoPath.toStdString());
        const std::vector<std::string> files{filePath.toStdString()};
        return runMutation(repoPath, [&staging, rPath, files]() -> std::string
        {
            auto res = staging.stageFiles(rPath, files);
            return res ? std::string{} : res.error().message;
        });
    }

    bool WorkingTreeViewModel::stageAll(const QString& repoPath)
    {
        auto& staging = m_stagingService;
        const auto rPath = gf::core::utf8ToPath(repoPath.toStdString());
        return runMutation(repoPath, [&staging, rPath]() -> std::string
        {
            auto res = staging.stageAll(rPath);
            return res ? std::string{} : res.error().message;
        });
    }

    bool WorkingTreeViewModel::unstageFile(const QString& repoPath, const QString& filePath)
    {
        auto& staging = m_stagingService;
        const auto rPath = gf::core::utf8ToPath(repoPath.toStdString());
        const std::vector<std::string> files{filePath.toStdString()};
        return runMutation(repoPath, [&staging, rPath, files]() -> std::string
        {
            auto res = staging.unstageFiles(rPath, files);
            return res ? std::string{} : res.error().message;
        });
    }

    bool WorkingTreeViewModel::unstageAll(const QString& repoPath)
    {
        auto& staging = m_stagingService;
        const auto rPath = gf::core::utf8ToPath(repoPath.toStdString());
        return runMutation(repoPath, [&staging, rPath]() -> std::string
        {
            auto res = staging.unstageAll(rPath);
            return res ? std::string{} : res.error().message;
        });
    }

    bool WorkingTreeViewModel::discardFile(const QString& repoPath, const QString& filePath)
    {
        auto& staging = m_stagingService;
        const auto rPath = gf::core::utf8ToPath(repoPath.toStdString());
        const std::vector<std::string> files{filePath.toStdString()};
        return runMutation(repoPath, [&staging, rPath, files]() -> std::string
        {
            // 1. Unstage if staged, 2. discard working tree changes
            (void)staging.unstageFiles(rPath, files);
            auto res = staging.discardFiles(rPath, files);
            return res ? std::string{} : res.error().message;
        });
    }

    bool WorkingTreeViewModel::discardAll(const QString& repoPath)
    {
        // The file list is what the user sees now (collected on the GUI thread).
        std::vector<std::string> allFiles;
        for (const auto* model : {&m_stagedModel, &m_unstagedModel, &m_untrackedModel})
        {
            for (const auto& item : model->items())
            {
                allFiles.push_back(item.path);
            }
        }

        auto& staging = m_stagingService;
        const auto rPath = gf::core::utf8ToPath(repoPath.toStdString());
        return runMutation(repoPath, [&staging, rPath, allFiles]() -> std::string
        {
            (void)staging.unstageAll(rPath);
            if (allFiles.empty())
            {
                return {};
            }
            auto res = staging.discardFiles(rPath, allFiles);
            return res ? std::string{} : res.error().message;
        });
    }

    bool WorkingTreeViewModel::waitForIdle(int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (m_refreshInFlight || m_refreshQueued || m_pendingOperations > 0)
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

    QVariantList WorkingTreeViewModel::getStagedFiles() const
    {
        QVariantList list;
        const auto& items = m_stagedModel.items();
        list.reserve(static_cast<qsizetype>(items.size()));
        for (const auto& item : items)
        {
            QVariantMap map;
            map[QStringLiteral("path")] = QString::fromStdString(item.path);
            map[QStringLiteral("oldPath")] = QString::fromStdString(item.originalPath);
            QString statusChar;
            switch (item.kind)
            {
            case git::bo::FileChangeKind::Added:
            case git::bo::FileChangeKind::Untracked:
                statusChar = QStringLiteral("A");
                break;
            case git::bo::FileChangeKind::Deleted:
                statusChar = QStringLiteral("D");
                break;
            case git::bo::FileChangeKind::Renamed:
            case git::bo::FileChangeKind::Copied:
                statusChar = QStringLiteral("R");
                break;
            default:
                statusChar = QStringLiteral("M");
                break;
            }
            map[QStringLiteral("status")] = statusChar;
            map[QStringLiteral("kind")] = QString::fromLatin1(git::bo::fileChangeKindToString(item.kind).data());
            map[QStringLiteral("isStaged")] = true;
            map[QStringLiteral("additions")] = 0;
            map[QStringLiteral("deletions")] = 0;
            list.append(map);
        }
        return list;
    }

    QVariantList WorkingTreeViewModel::getUnstagedFiles() const
    {
        QVariantList list;
        auto appendItems = [&list](const std::vector<git::bo::WorkingTreeFileItem>& items, bool isUntracked)
        {
            for (const auto& item : items)
            {
                QVariantMap map;
                map[QStringLiteral("path")] = QString::fromStdString(item.path);
                map[QStringLiteral("oldPath")] = QString::fromStdString(item.originalPath);
                QString statusChar;
                if (isUntracked || item.kind == git::bo::FileChangeKind::Added || item.kind ==
                    git::bo::FileChangeKind::Untracked)
                {
                    statusChar = QStringLiteral("A");
                }
                else if (item.kind == git::bo::FileChangeKind::Deleted)
                {
                    statusChar = QStringLiteral("D");
                }
                else if (item.kind == git::bo::FileChangeKind::Renamed || item.kind == git::bo::FileChangeKind::Copied)
                {
                    statusChar = QStringLiteral("R");
                }
                else
                {
                    statusChar = QStringLiteral("M");
                }
                map[QStringLiteral("status")] = statusChar;
                map[QStringLiteral("kind")] = QString::fromLatin1(git::bo::fileChangeKindToString(item.kind).data());
                map[QStringLiteral("isStaged")] = false;
                map[QStringLiteral("additions")] = 0;
                map[QStringLiteral("deletions")] = 0;
                list.append(map);
            }
        };
        appendItems(m_unstagedModel.items(), false);
        appendItems(m_untrackedModel.items(), true);
        appendItems(m_conflictedModel.items(), false);
        return list;
    }

    QVariantList WorkingTreeViewModel::getAllWorkingFiles() const
    {
        QVariantList list = getStagedFiles();
        list.append(getUnstagedFiles());
        return list;
    }
} // namespace gf::desktop
