#include "core/modules/git/bl/git_repository_watcher_service.hpp"

namespace gf::git::bl
{
    GitRepositoryWatcherService::GitRepositoryWatcherService(ports::IFsWatcherPort& watcherPort,
                                                             FsEventDebouncer::Milliseconds quietPeriod,
                                                             FsEventDebouncer::Milliseconds maxWindow)
        : m_watcherPort(watcherPort)
          , m_debouncer(quietPeriod, maxWindow)
    {
        m_watcherPort.setEventCallback([this](const bo::FsRawEvent& event)
        {
            onRawEvent(event);
        });
    }

    GitRepositoryWatcherService::~GitRepositoryWatcherService()
    {
        stopAutoFlush();
    }

    std::expected<void, ports::FsWatcherError>
    GitRepositoryWatcherService::startWatching(const std::filesystem::path& repoPath)
    {
        return m_watcherPort.watchRepository(repoPath);
    }

    std::expected<void, ports::FsWatcherError>
    GitRepositoryWatcherService::stopWatching(const std::filesystem::path& repoPath)
    {
        return m_watcherPort.unwatchRepository(repoPath);
    }

    bool GitRepositoryWatcherService::isWatching(const std::filesystem::path& repoPath) const
    {
        return m_watcherPort.isWatching(repoPath);
    }

    void GitRepositoryWatcherService::setBatchCallback(DebouncedBatchCallback callback)
    {
        std::lock_guard<std::mutex> lock(m_callbackMutex);
        m_batchCallback = std::move(callback);
    }

    void GitRepositoryWatcherService::onRawEvent(const bo::FsRawEvent& event)
    {
        auto classification = FsEventClassifier::classify(event.repositoryPath, event.changedPath);
        if (classification == bo::FsChangeClassification::Ignored)
        {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        m_debouncer.pushEvent(event, classification, now);
    }

    void GitRepositoryWatcherService::flushPending()
    {
        auto now = std::chrono::steady_clock::now();
        auto batches = m_debouncer.flushReadyBatches(now);

        if (batches.empty())
        {
            return;
        }

        DebouncedBatchCallback cbCopy;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            cbCopy = m_batchCallback;
        }

        if (cbCopy)
        {
            for (const auto& batch : batches)
            {
                cbCopy(batch);
            }
        }
    }

    void GitRepositoryWatcherService::flushAll()
    {
        auto batches = m_debouncer.flushAll();

        if (batches.empty())
        {
            return;
        }

        DebouncedBatchCallback cbCopy;
        {
            std::lock_guard<std::mutex> lock(m_callbackMutex);
            cbCopy = m_batchCallback;
        }

        if (cbCopy)
        {
            for (const auto& batch : batches)
            {
                cbCopy(batch);
            }
        }
    }

    void GitRepositoryWatcherService::startAutoFlush(std::chrono::milliseconds checkInterval)
    {
        if (m_autoFlushRunning.exchange(true))
        {
            return;
        }

        m_autoFlushThread = std::thread(&GitRepositoryWatcherService::autoFlushWorkerLoop, this, checkInterval);
    }

    void GitRepositoryWatcherService::stopAutoFlush()
    {
        if (!m_autoFlushRunning.exchange(false))
        {
            return;
        }

        m_autoFlushCv.notify_all();
        if (m_autoFlushThread.joinable())
        {
            m_autoFlushThread.join();
        }
    }

    void GitRepositoryWatcherService::autoFlushWorkerLoop(std::chrono::milliseconds checkInterval)
    {
        std::unique_lock<std::mutex> lock(m_autoFlushMutex);

        while (m_autoFlushRunning.load())
        {
            m_autoFlushCv.wait_for(lock, checkInterval, [this]()
            {
                return !m_autoFlushRunning.load();
            });

            if (!m_autoFlushRunning.load())
            {
                break;
            }

            flushPending();
        }
    }
} // namespace gf::git::bl
