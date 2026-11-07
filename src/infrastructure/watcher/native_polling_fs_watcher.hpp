#pragma once

#include "core/modules/git/ports/fs_watcher_port.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace gf::infra::watcher {

class NativePollingFsWatcher final : public gf::git::ports::IFsWatcherPort {
public:
    using Milliseconds = std::chrono::milliseconds;

    explicit NativePollingFsWatcher(Milliseconds pollInterval = Milliseconds(300));
    ~NativePollingFsWatcher() override;

    NativePollingFsWatcher(const NativePollingFsWatcher&) = delete;
    NativePollingFsWatcher& operator=(const NativePollingFsWatcher&) = delete;
    NativePollingFsWatcher(NativePollingFsWatcher&&) = delete;
    NativePollingFsWatcher& operator=(NativePollingFsWatcher&&) = delete;

    [[nodiscard]] std::expected<void, gf::git::ports::FsWatcherError>
    watchRepository(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<void, gf::git::ports::FsWatcherError>
    unwatchRepository(const std::filesystem::path& repoPath) override;

    [[nodiscard]] bool isWatching(const std::filesystem::path& repoPath) const override;

    void setEventCallback(gf::git::ports::FsRawEventCallback callback) override;

    void start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept { return m_running.load(); }

    void pollOnce();

private:
    struct RepoSnapshots {
        std::filesystem::path repoPath;
        std::filesystem::file_time_type headMtime{};
        std::filesystem::file_time_type indexMtime{};
        std::filesystem::file_time_type refsMtime{};
        std::filesystem::file_time_type packedRefsMtime{};
        std::filesystem::file_time_type rootMtime{};
        bool initialized{false};
    };

    void workerLoop();

    Milliseconds m_pollInterval{300};
    gf::git::ports::FsRawEventCallback m_callback{};
    mutable std::mutex m_mutex;

    std::unordered_map<std::string, RepoSnapshots> m_watched;

    std::atomic<bool> m_running{false};
    std::thread m_workerThread{};
    std::condition_variable m_cv{};
};

} // namespace gf::infra::watcher
