#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace gf::git::bl {

/// Thread-safe coordinator for per-repository mutual exclusion locks.
/// Ensures that concurrent write operations (stage, commit, discard)
/// on the same physical repository are serialized deterministically,
/// while allowing full concurrency across distinct repositories.
class RepositoryLockCoordinator {
public:
    static RepositoryLockCoordinator& instance() {
        static RepositoryLockCoordinator coordinator;
        return coordinator;
    }

    [[nodiscard]] std::unique_lock<std::mutex> acquire(const std::filesystem::path& repoPath) {
        std::error_code ec;
        auto canonicalPath = std::filesystem::weakly_canonical(repoPath, ec);
        std::string key = ec ? repoPath.lexically_normal().string() : canonicalPath.string();

        std::shared_ptr<std::mutex> repoMutex;
        {
            std::scoped_lock lock(m_mapMutex);
            auto it = m_locks.find(key);
            if (it == m_locks.end()) {
                repoMutex = std::make_shared<std::mutex>();
                m_locks.emplace(std::move(key), repoMutex);
            } else {
                repoMutex = it->second;
            }
        }

        return std::unique_lock<std::mutex>(*repoMutex);
    }

    void clear() {
        std::scoped_lock lock(m_mapMutex);
        m_locks.clear();
    }

private:
    RepositoryLockCoordinator() = default;
    ~RepositoryLockCoordinator() = default;

    std::mutex m_mapMutex;
    std::unordered_map<std::string, std::shared_ptr<std::mutex>> m_locks;
};

} // namespace gf::git::bl
