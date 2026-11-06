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

/// Coordina la concorrenza delle operazioni Git:
/// - Limita il parallelismo globale a `maxConcurrency` processi/task attivi.
/// - Dà precedenza ai task `HighActiveRepo` (repository selezionato nella UI).
/// - Applica una policy Readers-Writer per singolo `RepositoryId`: le operazioni
///   `Mutating` sullo stesso repository sono strettamente serializzate senza bloccare
///   i worker thread liberi per gli altri repository.
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
        std::packaged_task<ReturnType()> task(std::forward<Fn>(fn));
        auto future = task.get_future();

        enqueueTask(std::move(repoId), kind, priority,
                    [t = std::move(task)]() mutable { t(); });
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
        std::move_only_function<void()> work;
    };

    struct RepoConcurrencyState {
        std::size_t activeReaders = 0;
        bool writerActive = false;
    };

    void enqueueTask(repositories::bo::RepositoryId repoId,
                     OperationKind kind,
                     TaskPriority priority,
                     std::move_only_function<void()> work);

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
