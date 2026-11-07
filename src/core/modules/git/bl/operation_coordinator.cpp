#include "core/modules/git/bl/operation_coordinator.hpp"

#include <algorithm>

namespace gf::git::bl {

OperationCoordinator::OperationCoordinator(std::size_t maxConcurrency)
    : maxConcurrency_(std::max<std::size_t>(1, maxConcurrency)) {
    workers_.reserve(maxConcurrency_);
    for (std::size_t i = 0; i < maxConcurrency_; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }
}

OperationCoordinator::~OperationCoordinator() {
    {
        std::scoped_lock lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
    workers_.clear();
}

void OperationCoordinator::enqueueTask(repositories::bo::RepositoryId repoId,
                                       OperationKind kind,
                                       TaskPriority priority,
                                       std::function<void()> work) {
    {
        std::scoped_lock lock(mutex_);
        QueuedTask entry{std::move(repoId), kind, std::move(work)};
        if (priority == TaskPriority::HighActiveRepo) {
            highQueue_.push_back(std::move(entry));
        } else {
            normalQueue_.push_back(std::move(entry));
        }
    }
    cv_.notify_one();
}

bool OperationCoordinator::canStartTaskLocked(const QueuedTask& task) const {
    if (task.repoId.empty()) {
        return true;
    }
    const auto it = repoStates_.find(task.repoId);
    if (it == repoStates_.end()) {
        return true;
    }
    const RepoConcurrencyState& state = it->second;
    if (task.kind == OperationKind::PassiveRead) {
        return !state.writerActive;
    }
    return !state.writerActive && state.activeReaders == 0;
}

bool OperationCoordinator::tryPopRunnableTaskLocked(QueuedTask& outTask) {
    auto tryPopFrom = [&](std::deque<QueuedTask>& queue) -> bool {
        for (auto it = queue.begin(); it != queue.end(); ++it) {
            if (canStartTaskLocked(*it)) {
                if (!it->repoId.empty()) {
                    auto& state = repoStates_[it->repoId];
                    if (it->kind == OperationKind::PassiveRead) {
                        ++state.activeReaders;
                    } else {
                        state.writerActive = true;
                    }
                }
                outTask = std::move(*it);
                queue.erase(it);
                return true;
            }
        }
        return false;
    };

    if (tryPopFrom(highQueue_)) {
        return true;
    }
    return tryPopFrom(normalQueue_);
}

void OperationCoordinator::onTaskFinished(const repositories::bo::RepositoryId& repoId,
                                          OperationKind kind) {
    {
        std::scoped_lock lock(mutex_);
        if (!repoId.empty()) {
            const auto it = repoStates_.find(repoId);
            if (it != repoStates_.end()) {
                if (kind == OperationKind::PassiveRead) {
                    if (it->second.activeReaders > 0) {
                        --it->second.activeReaders;
                    }
                } else {
                    it->second.writerActive = false;
                }
                if (it->second.activeReaders == 0 && !it->second.writerActive) {
                    repoStates_.erase(it);
                }
            }
        }
    }
    // A task blocked on the same RepositoryId might now be runnable:
    // notify all waiting worker threads.
    cv_.notify_all();
}

void OperationCoordinator::workerLoop() {
    while (true) {
        QueuedTask task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this, &task]() {
                if (tryPopRunnableTaskLocked(task)) {
                    return true;
                }
                return stopping_ && highQueue_.empty() && normalQueue_.empty();
            });

            if (!task.work) {
                return;
            }
        }

        const std::size_t activeNow = currentActive_.fetch_add(1, std::memory_order_relaxed) + 1;
        std::size_t prevMax = maxObservedActive_.load(std::memory_order_relaxed);
        while (activeNow > prevMax &&
               !maxObservedActive_.compare_exchange_weak(prevMax, activeNow, std::memory_order_relaxed)) {
        }

        task.work();

        currentActive_.fetch_sub(1, std::memory_order_relaxed);
        completedTasks_.fetch_add(1, std::memory_order_relaxed);
        onTaskFinished(task.repoId, task.kind);
    }
}

} // namespace gf::git::bl
