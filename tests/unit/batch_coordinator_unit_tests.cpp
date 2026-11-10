#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bo/batch_operation_models.hpp"
#include "core/modules/git/bl/multi_repo_batch_coordinator.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"
#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/persistence/repositories/sqlite_repository_store.hpp"
#include "infrastructure/persistence/groups/sqlite_group_store.hpp"
#include "infrastructure/persistence/workspaces/sqlite_workspace_store.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using namespace gf::git::bo;
using namespace gf::git::bl;
using namespace gf::repositories::bo;
using namespace gf::repositories::bl;
using namespace gf::groups::bo;
using namespace gf::groups::bl;
using namespace gf::workspaces::bo;
using namespace gf::workspaces::bl;
using namespace gf::infra::persistence::sqlite;
using namespace gf::infra::persistence::repositories;
using namespace gf::infra::persistence::groups;
using namespace gf::infra::persistence::workspaces;

namespace
{
    class MockBatchGitOps final : public gf::git::ports::IRepositoryGitOps
    {
    public:
        std::unordered_set<std::string> failOnPaths;

        std::expected<RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
        getWorkingTreeStatus(const std::filesystem::path& repoPath) override
        {
            if (failOnPaths.contains(repoPath.string()))
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                    .exitCode = 1,
                    .message = "Working tree failed simulation",
                    .command = "git status"
                });
            }
            RepoWorkingTreeStatus s;
            s.branch.headName = "main";
            return s;
        }

        std::expected<void, gf::git::ports::GitOpsError>
        stageFiles(const std::filesystem::path&, const std::vector<std::string>&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        stageAll(const std::filesystem::path&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        unstageFiles(const std::filesystem::path&, const std::vector<std::string>&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        unstageAll(const std::filesystem::path&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        discardFiles(const std::filesystem::path&, const std::vector<std::string>&) override { return {}; }

        std::expected<CommitResult, gf::git::ports::GitOpsError>
        createCommit(const std::filesystem::path&, const CommitOptions&) override { return CommitResult{}; }

        std::expected<FileDiff, gf::git::ports::GitOpsError>
        getFileDiff(const std::filesystem::path&, const std::string&, DiffScope) override { return FileDiff{}; }

        std::expected<std::vector<FileDiff>, gf::git::ports::GitOpsError>
        getAllDiffs(const std::filesystem::path&, DiffScope) override { return std::vector<FileDiff>{}; }

        std::expected<std::vector<GitBranch>, gf::git::ports::GitOpsError>
        listBranches(const std::filesystem::path&) override { return std::vector<GitBranch>{}; }

        std::expected<void, gf::git::ports::GitOpsError>
        createBranch(const std::filesystem::path&, const CreateBranchOptions&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        checkoutBranch(const std::filesystem::path&, const CheckoutOptions&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        deleteBranch(const std::filesystem::path&, const std::string&, bool) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        renameBranch(const std::filesystem::path&, const std::string&, const std::string&) override { return {}; }

        std::expected<std::vector<GitCommitLogItem>, gf::git::ports::GitOpsError>
        getCommitLog(const std::filesystem::path&, const CommitLogOptions&) override
        {
            return std::vector<GitCommitLogItem>{};
        }

        std::expected<std::vector<GitTag>, gf::git::ports::GitOpsError>
        listTags(const std::filesystem::path&) override { return std::vector<GitTag>{}; }

        std::expected<void, gf::git::ports::GitOpsError>
        createTag(const std::filesystem::path&, const CreateTagOptions&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        deleteTag(const std::filesystem::path&, const std::string&) override { return {}; }

        std::expected<std::vector<GitRemote>, gf::git::ports::GitOpsError>
        listRemotes(const std::filesystem::path&) override { return std::vector<GitRemote>{}; }

        std::expected<void, gf::git::ports::GitOpsError>
        addRemote(const std::filesystem::path&, const std::string&, const std::string&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        removeRemote(const std::filesystem::path&, const std::string&) override { return {}; }

        std::expected<RemoteOperationResult, gf::git::ports::GitOpsError>
        fetchRemote(const std::filesystem::path& repoPath, const RemoteOperationOptions&) override
        {
            if (failOnPaths.contains(repoPath.string()))
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::NetworkAuthenticationFailed,
                    .exitCode = 128,
                    .message = "Auth failed on " + repoPath.filename().string(),
                    .command = "git fetch"
                });
            }
            return RemoteOperationResult{.success = true, .output = "Fetch success"};
        }

        std::expected<RemoteOperationResult, gf::git::ports::GitOpsError>
        pullRemote(const std::filesystem::path& repoPath, const RemoteOperationOptions&) override
        {
            if (failOnPaths.contains(repoPath.string()))
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                    .exitCode = 1,
                    .message = "Pull failed",
                    .command = "git pull"
                });
            }
            return RemoteOperationResult{.success = true, .output = "Pull success"};
        }

        std::expected<RemoteOperationResult, gf::git::ports::GitOpsError>
        pushRemote(const std::filesystem::path&, const RemoteOperationOptions&) override
        {
            return RemoteOperationResult{.success = true};
        }

        std::expected<MergeResult, gf::git::ports::GitOpsError>
        mergeBranch(const std::filesystem::path&, const MergeOptions&) override
        {
            return MergeResult{.status = MergeStatus::Merged};
        }

        std::expected<void, gf::git::ports::GitOpsError>
        abortMerge(const std::filesystem::path&) override { return {}; }

        std::expected<std::vector<GitStashItem>, gf::git::ports::GitOpsError>
        listStashes(const std::filesystem::path&) override { return std::vector<GitStashItem>{}; }

        std::expected<void, gf::git::ports::GitOpsError>
        stashPush(const std::filesystem::path&, const StashPushOptions&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        stashPop(const std::filesystem::path&, const StashApplyOptions&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        stashApply(const std::filesystem::path&, const StashApplyOptions&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        stashDrop(const std::filesystem::path&, std::size_t) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        stashClear(const std::filesystem::path&) override { return {}; }

        std::expected<RebaseResult, gf::git::ports::GitOpsError>
        startRebase(const std::filesystem::path&, const RebaseOptions&) override { return RebaseResult{}; }

        std::expected<RebaseResult, gf::git::ports::GitOpsError>
        continueRebase(const std::filesystem::path&) override { return RebaseResult{}; }

        std::expected<RebaseResult, gf::git::ports::GitOpsError>
        skipRebase(const std::filesystem::path&) override { return RebaseResult{}; }

        std::expected<void, gf::git::ports::GitOpsError>
        abortRebase(const std::filesystem::path&) override { return {}; }

        std::expected<bool, gf::git::ports::GitOpsError>
        isRebaseInProgress(const std::filesystem::path&) override { return false; }

        std::expected<CherryPickResult, gf::git::ports::GitOpsError>
        cherryPick(const std::filesystem::path&, const CherryPickOptions&) override { return CherryPickResult{}; }

        std::expected<void, gf::git::ports::GitOpsError>
        abortCherryPick(const std::filesystem::path&) override { return {}; }

        std::expected<ConflictFile, gf::git::ports::GitOpsError>
        getConflictFile(const std::filesystem::path&, const std::string&) override { return ConflictFile{}; }

        std::expected<std::string, gf::git::ports::GitOpsError>
        getFileStageContent(const std::filesystem::path&, const std::string&, int) override { return ""; }

        std::expected<void, gf::git::ports::GitOpsError>
        applyConflictResolution(const std::filesystem::path&, const ApplyConflictResolutionOptions&) override
        {
            return {};
        }
    };

    struct BatchTestContext
    {
        SqliteConnection connection;
        std::unique_ptr<SqliteRepositoryStore> repoStore;
        std::unique_ptr<SqliteGroupStore> groupStore;
        std::unique_ptr<SqliteWorkspaceStore> workspaceStore;

        std::unique_ptr<RepositoryService> repoService;
        std::unique_ptr<GroupService> groupService;
        std::unique_ptr<WorkspaceService> workspaceService;

        MockBatchGitOps mockOps;
        std::unique_ptr<GitRemoteMergeService> remoteMergeService;
        std::unique_ptr<GitWorkingTreeService> workingTreeService;
        std::unique_ptr<gf::tasks::bl::BackgroundTaskRunner> taskRunner;

        std::filesystem::path tempDir;
        Repository repoA;
        Repository repoB;
        Repository repoC;

        BatchTestContext()
        {
            auto connResult = SqliteConnection::openInMemory();
            REQUIRE(connResult.has_value());
            connection = std::move(*connResult);

            auto migResult = SchemaMigrator::migrateToLatest(connection);
            REQUIRE(migResult.has_value());

            repoStore = std::make_unique<SqliteRepositoryStore>(connection);
            groupStore = std::make_unique<SqliteGroupStore>(connection);
            workspaceStore = std::make_unique<SqliteWorkspaceStore>(connection);

            repoService = std::make_unique<RepositoryService>(*repoStore);
            groupService = std::make_unique<GroupService>(*groupStore, *repoStore);
            workspaceService = std::make_unique<WorkspaceService>(*workspaceStore, *groupStore, *repoStore);

            remoteMergeService = std::make_unique<GitRemoteMergeService>(mockOps);
            workingTreeService = std::make_unique<GitWorkingTreeService>(mockOps);
            taskRunner = std::make_unique<gf::tasks::bl::BackgroundTaskRunner>(2);

            tempDir = std::filesystem::temp_directory_path() / "gf_batch_unit_test";
            std::filesystem::remove_all(tempDir);
            std::filesystem::create_directories(tempDir / "repoA/.git");
            std::filesystem::create_directories(tempDir / "repoB/.git");
            std::filesystem::create_directories(tempDir / "repoC/.git");

            auto rA = repoService->registerRepository({.localPath = tempDir / "repoA", .displayName = "Repo Alpha"});
            REQUIRE(rA.has_value());
            repoA = *rA;

            auto rB = repoService->registerRepository({.localPath = tempDir / "repoB", .displayName = "Repo Beta"});
            REQUIRE(rB.has_value());
            repoB = *rB;

            auto rC = repoService->registerRepository({.localPath = tempDir / "repoC", .displayName = "Repo Gamma"});
            REQUIRE(rC.has_value());
            repoC = *rC;
        }

        ~BatchTestContext()
        {
            std::error_code ec;
            std::filesystem::remove_all(tempDir, ec);
        }
    };
} // namespace

TEST_CASE (
"MultiRepoBatchCoordinator target resolution and deduplication"
,
"[git][batch][coordinator]"
)
 {
    BatchTestContext ctx;
    MultiRepoBatchCoordinator coordinator(
        *ctx.repoService,
        *ctx.groupService,
        *ctx.workspaceService,
        *ctx.remoteMergeService,
        *ctx.workingTreeService,
        *ctx.taskRunner
    );

    SECTION("Deduplicates direct repository IDs") {
        std::vector<std::string> ids = {
            ctx.repoA.id.value(),
            ctx.repoB.id.value(),
            ctx.repoA.id.value(), // Duplicate
            "non-existent-id"
        };

        auto targets = coordinator.resolveTargets(ids);
        REQUIRE(targets.size() == 2);
        CHECK(targets[0].repoId == ctx.repoA.id.value());
        CHECK(targets[0].repoName == "Repo Alpha");
        CHECK(targets[1].repoId == ctx.repoB.id.value());
    }

    SECTION("Resolves group repositories") {
        auto gRes = ctx.groupService->createGroup("Backend", {ctx.repoA.id, ctx.repoB.id});
        REQUIRE(gRes.has_value());

        auto targets = coordinator.resolveGroupTargets(gRes->id.value());
        REQUIRE(targets.size() == 2);
        CHECK(targets[0].repoName == "Repo Alpha");
        CHECK(targets[1].repoName == "Repo Beta");
    }

    SECTION("Resolves workspace repositories unioning direct and groups without duplicates") {
        auto gRes = ctx.groupService->createGroup("Services", {ctx.repoA.id});
        REQUIRE(gRes.has_value());

        auto wsRes = ctx.workspaceService->createWorkspace("Main WS", {gRes->id}, {ctx.repoA.id, ctx.repoC.id});
        REQUIRE(wsRes.has_value());

        auto targets = coordinator.resolveWorkspaceTargets(wsRes->id.value());
        REQUIRE(targets.size() == 2); // repoA deduplicated, repoC included
    }
}

TEST_CASE (
"MultiRepoBatchCoordinator synchronous batch execution with partial failures"
,
"[git][batch][execution]"
)
 {
    BatchTestContext ctx;
    MultiRepoBatchCoordinator coordinator(
        *ctx.repoService,
        *ctx.groupService,
        *ctx.workspaceService,
        *ctx.remoteMergeService,
        *ctx.workingTreeService,
        *ctx.taskRunner
    );

    auto targets = coordinator.resolveTargets({ctx.repoA.id.value(), ctx.repoB.id.value(), ctx.repoC.id.value()});
    REQUIRE(targets.size() == 3);

    // Simulate failure only on repoB
    ctx.mockOps.failOnPaths.insert(ctx.repoB.localPath.string());

    std::vector<BatchProgressUpdate> progressUpdates;

    auto report = coordinator.executeBatchFetchSync(
        targets,
        "origin",
        [&](const BatchProgressUpdate& update) {
            progressUpdates.push_back(update);
        }
    );

    CHECK(report.totalCount == 3);
    CHECK(report.succeededCount == 2);
    CHECK(report.failedCount == 1);
    CHECK(report.hasFailures());
    CHECK_FALSE(report.isAllSucceeded());

    REQUIRE(report.items.size() == 3);
    CHECK(report.items[0].isSuccess());
    CHECK(report.items[1].isFailed());
    CHECK(report.items[1].message.find("Auth failed") != std::string::npos);
    CHECK(report.items[2].isSuccess());

    CHECK_FALSE(progressUpdates.empty());
    CHECK(progressUpdates.back().percentage() == 100);
}

TEST_CASE (
"MultiRepoBatchCoordinator asynchronous execution and cancellation"
,
"[git][batch][async]"
)
 {
    BatchTestContext ctx;
    MultiRepoBatchCoordinator coordinator(
        *ctx.repoService,
        *ctx.groupService,
        *ctx.workspaceService,
        *ctx.remoteMergeService,
        *ctx.workingTreeService,
        *ctx.taskRunner
    );

    auto targets = coordinator.resolveTargets({ctx.repoA.id.value(), ctx.repoB.id.value()});
    REQUIRE(targets.size() == 2);

    SECTION("Asynchronously executes batch status and delivers completion report") {
        std::atomic<bool> completed{false};
        BatchOperationReport capturedReport;

        std::string opId = coordinator.executeBatchStatusAsync(
            targets,
            nullptr,
            [&](const BatchOperationReport& r) {
                capturedReport = r;
                completed.store(true);
            }
        );

        REQUIRE_FALSE(opId.empty());
        ctx.taskRunner->waitForAll();

        CHECK(completed.load());
        CHECK(capturedReport.isAllSucceeded());
        CHECK(capturedReport.succeededCount == 2);
    }
}
