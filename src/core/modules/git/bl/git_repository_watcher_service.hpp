#pragma once

#include "core/modules/git/bo/fs_watcher_models.hpp"
#include "core/modules/git/ports/fs_watcher_port.hpp"
#include "core/modules/git/bl/fs_event_classifier.hpp"
#include "core/modules/git/bl/fs_event_debouncer.hpp"

#include <atomic>
#include <condition_variable>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace gf::git::bl {

using DebouncedBatchCallback = std::function<void(const bo::FsDebouncedBatch&)>;

class GitRepositoryWatcherService {
public:
    explicit GitRepositoryWatcherService(ports::IFsWatcherPort& watcherPort,
                                         FsEventDebouncer::Milliseconds quietPeriod = std::chrono::milliseconds(200),
                                         FsEventDebouncer::Milliseconds maxWindow = std::chrono::milliseconds(1000));
    ~GitRepositoryWatcherService();

    GitRepositoryWatcherService(const GitRepositoryWatcherService&) = delete;
    GitRepositoryWatcherService& operator=(const GitRepositoryWatcherService&) = delete;
    GitRepositoryWatcherService(GitRepositoryWatcherService&&) = delete;
    GitRepositoryWatcherService& operator=(GitRepositoryWatcherService&&) = delete;

    [[nodiscard]] std::expected<void, ports::FsWatcherError>
    startWatching(const std::filesystem::path& repoPath);

    [[nodiscard]] std::expected<void, ports::FsWatcherError>
    stopWatching(const std::filesystem::path& repoPath);

    [[nodiscard]] bool isWatching(const std::filesystem::path& repoPath) const;

    void setBatchCallback(DebouncedBatchCallback callback);

    void onRawEvent(const bo::FsRawEvent& event);

    void flushPending();
    void flushAll();

    void startAutoFlush(std::chrono::milliseconds checkInterval = std::chrono::milliseconds(50));
    void stopAutoFlush();

    [[nodiscard]] bool isAutoFlushRunning() const noexcept { return m_autoFlushRunning.load(); }
    [[nodiscard]] FsEventDebouncer& debouncer() noexcept { return m_debouncer; }

private:
    void autoFlushWorkerLoop(std::chrono::milliseconds checkInterval);

    ports::IFsWatcherPort& m_watcherPort;
    FsEventDebouncer m_debouncer;
    DebouncedBatchCallback m_batchCallback{};
    std::mutex m_callbackMutex;

    std::atomic<bool> m_autoFlushRunning{false};
    std::thread m_autoFlushThread{};
    std::condition_variable m_autoFlushCv{};
    std::mutex m_autoFlushMutex{};
};

} // namespace gf::git::bl
