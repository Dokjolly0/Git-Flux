#include "desktop/viewmodels/stash_manager_viewmodel.hpp"

#include "core/modules/git/bl/git_stash_service.hpp"
#include "core/shared/utf8_path.hpp"
#include "desktop/services/serial_git_executor.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <expected>

namespace gf::desktop
{
    StashManagerViewModel::StashManagerViewModel(gf::git::bl::GitStashService& stashService,
                                                 QObject* parent)
        : QObject(parent)
          , m_stashService(stashService)
          , m_stashesModel(this)
          , m_ownedExecutor(std::make_unique<SerialGitExecutor>())
    {
        m_executor = m_ownedExecutor.get();
    }

    StashManagerViewModel::StashManagerViewModel(gf::git::bl::GitStashService& stashService,
                                                 SerialGitExecutor& executor,
                                                 QObject* parent)
        : QObject(parent)
          , m_stashService(stashService)
          , m_stashesModel(this)
          , m_executor(&executor)
    {
    }

    StashManagerViewModel::~StashManagerViewModel() = default;

    void StashManagerViewModel::setRepoPath(const QString& path)
    {
        if (m_repoPath == path)
        {
            return;
        }
        m_repoPath = path;
        ++m_generation; // results still in flight belong to the previous repository
        emit repoPathChanged();
        refresh();
    }

    void StashManagerViewModel::updateBusy()
    {
        const bool busy = m_pendingTasks > 0;
        if (m_isBusy == busy)
        {
            return;
        }
        m_isBusy = busy;
        emit isBusyChanged();
    }

    void StashManagerViewModel::setError(const QString& message)
    {
        m_errorMessage = message;
        emit errorMessageChanged();
        emit hasErrorChanged();
    }

    void StashManagerViewModel::clearError()
    {
        if (!m_errorMessage.isEmpty())
        {
            m_errorMessage.clear();
            emit errorMessageChanged();
            emit hasErrorChanged();
        }
    }

    void StashManagerViewModel::refresh()
    {
        if (m_repoPath.isEmpty())
        {
            m_stashesModel.clear();
            return;
        }

        clearError();
        ++m_pendingTasks;
        updateBusy();

        auto& service = m_stashService;
        const auto path = gf::core::utf8ToPath(m_repoPath.toStdString());
        const quint64 generation = m_generation;
        m_executor->run(this,
                        [&service, path]() { return service.listStashes(path); },
                        [this, generation](auto result)
                        {
                            --m_pendingTasks;
                            if (generation == m_generation)
                            {
                                if (result)
                                {
                                    m_stashesModel.setStashes(std::move(*result));
                                }
                                else
                                {
                                    setError(QString::fromStdString(result.error().message));
                                }
                            }
                            updateBusy();
                        });
    }

    bool StashManagerViewModel::runOperation(const QString& name, Operation operation)
    {
        if (m_repoPath.isEmpty())
        {
            setError("Nessun repository selezionato");
            return false;
        }

        clearError();
        ++m_pendingTasks;
        updateBusy();

        const quint64 generation = m_generation;
        m_executor->run(this, std::move(operation),
                        [this, name, generation](const std::string& error)
                        {
                            --m_pendingTasks;
                            if (!error.empty())
                            {
                                const QString message = QString::fromStdString(error);
                                if (generation == m_generation)
                                {
                                    setError(message);
                                }
                                emit operationFailed(name, message);
                            }
                            else
                            {
                                if (generation == m_generation)
                                {
                                    refresh();
                                }
                                emit operationSucceeded(name);
                            }
                            updateBusy();
                        });
        return true;
    }

    namespace
    {
        template <typename Result>
        std::string errorOf(const Result& result)
        {
            return result ? std::string{} : result.error().message;
        }
    } // namespace

    bool StashManagerViewModel::push(const QString& message, bool includeUntracked, bool keepIndex)
    {
        gf::git::bo::StashPushOptions opts;
        if (!message.trimmed().isEmpty())
        {
            opts.message = message.trimmed().toStdString();
        }
        opts.includeUntracked = includeUntracked;
        opts.keepIndex = keepIndex;

        auto& service = m_stashService;
        const auto path = gf::core::utf8ToPath(m_repoPath.toStdString());
        return runOperation("push", [&service, path, opts]() { return errorOf(service.stashPush(path, opts)); });
    }

    bool StashManagerViewModel::pop(int index, bool restoreIndex)
    {
        const gf::git::bo::StashApplyOptions opts{
            .stashIndex = static_cast<std::size_t>(index >= 0 ? index : 0),
            .restoreIndex = restoreIndex
        };
        auto& service = m_stashService;
        const auto path = gf::core::utf8ToPath(m_repoPath.toStdString());
        return runOperation("pop", [&service, path, opts]() { return errorOf(service.stashPop(path, opts)); });
    }

    bool StashManagerViewModel::apply(int index, bool restoreIndex)
    {
        const gf::git::bo::StashApplyOptions opts{
            .stashIndex = static_cast<std::size_t>(index >= 0 ? index : 0),
            .restoreIndex = restoreIndex
        };
        auto& service = m_stashService;
        const auto path = gf::core::utf8ToPath(m_repoPath.toStdString());
        return runOperation("apply", [&service, path, opts]() { return errorOf(service.stashApply(path, opts)); });
    }

    bool StashManagerViewModel::drop(int index)
    {
        const auto stashIndex = static_cast<std::size_t>(index >= 0 ? index : 0);
        auto& service = m_stashService;
        const auto path = gf::core::utf8ToPath(m_repoPath.toStdString());
        return runOperation("drop", [&service, path, stashIndex]()
        {
            return errorOf(service.stashDrop(path, stashIndex));
        });
    }

    bool StashManagerViewModel::clear()
    {
        auto& service = m_stashService;
        const auto path = gf::core::utf8ToPath(m_repoPath.toStdString());
        return runOperation("clear", [&service, path]() { return errorOf(service.stashClear(path)); });
    }

    bool StashManagerViewModel::waitForIdle(int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (m_pendingTasks > 0)
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
