#include "desktop/viewmodels/conflict_resolver_viewmodel.hpp"

#include "core/modules/git/bl/git_conflict_service.hpp"
#include "core/modules/git/bl/conflict_marker_parser.hpp"
#include "desktop/services/serial_git_executor.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

namespace gf::desktop
{
    ConflictResolverViewModel::ConflictResolverViewModel(gf::git::bl::GitConflictService& conflictService,
                                                         QObject* parent)
        : QObject(parent)
          , m_conflictService(conflictService)
          , m_hunksModel(this)
          , m_ownedExecutor(std::make_unique<SerialGitExecutor>())
    {
        m_executor = m_ownedExecutor.get();
        connectModel();
    }

    ConflictResolverViewModel::ConflictResolverViewModel(gf::git::bl::GitConflictService& conflictService,
                                                         SerialGitExecutor& executor,
                                                         QObject* parent)
        : QObject(parent)
          , m_conflictService(conflictService)
          , m_hunksModel(this)
          , m_executor(&executor)
    {
        connectModel();
    }

    ConflictResolverViewModel::~ConflictResolverViewModel() = default;

    void ConflictResolverViewModel::connectModel()
    {
        connect(&m_hunksModel, &ConflictHunkListModel::resolutionChanged, this, [this]()
        {
            emit resolvedHunksChanged();
            emit allResolvedChanged();
        });
        connect(&m_hunksModel, &ConflictHunkListModel::countChanged, this, [this]()
        {
            emit totalHunksChanged();
            emit resolvedHunksChanged();
            emit allResolvedChanged();
        });
    }

    void ConflictResolverViewModel::setRepoPath(const QString& path)
    {
        if (m_repoPath == path)
        {
            return;
        }
        m_repoPath = path;
        emit repoPathChanged();
        clear();
    }

    void ConflictResolverViewModel::setBusy(bool busy)
    {
        if (m_isBusy == busy)
        {
            return;
        }
        m_isBusy = busy;
        emit isBusyChanged();
    }

    void ConflictResolverViewModel::setError(const QString& message)
    {
        m_errorMessage = message;
        emit errorMessageChanged();
        emit hasErrorChanged();
    }

    void ConflictResolverViewModel::clearError()
    {
        if (!m_errorMessage.isEmpty())
        {
            m_errorMessage.clear();
            emit errorMessageChanged();
            emit hasErrorChanged();
        }
    }

    void ConflictResolverViewModel::clear()
    {
        m_currentFilePath.clear();
        m_rawOriginalContent.clear();
        m_isBinary = false;
        m_hunksModel.clear();
        clearError();
        emit currentFilePathChanged();
        emit isBinaryChanged();
    }

    bool ConflictResolverViewModel::loadFile(const QString& relativeFilePath)
    {
        if (m_repoPath.isEmpty())
        {
            setError("Nessun repository selezionato");
            return false;
        }

        if (relativeFilePath.isEmpty())
        {
            setError("Percorso del file non specificato");
            return false;
        }

        setBusy(true);
        clearError();

        const std::filesystem::path path(m_repoPath.toStdString());
        auto result = m_conflictService.getConflictFile(path, relativeFilePath.toStdString());

        if (!result)
        {
            setError(QString::fromStdString(result.error().message));
            setBusy(false);
            return false;
        }

        m_currentFilePath = relativeFilePath;
        m_rawOriginalContent = result->fullContentWithMarkers;
        m_isBinary = result->isBinary;
        m_hunksModel.setHunks(std::move(result->hunks));

        setBusy(false);
        emit currentFilePathChanged();
        emit isBinaryChanged();
        return true;
    }

    void ConflictResolverViewModel::resolveHunk(int hunkIndex, int side, const QString& customText)
    {
        if (side < 0 || side > 3)
        {
            return;
        }
        const auto chosenSide = static_cast<gf::git::bo::ConflictSide>(side);
        m_hunksModel.resolveHunk(hunkIndex, chosenSide, customText);
    }

    bool ConflictResolverViewModel::applyResolution(bool autoStage)
    {
        if (m_repoPath.isEmpty() || m_currentFilePath.isEmpty())
        {
            setError("Nessun file in conflitto caricato");
            return false;
        }

        if (!m_hunksModel.allResolved())
        {
            setError("Non tutti i conflitti sono stati risolti");
            return false;
        }

        setBusy(true);
        clearError();

        auto reconstructed = gf::git::bl::ConflictMarkerParser::reconstruct(m_rawOriginalContent, m_hunksModel.hunks());
        if (!reconstructed)
        {
            setError(QString::fromStdString(reconstructed.error()));
            setBusy(false);
            return false;
        }

        const std::filesystem::path path(m_repoPath.toStdString());
        gf::git::bo::ApplyConflictResolutionOptions opts{
            .filePath = m_currentFilePath.toStdString(),
            .resolvedContent = *reconstructed,
            .autoStage = autoStage
        };

        // Writing the file and `git add` run on the git executor.
        auto& service = m_conflictService;
        const QString resolvedPath = m_currentFilePath;
        const QString repoPath = m_repoPath;
        m_executor->run(this,
                        [&service, path, opts]() { return service.applyConflictResolution(path, opts); },
                        [this, resolvedPath, repoPath](auto applyRes)
                        {
                            setBusy(false);
                            if (!applyRes)
                            {
                                setError(QString::fromStdString(applyRes.error().message));
                                return;
                            }
                            // Clear only if the view still shows the file that was resolved.
                            if (m_repoPath == repoPath && m_currentFilePath == resolvedPath)
                            {
                                clear();
                            }
                            emit resolutionApplied(resolvedPath);
                        });
        return true;
    }

    bool ConflictResolverViewModel::waitForIdle(int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (m_isBusy)
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
} // namespace gf::desktop
