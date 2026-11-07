#include "core/modules/tasks/bl/background_task_runner.hpp"

#include <chrono>

namespace gf::tasks::bl {

namespace {

class TaskProgressSinkImpl final : public ports::ITaskProgressSink {
public:
    TaskProgressSinkImpl(std::stop_token token, ProgressCallback callback)
        : m_token(std::move(token))
        , m_callback(std::move(callback)) {}

    void reportProgress(const bo::TaskProgress& progress) override {
        if (m_callback) {
            m_callback(progress);
        }
    }

    void reportProgress(int percentage, const std::string& message) override {
        if (m_callback) {
            m_callback(bo::TaskProgress{
                .percentage = percentage,
                .message = message
            });
        }
    }

    [[nodiscard]] bool isCancelled() const noexcept override {
        return m_token.stop_requested();
    }

    [[nodiscard]] std::stop_token stopToken() const noexcept override {
        return m_token;
    }

private:
    std::stop_token m_token;
    ProgressCallback m_callback;
};

} // namespace

BackgroundTaskRunner::BackgroundTaskRunner(std::size_t threadCount) {
    if (threadCount == 0) {
        threadCount = 1;
    }

    m_workers.reserve(threadCount);
    for (std::size_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back(&BackgroundTaskRunner::workerThreadLoop, this);
    }
}

BackgroundTaskRunner::~BackgroundTaskRunner() {
    shutdown();
}

std::string BackgroundTaskRunner::postTask(std::string name,
                                           TaskExecutable executable,
                                           bo::TaskPriority priority,
                                           ProgressCallback onProgress,
                                           CompletionCallback onCompleted) {
    if (!executable || m_shutdown.load()) {
        return {};
    }

    uint64_t seq = m_nextSeq.fetch_add(1);
    std::string taskId = "task-" + std::to_string(seq);

    TaskItem item{
        .taskId = taskId,
        .name = std::move(name),
        .priority = priority,
        .sequenceNumber = seq,
        .executable = std::move(executable),
        .onProgress = std::move(onProgress),
        .onCompleted = std::move(onCompleted),
        .stopSource = std::stop_source{}
    };

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_reports[taskId] = bo::TaskReport{
            .taskId = taskId,
            .taskName = item.name,
            .state = bo::TaskState::Pending,
            .errorMessage = {},
            .durationMs = std::chrono::milliseconds(0)
        };
        m_runningStopSources.emplace(taskId, item.stopSource);
        m_taskQueue.push(std::move(item));
    }

    m_cv.notify_one();
    return taskId;
}

bool BackgroundTaskRunner::cancelTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_runningStopSources.find(taskId);
    if (it != m_runningStopSources.end()) {
        return it->second.request_stop();
    }
    return false;
}

void BackgroundTaskRunner::cancelAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [id, source] : m_runningStopSources) {
        source.request_stop();
    }
}

std::optional<bo::TaskReport> BackgroundTaskRunner::getTaskReport(const std::string& taskId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_reports.find(taskId);
    if (it != m_reports.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::size_t BackgroundTaskRunner::activeCount() const noexcept {
    return m_activeCount.load();
}

std::size_t BackgroundTaskRunner::queuedCount() const noexcept {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_taskQueue.size();
}

void BackgroundTaskRunner::waitForAll() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_idleCv.wait(lock, [this]() {
        return m_taskQueue.empty() && m_activeCount.load() == 0;
    });
}

void BackgroundTaskRunner::shutdown() {
    if (m_shutdown.exchange(true)) {
        return;
    }

    cancelAll();
    m_cv.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.request_stop();
            worker.join();
        }
    }
    m_workers.clear();

    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_taskQueue.empty()) {
        m_taskQueue.pop();
    }
    m_runningStopSources.clear();
    m_idleCv.notify_all();
}

void BackgroundTaskRunner::workerThreadLoop() {
    while (!m_shutdown.load()) {
        TaskItem item;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() {
                return m_shutdown.load() || !m_taskQueue.empty();
            });

            if (m_shutdown.load() && m_taskQueue.empty()) {
                break;
            }

            if (m_taskQueue.empty()) {
                continue;
            }

            item = std::move(const_cast<TaskItem&>(m_taskQueue.top()));
            m_taskQueue.pop();
            m_activeCount.fetch_add(1);

            if (auto it = m_reports.find(item.taskId); it != m_reports.end()) {
                it->second.state = bo::TaskState::Running;
            }
        }

        auto startTime = std::chrono::steady_clock::now();
        bo::TaskReport report{
            .taskId = item.taskId,
            .taskName = item.name,
            .state = bo::TaskState::Running,
            .errorMessage = {},
            .durationMs = std::chrono::milliseconds(0)
        };

        if (item.stopSource.stop_requested()) {
            report.state = bo::TaskState::Cancelled;
        } else {
            TaskProgressSinkImpl sink(item.stopSource.get_token(), item.onProgress);
            auto execResult = item.executable(sink);

            auto endTime = std::chrono::steady_clock::now();
            report.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

            if (item.stopSource.stop_requested() || sink.isCancelled()) {
                report.state = bo::TaskState::Cancelled;
            } else if (!execResult.has_value()) {
                report.state = bo::TaskState::Failed;
                report.errorMessage = execResult.error();
            } else {
                report.state = bo::TaskState::Completed;
            }
        }

        CompletionCallback onCompletedCopy = std::move(item.onCompleted);

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_reports[item.taskId] = report;
            m_runningStopSources.erase(item.taskId);
            m_activeCount.fetch_sub(1);
        }

        if (onCompletedCopy) {
            onCompletedCopy(report);
        }

        m_idleCv.notify_all();
    }
}

} // namespace gf::tasks::bl
