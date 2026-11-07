#pragma once

#include "core/modules/tasks/bo/task_models.hpp"
#include "core/modules/tasks/ports/task_progress_sink.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace gf::tasks::bl {

using TaskExecutable = std::function<std::expected<void, std::string>(ports::ITaskProgressSink&)>;
using ProgressCallback = std::function<void(const bo::TaskProgress&)>;
using CompletionCallback = std::function<void(const bo::TaskReport&)>;

class BackgroundTaskRunner {
public:
    explicit BackgroundTaskRunner(std::size_t threadCount = 4);
    ~BackgroundTaskRunner();

    BackgroundTaskRunner(const BackgroundTaskRunner&) = delete;
    BackgroundTaskRunner& operator=(const BackgroundTaskRunner&) = delete;
    BackgroundTaskRunner(BackgroundTaskRunner&&) = delete;
    BackgroundTaskRunner& operator=(BackgroundTaskRunner&&) = delete;

    std::string postTask(std::string name,
                         TaskExecutable executable,
                         bo::TaskPriority priority = bo::TaskPriority::Normal,
                         ProgressCallback onProgress = nullptr,
                         CompletionCallback onCompleted = nullptr);

    bool cancelTask(const std::string& taskId);
    void cancelAll();

    [[nodiscard]] std::optional<bo::TaskReport> getTaskReport(const std::string& taskId) const;
    [[nodiscard]] std::size_t activeCount() const noexcept;
    [[nodiscard]] std::size_t queuedCount() const noexcept;

    void waitForAll();
    void shutdown();
    [[nodiscard]] bool isShutdown() const noexcept { return m_shutdown.load(); }

private:
    struct TaskItem {
        std::string taskId;
        std::string name;
        bo::TaskPriority priority{bo::TaskPriority::Normal};
        uint64_t sequenceNumber{0};
        TaskExecutable executable;
        ProgressCallback onProgress;
        CompletionCallback onCompleted;
        std::stop_source stopSource;

        bool operator<(const TaskItem& other) const noexcept {
            if (priority != other.priority) {
                return static_cast<int>(priority) < static_cast<int>(other.priority);
            }
            return sequenceNumber > other.sequenceNumber; // FIFO a parità di priorità
        }
    };

    void workerThreadLoop();

    std::atomic<bool> m_shutdown{false};
    std::atomic<uint64_t> m_nextSeq{1};
    std::atomic<std::size_t> m_activeCount{0};

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::condition_variable m_idleCv;

    std::priority_queue<TaskItem> m_taskQueue;
    std::unordered_map<std::string, std::stop_source> m_runningStopSources;
    std::unordered_map<std::string, bo::TaskReport> m_reports;

    std::vector<std::jthread> m_workers;
};

} // namespace gf::tasks::bl
