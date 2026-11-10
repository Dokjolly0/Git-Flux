#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bl/fs_event_classifier.hpp"
#include "core/modules/git/bl/fs_event_debouncer.hpp"
#include "core/modules/git/bl/git_repository_watcher_service.hpp"
#include "core/modules/git/ports/fs_watcher_port.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

using namespace gf::git::bl;
using namespace gf::git::bo;
using namespace gf::git::ports;

TEST_CASE (
"FsEventClassifier classifies paths correctly"
,
"[watcher][classifier]"
)
 {
    std::filesystem::path repoRoot = "/workspaces/my-project";

    SECTION("Identifies Git metadata files") {
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git" / "HEAD") == FsChangeClassification::GitHeadChanged);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git/refs/heads/main") == FsChangeClassification::GitRefsChanged);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git/refs/tags/v1.0.0") == FsChangeClassification::GitRefsChanged);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git/packed-refs") == FsChangeClassification::GitRefsChanged);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git/FETCH_HEAD") == FsChangeClassification::GitRefsChanged);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git" / "index") == FsChangeClassification::GitIndexChanged);

        // Operation states
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git" / "MERGE_HEAD") == FsChangeClassification::GitOperationState);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git/rebase-merge/head-name") == FsChangeClassification::GitOperationState);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git" / "CHERRY_PICK_HEAD") == FsChangeClassification::GitOperationState);
    }

    SECTION("Filters out noisy, lock and temporary files as Ignored") {
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git/objects/1a/2b3c4d") == FsChangeClassification::Ignored);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git/logs/HEAD") == FsChangeClassification::Ignored);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git/hooks/pre-commit") == FsChangeClassification::Ignored);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git" / "COMMIT_EDITMSG") == FsChangeClassification::Ignored);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".git" / "index.lock") == FsChangeClassification::Ignored);

        // Temporary / Editor backup files
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / "src" / "main.cpp~") == FsChangeClassification::Ignored);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / "src" / ".main.cpp.swp") == FsChangeClassification::Ignored);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / "test.tmp") == FsChangeClassification::Ignored);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / ".DS_Store") == FsChangeClassification::Ignored);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / "Thumbs.db") == FsChangeClassification::Ignored);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / "4913") == FsChangeClassification::Ignored);

        CHECK(FsEventClassifier::isIgnored(repoRoot, repoRoot / ".git/objects/pack/pack-123.pack"));
    }

    SECTION("Classifies user source code files as WorkingTreeModified") {
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / "src" / "main.cpp") == FsChangeClassification::WorkingTreeModified);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / "CMakeLists.txt") == FsChangeClassification::WorkingTreeModified);
        CHECK(FsEventClassifier::classify(repoRoot, repoRoot / "docs" / "readme.md") == FsChangeClassification::WorkingTreeModified);
        CHECK_FALSE(FsEventClassifier::isIgnored(repoRoot, repoRoot / "src" / "main.cpp"));
    }

    SECTION("Supports relative paths directly") {
        CHECK(FsEventClassifier::classify(repoRoot, ".git/HEAD") == FsChangeClassification::GitHeadChanged);
        CHECK(FsEventClassifier::classify(repoRoot, "src/main.cpp") == FsChangeClassification::WorkingTreeModified);
        CHECK(FsEventClassifier::classify(repoRoot, ".git/objects/abc") == FsChangeClassification::Ignored);
    }
}

