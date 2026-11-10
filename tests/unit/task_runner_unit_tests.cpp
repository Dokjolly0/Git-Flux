#include <catch2/catch_test_macros.hpp>

#include "core/modules/tasks/bl/background_task_runner.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

using namespace gf::tasks::bo;
using namespace gf::tasks::bl;
using namespace gf::tasks::ports;

TEST_CASE (
"BackgroundTaskRunner basic lifecycle and task execution"
,
"[tasks][runner]"
)
 {
    using namespace std::chrono_literals;

    BackgroundTaskRunner runner(2);

    SECTION("Executes successful task and reports completion") {
        std::atomic<bool> executed{false};
        TaskReport finalReport;

        std::string taskId = runner.postTask(
            "Test Success Task",
            [&](ITaskProgressSink& /*sink*/) -> std::expected<void, std::string> {
                executed.store(true);
                return {};
            },
            TaskPriority::Normal,
            nullptr,
            [&](const TaskReport& report) {
                finalReport = report;
            }
        );

        REQUIRE_FALSE(taskId.empty());
        runner.waitForAll();

        CHECK(executed.load());
        CHECK(finalReport.taskId == taskId);
        CHECK(finalReport.taskName == "Test Success Task");
        CHECK(finalReport.isSuccess());
        CHECK_FALSE(finalReport.isFailed());
        CHECK_FALSE(finalReport.isCancelled());

        auto stored = runner.getTaskReport(taskId);
        REQUIRE(stored.has_value());
        CHECK(stored->isSuccess());
    }

    SECTION("Executes failing task and captures error message") {
        TaskReport finalReport;

        std::string taskId = runner.postTask(
            "Failing Task",
            [](ITaskProgressSink& /*sink*/) -> std::expected<void, std::string> {
                return std::unexpected("Disk IO failure simulation");
            },
            TaskPriority::Normal,
            nullptr,
            [&](const TaskReport& report) {
                finalReport = report;
            }
        );

        runner.waitForAll();

        CHECK(finalReport.isFailed());
        CHECK(finalReport.errorMessage == "Disk IO failure simulation");
    }

    SECTION("Streams progress updates through ITaskProgressSink") {
        std::vector<int> progressValues;
        std::vector<std::string> progressMessages;

        std::string taskId = runner.postTask(
            "Progress Task",
            [](ITaskProgressSink& sink) -> std::expected<void, std::string> {
                sink.reportProgress(20, "Step 1");
                sink.reportProgress(60, "Step 2");
                sink.reportProgress(100, "Done");
                return {};
            },
            TaskPriority::Normal,
            [&](const TaskProgress& p) {
                if (p.percentage.has_value()) {
                    progressValues.push_back(*p.percentage);
                }
                progressMessages.push_back(p.message);
            }
        );

        runner.waitForAll();

        REQUIRE(progressValues.size() == 3);
        CHECK(progressValues[0] == 20);
        CHECK(progressValues[1] == 60);
        CHECK(progressValues[2] == 100);

        REQUIRE(progressMessages.size() == 3);
        CHECK(progressMessages[0] == "Step 1");
        CHECK(progressMessages[2] == "Done");
    }
}

TEST_CASE (
"BackgroundTaskRunner cancellation and priorities"
,
"[tasks][cancellation]"
)
 {
    using namespace std::chrono_literals;

    BackgroundTaskRunner runner(2);

    SECTION("Cancels running task via stop_token") {
        std::atomic<bool> started{false};
        std::atomic<bool> cancelledObserved{false};
        TaskReport finalReport;

        std::string taskId = runner.postTask(
            "Long running task",
            [&](ITaskProgressSink& sink) -> std::expected<void, std::string> {
                started.store(true);
                while (!sink.isCancelled()) {
                    std::this_thread::sleep_for(5ms);
                }
                cancelledObserved.store(true);
                return {};
            },
            TaskPriority::Normal,
            nullptr,
            [&](const TaskReport& report) {
                finalReport = report;
            }
        );

        while (!started.load()) {
            std::this_thread::sleep_for(1ms);
        }

        bool cancelled = runner.cancelTask(taskId);
        CHECK(cancelled);

        runner.waitForAll();

        CHECK(cancelledObserved.load());
        CHECK(finalReport.isCancelled());
    }

    SECTION("Cancels queued task before it starts") {
        // Saturate the runner with 2 long tasks
        std::atomic<bool> blockWorkers{true};
        runner.postTask("Blocker 1", [&](ITaskProgressSink&) -> std::expected<void, std::string> {
            while (blockWorkers.load()) std::this_thread::sleep_for(5ms);
            return {};
        });
        runner.postTask("Blocker 2", [&](ITaskProgressSink&) -> std::expected<void, std::string> {
            while (blockWorkers.load()) std::this_thread::sleep_for(5ms);
            return {};
        });

        // Post queued task
        std::atomic<bool> queuedExecuted{false};
        TaskReport queuedReport;
        std::string queuedId = runner.postTask(
            "Queued Task",
            [&](ITaskProgressSink&) -> std::expected<void, std::string> {
                queuedExecuted.store(true);
                return {};
            },
            TaskPriority::Normal,
            nullptr,
            [&](const TaskReport& r) {
                queuedReport = r;
            }
        );

        // Cancel while in queue
        bool cancelReq = runner.cancelTask(queuedId);
        CHECK(cancelReq);

        // Release workers
        blockWorkers.store(false);
        runner.waitForAll();

        CHECK_FALSE(queuedExecuted.load());
        CHECK(queuedReport.isCancelled());
    }

    SECTION("Stress test with 50 concurrent fast tasks") {
        std::atomic<int> completedCount{0};

        for (int i = 0; i < 50; ++i) {
            runner.postTask(
                "Fast task " + std::to_string(i),
                [&](ITaskProgressSink&) -> std::expected<void, std::string> {
                    completedCount.fetch_add(1);
                    return {};
                }
            );
        }

        runner.waitForAll();
        CHECK(completedCount.load() == 50);
        CHECK(runner.activeCount() == 0);
        CHECK(runner.queuedCount() == 0);
    }
}
