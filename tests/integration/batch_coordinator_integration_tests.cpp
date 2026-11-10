#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bl/multi_repo_batch_coordinator.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/tasks/bl/background_task_runner.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/persistence/repositories/sqlite_repository_store.hpp"
#include "infrastructure/persistence/groups/sqlite_group_store.hpp"
#include "infrastructure/persistence/workspaces/sqlite_workspace_store.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

using namespace gf::git::bl;
using namespace gf::git::bo;
using namespace gf::repositories::bo;
using namespace gf::repositories::bl;
using namespace gf::groups::bo;
using namespace gf::groups::bl;
using namespace gf::workspaces::bo;
using namespace gf::workspaces::bl;
using namespace gf::tasks::bl;
using namespace gf::infra::git;
using namespace gf::infra::persistence::sqlite;
using namespace gf::infra::persistence::repositories;
using namespace gf::infra::persistence::groups;
using namespace gf::infra::persistence::workspaces;

namespace
{
    struct RealMultiRepoFixture
    {
        std::filesystem::path baseDir;
        std::filesystem::path bareOrigin1;
        std::filesystem::path bareOrigin2;
        std::filesystem::path repoDir1;
        std::filesystem::path repoDir2;

        RealMultiRepoFixture()
        {
            auto now = std::chrono::system_clock::now().time_since_epoch().count();
            baseDir = std::filesystem::temp_directory_path() / ("gf_batch_integ_" + std::to_string(now));
            bareOrigin1 = baseDir / "origin1.git";
            bareOrigin2 = baseDir / "origin2.git";
            repoDir1 = baseDir / "repo1";
            repoDir2 = baseDir / "repo2";

            std::filesystem::create_directories(bareOrigin1);
            std::filesystem::create_directories(bareOrigin2);
            std::filesystem::create_directories(repoDir1);
            std::filesystem::create_directories(repoDir2);

            // Init bare origins
            std::string initBare1 = "git init --bare -b main \"" + bareOrigin1.string() + "\" > /dev/null 2>&1";
            [[maybe_unused]] int r1 = std::system(initBare1.c_str());
            std::string initBare2 = "git init --bare -b main \"" + bareOrigin2.string() + "\" > /dev/null 2>&1";
            [[maybe_unused]] int r2 = std::system(initBare2.c_str());

            // Init repo 1
            std::string init1 = "git -C \"" + repoDir1.string() + "\" init -b main > /dev/null 2>&1";
            [[maybe_unused]] int r3 = std::system(init1.c_str());
            std::string cfgU1 = "git -C \"" + repoDir1.string() + "\" config user.name \"Batch Tester\"";
            [[maybe_unused]] int r4 = std::system(cfgU1.c_str());
            std::string cfgE1 = "git -C \"" + repoDir1.string() + "\" config user.email \"batch@test.com\"";
            [[maybe_unused]] int r5 = std::system(cfgE1.c_str());

            std::ofstream f1(repoDir1 / "f1.txt");
            f1 << "file1\n";
            f1.close();
            std::string a1 = "git -C \"" + repoDir1.string() + "\" add f1.txt > /dev/null 2>&1";
            [[maybe_unused]] int r6 = std::system(a1.c_str());
            std::string c1 = "git -C \"" + repoDir1.string() + "\" commit -m \"Init 1\" > /dev/null 2>&1";
            [[maybe_unused]] int r7 = std::system(c1.c_str());
            std::string rem1 = "git -C \"" + repoDir1.string() + "\" remote add origin \"" + bareOrigin1.string() +
                "\" > /dev/null 2>&1";
            [[maybe_unused]] int r8 = std::system(rem1.c_str());
            std::string p1 = "git -C \"" + repoDir1.string() + "\" push -u origin main > /dev/null 2>&1";
            [[maybe_unused]] int r9 = std::system(p1.c_str());

            // Init repo 2
            std::string init2 = "git -C \"" + repoDir2.string() + "\" init -b main > /dev/null 2>&1";
            [[maybe_unused]] int r10 = std::system(init2.c_str());
            std::string cfgU2 = "git -C \"" + repoDir2.string() + "\" config user.name \"Batch Tester\"";
            [[maybe_unused]] int r11 = std::system(cfgU2.c_str());
            std::string cfgE2 = "git -C \"" + repoDir2.string() + "\" config user.email \"batch@test.com\"";
            [[maybe_unused]] int r12 = std::system(cfgE2.c_str());

            std::ofstream f2(repoDir2 / "f2.txt");
            f2 << "file2\n";
            f2.close();
            std::string a2 = "git -C \"" + repoDir2.string() + "\" add f2.txt > /dev/null 2>&1";
            [[maybe_unused]] int r13 = std::system(a2.c_str());
            std::string c2 = "git -C \"" + repoDir2.string() + "\" commit -m \"Init 2\" > /dev/null 2>&1";
            [[maybe_unused]] int r14 = std::system(c2.c_str());
            std::string rem2 = "git -C \"" + repoDir2.string() + "\" remote add origin \"" + bareOrigin2.string() +
                "\" > /dev/null 2>&1";
            [[maybe_unused]] int r15 = std::system(rem2.c_str());
            std::string p2 = "git -C \"" + repoDir2.string() + "\" push -u origin main > /dev/null 2>&1";
            [[maybe_unused]] int r16 = std::system(p2.c_str());
        }