TEST_CASE (
"FsEventDebouncer aggregates rapid events deterministically"
,
"[watcher][debouncer]"
)
 {
    using namespace std::chrono_literals;

    FsEventDebouncer debouncer(200ms, 1000ms);
    std::filesystem::path repoA = "/repos/projectA";
    std::filesystem::path repoB = "/repos/projectB";

    auto t0 = std::chrono::steady_clock::time_point{};

    SECTION("Debounces events within quiet period") {
        CHECK_FALSE(debouncer.hasPending());

        // Event 1 at t=0ms
        debouncer.pushEvent(
            FsRawEvent{.repositoryPath = repoA, .changedPath = repoA / "file1.cpp"},
            FsChangeClassification::WorkingTreeModified,
            t0
        );

        CHECK(debouncer.hasPending());

        // At t=100ms (< 200ms quiet period) -> not ready
        auto ready1 = debouncer.flushReadyBatches(t0 + 100ms);
        CHECK(ready1.empty());
        CHECK(debouncer.hasPending());

        // Event 2 at t=120ms resets quiet timer
        debouncer.pushEvent(
            FsRawEvent{.repositoryPath = repoA, .changedPath = repoA / "file2.cpp"},
            FsChangeClassification::WorkingTreeModified,
            t0 + 120ms
        );

        // At t=250ms (only 130ms since event 2) -> still not ready
        auto ready2 = debouncer.flushReadyBatches(t0 + 250ms);
        CHECK(ready2.empty());

        // At t=330ms (210ms since event 2 >= 200ms) -> ready!
        auto ready3 = debouncer.flushReadyBatches(t0 + 330ms);
        REQUIRE(ready3.size() == 1);
        CHECK(ready3[0].repositoryPath == repoA);
        CHECK(ready3[0].workingTreeChanged);
        CHECK_FALSE(ready3[0].headChanged);
        CHECK(ready3[0].affectedPaths.size() == 2);
        CHECK(ready3[0].requiresWorkingTreeRefresh());

        CHECK_FALSE(debouncer.hasPending());
    }

    SECTION("Max window enforces flush despite continuous events") {
        // Events every 50ms for 1100ms
        for (int i = 0; i <= 22; ++i) {
            debouncer.pushEvent(
                FsRawEvent{.repositoryPath = repoA, .changedPath = repoA / ("file" + std::to_string(i) + ".txt")},
                FsChangeClassification::WorkingTreeModified,
                t0 + std::chrono::milliseconds(i * 50)
            );
        }

        // At t=950ms (quiet period never reached, maxWindow 1000ms not reached yet)
        auto readyPre = debouncer.flushReadyBatches(t0 + 950ms);
        CHECK(readyPre.empty());

        // At t=1050ms (maxWindow exceeded!)
        auto readyPost = debouncer.flushReadyBatches(t0 + 1050ms);
        REQUIRE(readyPost.size() == 1);
        CHECK(readyPost[0].workingTreeChanged);
        CHECK(readyPost[0].affectedPaths.size() >= 20);
    }

    SECTION("Combines multiple Git change classifications into one batch") {
        debouncer.pushEvent(
            FsRawEvent{.repositoryPath = repoA, .changedPath = repoA / ".git/HEAD"},
            FsChangeClassification::GitHeadChanged,
            t0
        );
        debouncer.pushEvent(
            FsRawEvent{.repositoryPath = repoA, .changedPath = repoA / ".git/index"},
            FsChangeClassification::GitIndexChanged,
            t0 + 10ms
        );
        debouncer.pushEvent(
            FsRawEvent{.repositoryPath = repoA, .changedPath = repoA / "src/app.cpp"},
            FsChangeClassification::WorkingTreeModified,
            t0 + 20ms
        );

        auto batches = debouncer.flushReadyBatches(t0 + 300ms);
        REQUIRE(batches.size() == 1);
        CHECK(batches[0].headChanged);
        CHECK(batches[0].indexChanged);
        CHECK(batches[0].workingTreeChanged);
        CHECK(batches[0].requiresBranchRefresh());
        CHECK(batches[0].requiresWorkingTreeRefresh());
    }

    SECTION("Keeps separate batches for multiple repositories") {
        debouncer.pushEvent(
            FsRawEvent{.repositoryPath = repoA, .changedPath = repoA / "a.txt"},
            FsChangeClassification::WorkingTreeModified,
            t0
        );
        debouncer.pushEvent(
            FsRawEvent{.repositoryPath = repoB, .changedPath = repoB / "b.txt"},
            FsChangeClassification::WorkingTreeModified,
            t0 + 10ms
        );

        auto batches = debouncer.flushAll();
        CHECK(batches.size() == 2);
        CHECK_FALSE(debouncer.hasPending());
    }
}

class MockFsWatcherPort final : public IFsWatcherPort
{
public:
    std::vector<std::filesystem::path> watchedRepos;
    FsRawEventCallback callback;

    std::expected<void, FsWatcherError> watchRepository(const std::filesystem::path& repoPath) override
    {
        watchedRepos.push_back(repoPath);
        return {};
    }

    std::expected<void, FsWatcherError> unwatchRepository(const std::filesystem::path& repoPath) override
    {
        std::erase(watchedRepos, repoPath);
        return {};
    }

    bool isWatching(const std::filesystem::path& repoPath) const override
    {
        return std::find(watchedRepos.begin(), watchedRepos.end(), repoPath) != watchedRepos.end();
    }

    void setEventCallback(FsRawEventCallback cb) override
    {
        callback = std::move(cb);
    }
};

TEST_CASE (
"GitRepositoryWatcherService orchestrates port, debouncer and notifications"
,
"[watcher][service]"
)
 {
    using namespace std::chrono_literals;

    MockFsWatcherPort mockPort;
    GitRepositoryWatcherService service(mockPort, 100ms, 500ms);

    std::filesystem::path repo = "/repos/service-test";

    // Watch repo
    auto resWatch = service.startWatching(repo);
    REQUIRE(resWatch.has_value());
    CHECK(service.isWatching(repo));
    CHECK(mockPort.isWatching(repo));

    std::vector<FsDebouncedBatch> receivedBatches;
    service.setBatchCallback([&](const FsDebouncedBatch& batch) {
        receivedBatches.push_back(batch);
    });

    // Simulate raw event emitted from watcher port
    mockPort.callback(FsRawEvent{
        .repositoryPath = repo,
        .changedPath = repo / ".git" / "HEAD",
        .changeType = FsChangeType::Modified
    });

    // Ignored event should not enter debouncer
    mockPort.callback(FsRawEvent{
        .repositoryPath = repo,
        .changedPath = repo / ".git/objects/ff/1234",
        .changeType = FsChangeType::Created
    });

    CHECK(service.debouncer().hasPending());

    // Flush immediately
    service.flushAll();
    REQUIRE(receivedBatches.size() == 1);
    CHECK(receivedBatches[0].headChanged);
    CHECK(receivedBatches[0].requiresBranchRefresh());
    CHECK_FALSE(receivedBatches[0].workingTreeChanged);

    // Unwatch repo
    auto resUnwatch = service.stopWatching(repo);
    REQUIRE(resUnwatch.has_value());
    CHECK_FALSE(service.isWatching(repo));
}
