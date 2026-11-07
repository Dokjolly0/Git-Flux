#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/modules/git/bl/repository_lock_coordinator.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <future>
#include <vector>

namespace {

class MockRepositoryGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    std::filesystem::path lastRepoPath;
    std::vector<std::string> lastFilePaths;
    gf::git::bo::CommitOptions lastCommitOptions;
    bool stageFilesCalled = false;
    bool stageAllCalled = false;
    bool unstageFilesCalled = false;
    bool unstageAllCalled = false;
    bool discardFilesCalled = false;
    bool createCommitCalled = false;

    std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
    getWorkingTreeStatus(const std::filesystem::path&) override {
        return gf::git::bo::RepoWorkingTreeStatus{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stageFiles(const std::filesystem::path& repoPath,
               const std::vector<std::string>& filePaths) override {
        stageFilesCalled = true;
        lastRepoPath = repoPath;
        lastFilePaths = filePaths;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stageAll(const std::filesystem::path& repoPath) override {
        stageAllCalled = true;
        lastRepoPath = repoPath;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    unstageFiles(const std::filesystem::path& repoPath,
                 const std::vector<std::string>& filePaths) override {
        unstageFilesCalled = true;
        lastRepoPath = repoPath;
        lastFilePaths = filePaths;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    unstageAll(const std::filesystem::path& repoPath) override {
        unstageAllCalled = true;
        lastRepoPath = repoPath;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    discardFiles(const std::filesystem::path& repoPath,
                 const std::vector<std::string>& filePaths) override {
        discardFilesCalled = true;
        lastRepoPath = repoPath;
        lastFilePaths = filePaths;
        return {};
    }

    std::expected<gf::git::bo::CommitResult, gf::git::ports::GitOpsError>
    createCommit(const std::filesystem::path& repoPath,
                 const gf::git::bo::CommitOptions& options) override {
        createCommitCalled = true;
        lastRepoPath = repoPath;
        lastCommitOptions = options;
        return gf::git::bo::CommitResult{
            .commitSha = "abc1234567890abcdef1234567890abcdef12345",
            .shortSha = "abc12345",
            .summary = "Mock summary",
            .branch = "main"
        };
    }

    std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
    getFileDiff(const std::filesystem::path&,
                const std::string&,
                gf::git::bo::DiffScope) override {
        return gf::git::bo::FileDiff{};
    }

    std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError>
    getAllDiffs(const std::filesystem::path&,
                gf::git::bo::DiffScope) override {
        return std::vector<gf::git::bo::FileDiff>{};
    }

    std::expected<std::vector<gf::git::bo::GitBranch>, gf::git::ports::GitOpsError>
    listBranches(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitBranch>{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    createBranch(const std::filesystem::path&,
                 const gf::git::bo::CreateBranchOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    checkoutBranch(const std::filesystem::path&,
                   const gf::git::bo::CheckoutOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    deleteBranch(const std::filesystem::path&,
                 const std::string&,
                 bool) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    renameBranch(const std::filesystem::path&,
                 const std::string&,
                 const std::string&) override {
        return {};
    }

    std::expected<std::vector<gf::git::bo::GitCommitLogItem>, gf::git::ports::GitOpsError>
    getCommitLog(const std::filesystem::path&, const gf::git::bo::CommitLogOptions&) override {
        return std::vector<gf::git::bo::GitCommitLogItem>{};
    }

    std::expected<std::vector<gf::git::bo::GitTag>, gf::git::ports::GitOpsError>
    listTags(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitTag>{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    createTag(const std::filesystem::path&, const gf::git::bo::CreateTagOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    deleteTag(const std::filesystem::path&, const std::string&) override {
        return {};
    }

    std::expected<std::vector<gf::git::bo::GitRemote>, gf::git::ports::GitOpsError>
    listRemotes(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitRemote>{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    addRemote(const std::filesystem::path&, const std::string&, const std::string&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    removeRemote(const std::filesystem::path&, const std::string&) override {
        return {};
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    fetchRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override {
        return gf::git::bo::RemoteOperationResult{.success = true};
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    pullRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override {
        return gf::git::bo::RemoteOperationResult{.success = true};
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    pushRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override {
        return gf::git::bo::RemoteOperationResult{.success = true};
    }

    std::expected<gf::git::bo::MergeResult, gf::git::ports::GitOpsError>
    mergeBranch(const std::filesystem::path&, const gf::git::bo::MergeOptions&) override {
        return gf::git::bo::MergeResult{.status = gf::git::bo::MergeStatus::Merged};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    abortMerge(const std::filesystem::path&) override {
        return {};
    }

    std::expected<std::vector<gf::git::bo::GitStashItem>, gf::git::ports::GitOpsError>
    listStashes(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitStashItem>{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashPush(const std::filesystem::path&, const gf::git::bo::StashPushOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashPop(const std::filesystem::path&, const gf::git::bo::StashApplyOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashApply(const std::filesystem::path&, const gf::git::bo::StashApplyOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashDrop(const std::filesystem::path&, std::size_t) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashClear(const std::filesystem::path&) override {
        return {};
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    startRebase(const std::filesystem::path&, const gf::git::bo::RebaseOptions&) override {
        return gf::git::bo::RebaseResult{.status = gf::git::bo::RebaseStatus::Applied};
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    continueRebase(const std::filesystem::path&) override {
        return gf::git::bo::RebaseResult{.status = gf::git::bo::RebaseStatus::Finished};
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    skipRebase(const std::filesystem::path&) override {
        return gf::git::bo::RebaseResult{.status = gf::git::bo::RebaseStatus::Skipped};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    abortRebase(const std::filesystem::path&) override {
        return {};
    }

    std::expected<bool, gf::git::ports::GitOpsError>
    isRebaseInProgress(const std::filesystem::path&) override {
        return false;
    }

    std::expected<gf::git::bo::CherryPickResult, gf::git::ports::GitOpsError>
    cherryPick(const std::filesystem::path&, const gf::git::bo::CherryPickOptions&) override {
        return gf::git::bo::CherryPickResult{.success = true};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    abortCherryPick(const std::filesystem::path&) override {
        return {};
    }

    std::expected<gf::git::bo::ConflictFile, gf::git::ports::GitOpsError>
    getConflictFile(const std::filesystem::path&, const std::string&) override {
        return gf::git::bo::ConflictFile{};
    }

    std::expected<std::string, gf::git::ports::GitOpsError>
    getFileStageContent(const std::filesystem::path&, const std::string&, int) override {
        return std::string{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    applyConflictResolution(const std::filesystem::path&, const gf::git::bo::ApplyConflictResolutionOptions&) override {
        return {};
    }
};

} // namespace

TEST_CASE("GitStagingService validations and delegation", "[git][staging][service]") {
    MockRepositoryGitOps mockGitOps;
    gf::git::bl::GitStagingService stagingService(mockGitOps);

    auto validTempDir = std::filesystem::temp_directory_path();

    SECTION("Rifiuta repository path vuoto o non esistente") {
        auto emptyPathRes = stagingService.stageAll(std::filesystem::path{});
        REQUIRE_FALSE(emptyPathRes.has_value());
        CHECK(emptyPathRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto nonExistent = stagingService.stageAll("/non/existent/path/gitflux_xyz_123");
        REQUIRE_FALSE(nonExistent.has_value());
        CHECK(nonExistent.error().kind == gf::git::ports::GitOpsError::Kind::PathNotFound);
    }

    SECTION("Rifiuta percorsi assoluti e path traversal") {
        auto absRes = stagingService.stageFiles(validTempDir, {"/etc/passwd"});
        REQUIRE_FALSE(absRes.has_value());
        CHECK(absRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto traversalRes = stagingService.stageFiles(validTempDir, {"src/../../secret.txt"});
        REQUIRE_FALSE(traversalRes.has_value());
        CHECK(traversalRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto emptyFileRes = stagingService.stageFiles(validTempDir, {""});
        REQUIRE_FALSE(emptyFileRes.has_value());
        CHECK(emptyFileRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);
    }

    SECTION("Lista file vuota produce no-op di successo immediato") {
        auto res = stagingService.stageFiles(validTempDir, {});
        REQUIRE(res.has_value());
        CHECK_FALSE(mockGitOps.stageFilesCalled);
    }

    SECTION("Delega corretta di stageFiles e stageAll") {
        auto res1 = stagingService.stageFiles(validTempDir, {"file1.txt", "dir/file2.cpp"});
        REQUIRE(res1.has_value());
        CHECK(mockGitOps.stageFilesCalled);
        CHECK(mockGitOps.lastFilePaths.size() == 2);
        CHECK(mockGitOps.lastFilePaths[0] == "file1.txt");

        auto res2 = stagingService.stageAll(validTempDir);
        REQUIRE(res2.has_value());
        CHECK(mockGitOps.stageAllCalled);
    }

    SECTION("Delega corretta di unstageFiles e unstageAll") {
        auto res1 = stagingService.unstageFiles(validTempDir, {"file1.txt"});
        REQUIRE(res1.has_value());
        CHECK(mockGitOps.unstageFilesCalled);
        CHECK(mockGitOps.lastFilePaths.size() == 1);

        auto res2 = stagingService.unstageAll(validTempDir);
        REQUIRE(res2.has_value());
        CHECK(mockGitOps.unstageAllCalled);
    }

    SECTION("Delega corretta di discardFiles") {
        auto res = stagingService.discardFiles(validTempDir, {"file1.txt"});
        REQUIRE(res.has_value());
        CHECK(mockGitOps.discardFilesCalled);
        CHECK(mockGitOps.lastFilePaths.size() == 1);
    }

    SECTION("Validazione commit message vuoto o solo whitespace") {
        gf::git::bo::CommitOptions emptyOpt{.message = ""};
        auto res1 = stagingService.createCommit(validTempDir, emptyOpt);
        REQUIRE_FALSE(res1.has_value());
        CHECK(res1.error().kind == gf::git::ports::GitOpsError::Kind::EmptyCommitMessage);

        gf::git::bo::CommitOptions wsOpt{.message = "  \n\t  \r\n  "};
        auto res2 = stagingService.createCommit(validTempDir, wsOpt);
        REQUIRE_FALSE(res2.has_value());
        CHECK(res2.error().kind == gf::git::ports::GitOpsError::Kind::EmptyCommitMessage);
    }

    SECTION("Delega corretta di createCommit con opzioni") {
        gf::git::bo::CommitOptions opt{
            .message = "feat: initial commit\n\nDetailed body description",
            .author = gf::git::bo::CommitAuthor{.name = "John Doe", .email = "john@example.com"},
            .amend = false,
            .allowEmpty = false
        };
        auto res = stagingService.createCommit(validTempDir, opt);
        REQUIRE(res.has_value());
        CHECK(mockGitOps.createCommitCalled);
        CHECK(mockGitOps.lastCommitOptions.message == opt.message);
        CHECK(mockGitOps.lastCommitOptions.author.has_value());
        CHECK(mockGitOps.lastCommitOptions.author->name == "John Doe");
        CHECK(res->commitSha == "abc1234567890abcdef1234567890abcdef12345");
        CHECK(res->shortSha == "abc12345");
        CHECK(res->branch == "main");
    }
}

TEST_CASE("RepositoryLockCoordinator per-repo serialization", "[git][concurrency][lock]") {
    auto& coordinator = gf::git::bl::RepositoryLockCoordinator::instance();
    auto tempPathA = std::filesystem::temp_directory_path() / "gf_lock_test_repo_a";
    auto tempPathB = std::filesystem::temp_directory_path() / "gf_lock_test_repo_b";

    std::filesystem::create_directories(tempPathA);
    std::filesystem::create_directories(tempPathB);

    SECTION("Lock su percorsi distinti non bloccano in parallelo") {
        std::atomic<bool> threadAEntered{false};
        std::atomic<bool> threadBEntered{false};

        auto futA = std::async(std::launch::async, [&]() {
            auto lock = coordinator.acquire(tempPathA);
            threadAEntered = true;
            std::this_thread::sleep_for(std::chrono::milliseconds{50});
        });

        auto futB = std::async(std::launch::async, [&]() {
            // Dovrebbe acquisire immediatamente senza attendere threadA
            while (!threadAEntered.load()) {
                std::this_thread::yield();
            }
            auto lock = coordinator.acquire(tempPathB);
            threadBEntered = true;
        });

        futA.get();
        futB.get();

        CHECK(threadAEntered.load());
        CHECK(threadBEntered.load());
    }

    SECTION("Lock sullo stesso percorso serializza l'accesso") {
        std::atomic<int> sequenceCounter{0};
        std::vector<int> executionOrder;
        std::mutex orderMutex;

        auto worker = [&](int id) {
            auto lock = coordinator.acquire(tempPathA);
            {
                std::scoped_lock lk(orderMutex);
                executionOrder.push_back(id);
            }
            sequenceCounter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        };

        auto fut1 = std::async(std::launch::async, worker, 1);
        auto fut2 = std::async(std::launch::async, worker, 2);

        fut1.get();
        fut2.get();

        CHECK(sequenceCounter.load() == 2);
        CHECK(executionOrder.size() == 2);
    }

    std::filesystem::remove_all(tempPathA);
    std::filesystem::remove_all(tempPathB);
}