        ~RealMultiRepoFixture()
        {
            std::error_code ec;
            std::filesystem::remove_all(baseDir, ec);
        }
    };
} // namespace

TEST_CASE (
"MultiRepoBatchCoordinator integration on real Git repositories"
,
"[git][batch][integration]"
)
 {
    RealMultiRepoFixture fixture;

    auto gitDiscovery = GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    auto gitExe = gitDiscovery->directBinaryPath.empty() ? gitDiscovery->discoveredInPath : gitDiscovery->directBinaryPath;

    NativeGitCommandExecutor gitExecutor(gitExe);
    NativeRepositoryGitOps gitOps(gitExecutor);

    GitRemoteMergeService remoteMergeService(gitOps);
    GitWorkingTreeService workingTreeService(gitOps);
    BackgroundTaskRunner taskRunner(4);

    auto connResult = SqliteConnection::openInMemory();
    REQUIRE(connResult.has_value());
    SqliteConnection conn = std::move(*connResult);

    auto migRes = SchemaMigrator::migrateToLatest(conn);
    REQUIRE(migRes.has_value());

    SqliteRepositoryStore repoStore(conn);
    SqliteGroupStore groupStore(conn);
    SqliteWorkspaceStore wsStore(conn);

    RepositoryService repoService(repoStore);
    GroupService groupService(groupStore, repoStore);
    WorkspaceService wsService(wsStore, groupStore, repoStore);

    auto r1Res = repoService.registerRepository({.localPath = fixture.repoDir1, .displayName = "Repo Real 1"});
    REQUIRE(r1Res.has_value());
    auto r2Res = repoService.registerRepository({.localPath = fixture.repoDir2, .displayName = "Repo Real 2"});
    REQUIRE(r2Res.has_value());

    auto groupRes = groupService.createGroup("All Repos", {r1Res->id, r2Res->id});
    REQUIRE(groupRes.has_value());

    MultiRepoBatchCoordinator coordinator(
        repoService,
        groupService,
        wsService,
        remoteMergeService,
        workingTreeService,
        taskRunner,
        2
    );

    SECTION("Executes asynchronous group fetch across all real repositories") {
        std::atomic<bool> done{false};
        BatchOperationReport capturedReport;
        std::vector<BatchProgressUpdate> updates;

        std::string opId = coordinator.executeGroupFetchAsync(
            groupRes->id.value(),
            "origin",
            [&](const BatchProgressUpdate& u) {
                updates.push_back(u);
            },
            [&](const BatchOperationReport& rep) {
                capturedReport = rep;
                done.store(true);
            }
        );

        REQUIRE_FALSE(opId.empty());
        taskRunner.waitForAll();

        CHECK(done.load());
        CHECK(capturedReport.isAllSucceeded());
        CHECK(capturedReport.totalCount == 2);
        CHECK(capturedReport.succeededCount == 2);
        CHECK(capturedReport.failedCount == 0);
        CHECK_FALSE(updates.empty());
    }

    SECTION("Executes synchronous bulk working tree status check") {
        auto targets = coordinator.resolveGroupTargets(groupRes->id.value());
        REQUIRE(targets.size() == 2);

        auto report = coordinator.executeBatchStatusSync(targets);
        CHECK(report.isAllSucceeded());
        CHECK(report.totalCount == 2);
        CHECK(report.succeededCount == 2);
        for (const auto& item : report.items) {
            CHECK(item.isSuccess());
        }
    }
}
