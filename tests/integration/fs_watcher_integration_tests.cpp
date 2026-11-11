#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bl/git_repository_watcher_service.hpp"
#include "infrastructure/watcher/native_polling_fs_watcher.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
// std::system() runs through cmd.exe on Windows, where /dev/null does not exist.
#ifdef _WIN32
static const std::string kQuietRedirect = " > NUL 2>&1";
#else
static const std::string kQuietRedirect = " > /dev/null 2>&1";
#endif

using namespace gf::git::bl;
using namespace gf::git::bo;
using namespace gf::infra::watcher;

namespace
{
    struct IntegrationTestRepo
    {
        std::filesystem::path path;

        IntegrationTestRepo()
        {
            path = std::filesystem::temp_directory_path() / ("gf_fs_watcher_test_" + std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count()));
            std::filesystem::remove_all(path);
            std::filesystem::create_directories(path);

            // Initialize git repo
            std::string initCmd = "git -C \"" + path.string() + "\" init -b main" + kQuietRedirect;
            [[maybe_unused]] int r1 = std::system(initCmd.c_str());

            std::string configUser = "git -C \"" + path.string() + "\" config user.name \"Watcher Tester\"";
            [[maybe_unused]] int r2 = std::system(configUser.c_str());
            std::string configEmail = "git -C \"" + path.string() + "\" config user.email \"watcher@test.com\"";
            [[maybe_unused]] int r3 = std::system(configEmail.c_str());

            // Initial commit
            std::ofstream ofs(path / "initial.txt");
            ofs << "Initial commit\n";
            ofs.close();

            std::string addCmd = "git -C \"" + path.string() + "\" add initial.txt" + kQuietRedirect;
            [[maybe_unused]] int r4 = std::system(addCmd.c_str());
            std::string commitCmd = "git -C \"" + path.string() + "\" commit -m \"Initial\"" + kQuietRedirect;
            [[maybe_unused]] int r5 = std::system(commitCmd.c_str());
        }

        ~IntegrationTestRepo()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    };
} // namespace

TEST_CASE (
"NativePollingFsWatcher and GitRepositoryWatcherService on real Git repo"
,
"[watcher][integration]"
)
 {
    using namespace std::chrono_literals;

    IntegrationTestRepo repo;
    NativePollingFsWatcher watcher(50ms);
    GitRepositoryWatcherService watcherService(watcher, 50ms, 200ms);

    std::vector<FsDebouncedBatch> batches;
    watcherService.setBatchCallback([&](const FsDebouncedBatch& b) {
        batches.push_back(b);
    });

    auto watchRes = watcherService.startWatching(repo.path);
    REQUIRE(watchRes.has_value());
    CHECK(watcherService.isWatching(repo.path));

    SECTION("Detects working tree file modification") {
        // Sleep a short moment to ensure distinct mtime
        std::this_thread::sleep_for(20ms);

        // Modify a file in working tree
        {
            std::ofstream ofs(repo.path / "code.cpp", std::ios::app);
            ofs << "// New code\n";
            ofs.close();
        }

        // Trigger polling
        watcher.pollOnce();
        watcherService.flushAll();

        REQUIRE_FALSE(batches.empty());
        CHECK(batches.back().workingTreeChanged);
        CHECK(batches.back().requiresWorkingTreeRefresh());
    }

    SECTION("Detects HEAD and branch change from external git branch switch") {
        std::this_thread::sleep_for(20ms);

        // Create and switch branch via CLI
        std::string switchCmd = "git -C \"" + repo.path.string() + "\" switch -c feature/reactive" + kQuietRedirect;
        int res = std::system(switchCmd.c_str());
        REQUIRE(res == 0);

        watcher.pollOnce();
        watcherService.flushAll();

        REQUIRE_FALSE(batches.empty());
        CHECK(batches.back().headChanged);
        CHECK(batches.back().requiresBranchRefresh());
    }

    SECTION("Detects index changes from external git add") {
        std::this_thread::sleep_for(20ms);

        // Create new file and stage it
        {
            std::ofstream ofs(repo.path / "staged.txt");
            ofs << "hello\n";
            ofs.close();
        }
        std::string addCmd = "git -C \"" + repo.path.string() + "\" add staged.txt" + kQuietRedirect;
        int res = std::system(addCmd.c_str());
        REQUIRE(res == 0);

        watcher.pollOnce();
        watcherService.flushAll();

        REQUIRE_FALSE(batches.empty());
        CHECK(batches.back().indexChanged);
        CHECK(batches.back().requiresWorkingTreeRefresh());
    }

    SECTION("Background worker thread runs continuously without race conditions") {
        watcher.start();
        watcherService.startAutoFlush(20ms);

        CHECK(watcher.isRunning());
        CHECK(watcherService.isAutoFlushRunning());

        // External file write
        {
            std::ofstream ofs(repo.path / "threaded_test.txt");
            ofs << "async test\n";
            ofs.close();
        }

        // Wait a small window for worker to pick up
        std::this_thread::sleep_for(150ms);

        watcherService.stopAutoFlush();
        watcher.stop();

        CHECK_FALSE(watcher.isRunning());
        CHECK_FALSE(watcherService.isAutoFlushRunning());

        CHECK_FALSE(batches.empty());
    }
}
