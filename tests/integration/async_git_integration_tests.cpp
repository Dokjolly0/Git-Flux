#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bl/async_git_service.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/tasks/bl/background_task_runner.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace gf::git::bl;
using namespace gf::git::bo;
using namespace gf::tasks::bl;
using namespace gf::tasks::bo;
using namespace gf::tasks::ports;
using namespace gf::infra::git;

namespace {

struct AsyncGitTestFixture {
    std::filesystem::path originPath;
    std::filesystem::path localPath;

    AsyncGitTestFixture() {
        auto now = std::chrono::system_clock::now().time_since_epoch().count();
        auto tmpBase = std::filesystem::temp_directory_path() / ("gf_async_git_" + std::to_string(now));

        originPath = tmpBase / "origin.git";
        localPath = tmpBase / "local";

        std::filesystem::create_directories(originPath);
        std::filesystem::create_directories(localPath);

        // Init bare origin
        std::string initBare = "git init --bare -b main \"" + originPath.string() + "\" > /dev/null 2>&1";
        [[maybe_unused]] int r1 = std::system(initBare.c_str());

        // Init local and push to origin
        std::string initLocal = "git -C \"" + localPath.string() + "\" init -b main > /dev/null 2>&1";
        [[maybe_unused]] int r2 = std::system(initLocal.c_str());

        std::string cfgUser = "git -C \"" + localPath.string() + "\" config user.name \"Async Tester\"";
        [[maybe_unused]] int r3 = std::system(cfgUser.c_str());
        std::string cfgEmail = "git -C \"" + localPath.string() + "\" config user.email \"async@test.com\"";
        [[maybe_unused]] int r4 = std::system(cfgEmail.c_str());

        // File 1
        std::ofstream ofs(localPath / "file.txt");
        ofs << "Initial async content\n";
        ofs.close();

        std::string addCmd = "git -C \"" + localPath.string() + "\" add file.txt > /dev/null 2>&1";
        [[maybe_unused]] int r5 = std::system(addCmd.c_str());
        std::string commitCmd = "git -C \"" + localPath.string() + "\" commit -m \"Init\" > /dev/null 2>&1";
        [[maybe_unused]] int r6 = std::system(commitCmd.c_str());

        std::string remoteAdd = "git -C \"" + localPath.string() + "\" remote add origin \"" + originPath.string() + "\" > /dev/null 2>&1";
        [[maybe_unused]] int r7 = std::system(remoteAdd.c_str());

        std::string pushCmd = "git -C \"" + localPath.string() + "\" push -u origin main > /dev/null 2>&1";
        [[maybe_unused]] int r8 = std::system(pushCmd.c_str());
    }

    ~AsyncGitTestFixture() {
        std::error_code ec;
        if (!originPath.empty()) std::filesystem::remove_all(originPath.parent_path(), ec);
    }
};

} // namespace

TEST_CASE("AsyncGitService asynchronous Git operations and progress reporting", "[git][async][integration]") {
    using namespace std::chrono_literals;

    AsyncGitTestFixture fixture;

    auto gitDiscovery = GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    auto gitExe = gitDiscovery->directBinaryPath.empty() ? gitDiscovery->discoveredInPath : gitDiscovery->directBinaryPath;

    NativeGitCommandExecutor gitExecutor(gitExe);
    NativeRepositoryGitOps gitOps(gitExecutor);
    GitRemoteMergeService remoteMergeService(gitOps);
    GitWorkingTreeService workingTreeService(gitOps);
    BackgroundTaskRunner taskRunner(2);

    AsyncGitService asyncGit(taskRunner, remoteMergeService, workingTreeService);

    SECTION("Asynchronously scans working tree and streams progress") {
        std::atomic<bool> done{false};
        std::atomic<int> progressCount{0};
        std::expected<RepoWorkingTreeStatus, gf::git::ports::GitOpsError> capturedStatus =
            std::unexpected(gf::git::ports::GitOpsError{.kind = gf::git::ports::GitOpsError::Kind::InvalidOperation, .exitCode = -1, .message = "", .command = ""});

        std::string taskId = asyncGit.refreshWorkingTreeAsync(
            fixture.localPath,
            [&](std::expected<RepoWorkingTreeStatus, gf::git::ports::GitOpsError> res) {
                capturedStatus = std::move(res);
                done.store(true);
            },
            [&](const TaskProgress&) {
                progressCount.fetch_add(1);
            }
        );

        REQUIRE_FALSE(taskId.empty());
        taskRunner.waitForAll();

        CHECK(done.load());
        REQUIRE(capturedStatus.has_value());
        CHECK(capturedStatus->branch.headName == "main");
        CHECK(capturedStatus->isClean());
        CHECK(progressCount.load() >= 1);
    }

    SECTION("Asynchronously fetches from remote origin") {
        std::atomic<bool> done{false};
        std::expected<RemoteOperationResult, gf::git::ports::GitOpsError> capturedResult =
            std::unexpected(gf::git::ports::GitOpsError{.kind = gf::git::ports::GitOpsError::Kind::InvalidOperation, .exitCode = -1, .message = "", .command = ""});

        RemoteOperationOptions opts{
            .remoteName = "origin"
        };

        std::string taskId = asyncGit.fetchRemoteAsync(
            fixture.localPath,
            opts,
            [&](std::expected<RemoteOperationResult, gf::git::ports::GitOpsError> res) {
                capturedResult = std::move(res);
                done.store(true);
            }
        );

        REQUIRE_FALSE(taskId.empty());
        taskRunner.waitForAll();

        CHECK(done.load());
        REQUIRE(capturedResult.has_value());
        CHECK(capturedResult->success);
    }

    SECTION("Cancels long-running async operation") {
        // Block the runner with 2 tasks to allow cancelling the 3rd while queued
        std::atomic<bool> holdWorkers{true};
        taskRunner.postTask("Hold 1", [&](ITaskProgressSink&) -> std::expected<void, std::string> {
            while (holdWorkers.load()) std::this_thread::sleep_for(5ms);
            return {};
        });
        taskRunner.postTask("Hold 2", [&](ITaskProgressSink&) -> std::expected<void, std::string> {
            while (holdWorkers.load()) std::this_thread::sleep_for(5ms);
            return {};
        });

        std::atomic<bool> cancelCbInvoked{false};
        std::string fetchTaskId = asyncGit.fetchRemoteAsync(
            fixture.localPath,
            RemoteOperationOptions{.remoteName = "origin"},
            [&](std::expected<RemoteOperationResult, gf::git::ports::GitOpsError> res) {
                if (!res.has_value()) {
                    cancelCbInvoked.store(true);
                }
            }
        );

        bool cancelled = asyncGit.cancelOperation(fetchTaskId);
        CHECK(cancelled);

        holdWorkers.store(false);
        taskRunner.waitForAll();

        auto report = taskRunner.getTaskReport(fetchTaskId);
        REQUIRE(report.has_value());
        CHECK(report->isCancelled());
    }
}
