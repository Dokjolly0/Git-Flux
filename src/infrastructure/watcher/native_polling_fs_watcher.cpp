#include "infrastructure/watcher/native_polling_fs_watcher.hpp"

#include <system_error>

namespace gf::infra::watcher {

namespace {

std::filesystem::file_time_type safeGetMtime(const std::filesystem::path& p) noexcept {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(p, ec);
    if (ec) {
        return std::filesystem::file_time_type::min();
    }
    return t;
}

} // namespace

NativePollingFsWatcher::NativePollingFsWatcher(Milliseconds pollInterval)
    : m_pollInterval(pollInterval) {}

NativePollingFsWatcher::~NativePollingFsWatcher() {
    stop();
}

std::expected<void, gf::git::ports::FsWatcherError>
NativePollingFsWatcher::watchRepository(const std::filesystem::path& repoPath) {
    if (repoPath.empty()) {
        return std::unexpected(gf::git::ports::FsWatcherError{"Percorso repository vuoto"});
    }

    std::error_code ec;
    if (!std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec)) {
        return std::unexpected(gf::git::ports::FsWatcherError{"Directory del repository inesistente: " + repoPath.string()});
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = repoPath.lexically_normal().string();

    auto it = m_watched.find(key);
    if (it == m_watched.end()) {
        RepoSnapshots snap;
        snap.repoPath = repoPath;
        snap.headMtime = safeGetMtime(repoPath / ".git" / "HEAD");
        snap.indexMtime = safeGetMtime(repoPath / ".git" / "index");
        snap.refsMtime = safeGetMtime(repoPath / ".git" / "refs");
        snap.rootMtime = safeGetMtime(repoPath);
        snap.initialized = true;

        m_watched.emplace(key, snap);
    }

    return {};
}

std::expected<void, gf::git::ports::FsWatcherError>
NativePollingFsWatcher::unwatchRepository(const std::filesystem::path& repoPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = repoPath.lexically_normal().string();
    m_watched.erase(key);
    return {};
}

bool NativePollingFsWatcher::isWatching(const std::filesystem::path& repoPath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = repoPath.lexically_normal().string();
    return m_watched.find(key) != m_watched.end();
}

void NativePollingFsWatcher::setEventCallback(gf::git::ports::FsRawEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(callback);
}

void NativePollingFsWatcher::start() {
    if (m_running.exchange(true)) {
        return;
    }

    m_workerThread = std::thread(&NativePollingFsWatcher::workerLoop, this);
}

void NativePollingFsWatcher::stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    m_cv.notify_all();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void NativePollingFsWatcher::pollOnce() {
    gf::git::ports::FsRawEventCallback cb;
    std::vector<gf::git::bo::FsRawEvent> eventsToEmit;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_callback;

        for (auto& [key, snap] : m_watched) {
            auto currentHead = safeGetMtime(snap.repoPath / ".git" / "HEAD");
            auto currentIndex = safeGetMtime(snap.repoPath / ".git" / "index");
            auto currentRefs = safeGetMtime(snap.repoPath / ".git" / "refs");
            auto currentRoot = safeGetMtime(snap.repoPath);

            if (!snap.initialized) {
                snap.headMtime = currentHead;
                snap.indexMtime = currentIndex;
                snap.refsMtime = currentRefs;
                snap.rootMtime = currentRoot;
                snap.initialized = true;
                continue;
            }

            if (currentHead != snap.headMtime) {
                snap.headMtime = currentHead;
                eventsToEmit.push_back(gf::git::bo::FsRawEvent{
                    .repositoryPath = snap.repoPath,
                    .changedPath = snap.repoPath / ".git" / "HEAD",
                    .changeType = gf::git::bo::FsChangeType::Modified
                });
            }

            if (currentIndex != snap.indexMtime) {
                snap.indexMtime = currentIndex;
                eventsToEmit.push_back(gf::git::bo::FsRawEvent{
                    .repositoryPath = snap.repoPath,
                    .changedPath = snap.repoPath / ".git" / "index",
                    .changeType = gf::git::bo::FsChangeType::Modified
                });
            }

            if (currentRefs != snap.refsMtime) {
                snap.refsMtime = currentRefs;
                eventsToEmit.push_back(gf::git::bo::FsRawEvent{
                    .repositoryPath = snap.repoPath,
                    .changedPath = snap.repoPath / ".git" / "refs",
                    .changeType = gf::git::bo::FsChangeType::Modified
                });
            }

            if (currentRoot != snap.rootMtime) {
                snap.rootMtime = currentRoot;
                eventsToEmit.push_back(gf::git::bo::FsRawEvent{
                    .repositoryPath = snap.repoPath,
                    .changedPath = snap.repoPath,
                    .changeType = gf::git::bo::FsChangeType::Modified
                });
            }
        }
    }

    if (cb) {
        for (const auto& ev : eventsToEmit) {
            cb(ev);
        }
    }
}

void NativePollingFsWatcher::workerLoop() {
    std::unique_lock<std::mutex> lock(m_mutex);

    while (m_running.load()) {
        m_cv.wait_for(lock, m_pollInterval, [this]() {
            return !m_running.load();
        });

        if (!m_running.load()) {
            break;
        }

        lock.unlock();
        pollOnce();
        lock.lock();
    }
}

} // namespace gf::infra::watcher
