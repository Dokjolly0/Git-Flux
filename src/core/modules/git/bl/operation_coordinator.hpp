#pragma once

#include "core/modules/repositories/bo/repository_id.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace gf::git::bl {

enum class OperationKind {
    PassiveRead,
    Mutating
};

enum class TaskPriority {
    NormalBackground = 0,
    HighActiveRepo = 1
};

/// Coordinates concurrency of Git operations:
/// - Limits global parallelism to `maxConcurrency` active processes/tasks.
/// - Prioritizes `HighActiveRepo` tasks (repository selected in UI).
/// - Enforces Readers-Writer policy per `RepositoryId`: `Mutating` operations
///   on the same repository are strictly serialized without blocking free worker
///   threads for other repositories.
class OperationCoordinator {
public:
    explicit OperationCoordinator(std::size_t maxConcurrency);
    ~OperationCoordinator();

    OperationCoordinator(const OperationCoordinator&) = delete;
    OperationCoordinator& operator=(const OperationCoordinator&) = delete;

    template <typename Fn>
    auto submit(repositories::bo::RepositoryId repoId,
                OperationKind kind,
                TaskPriority priority,
                Fn&& fn) -> std::future<std::invoke_result_t<Fn>> {
        using ReturnType = std::invoke_result_t<Fn>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::forward<Fn>(fn));
        auto future = task->get_future();

        enqueueTask(std::move(repoId), kind, priority,
                    [task]() { (*task)(); });
        return future;
    }

    [[nodiscard]] std::size_t maxConcurrency() const noexcept { return maxConcurrency_; }
    [[nodiscard]] std::size_t maxObservedActiveWorkers() const noexcept {
        return maxObservedActive_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::size_t completedTasksCount() const noexcept {
        return completedTasks_.load(std::memory_order_relaxed);
    }

private:
    struct QueuedTask {
        repositories::bo::RepositoryId repoId;
        OperationKind kind;
        std::function<void()> work;
    };

    struct RepoConcurrencyState {
        std::size_t activeReaders = 0;
        bool writerActive = false;
    };

    void enqueueTask(repositories::bo::RepositoryId repoId,
                     OperationKind kind,
                     TaskPriority priority,
                     std::function<void()> work);

    [[nodiscard]] bool canStartTaskLocked(const QueuedTask& task) const;
    [[nodiscard]] bool tryPopRunnableTaskLocked(QueuedTask& outTask);
    void onTaskFinished(const repositories::bo::RepositoryId& repoId, OperationKind kind);
    void workerLoop();

    std::size_t maxConcurrency_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;

    std::deque<QueuedTask> highQueue_;
    std::deque<QueuedTask> normalQueue_;
    std::unordered_map<repositories::bo::RepositoryId, RepoConcurrencyState> repoStates_;

    std::atomic<std::size_t> currentActive_{0};
    std::atomic<std::size_t> maxObservedActive_{0};
    std::atomic<std::size_t> completedTasks_{0};
    std::vector<std::jthread> workers_;
};

} // namespace gf::git::bl
