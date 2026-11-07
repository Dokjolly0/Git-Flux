#include "core/modules/git/bl/git_rebase_service.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace {

class MockRebaseGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    bool startRebaseCalled = false;
    bool continueRebaseCalled = false;
    bool skipRebaseCalled = false;
    bool abortRebaseCalled = false;
    bool isRebaseInProgressCalled = false;
    bool cherryPickCalled = false;
    bool abortCherryPickCalled = false;

    gf::git::bo::RebaseOptions lastRebaseOptions;
    gf::git::bo::CherryPickOptions lastCherryPickOptions;

    gf::git::bo::RebaseResult rebaseResultToReturn;
    gf::git::bo::CherryPickResult cherryPickResultToReturn;
    bool rebaseInProgressToReturn = false;
    bool failOperations = false;
    gf::git::ports::GitOpsError::Kind failureKind = gf::git::ports::GitOpsError::Kind::ExecutionFailed;

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    startRebase(const std::filesystem::path&, const gf::git::bo::RebaseOptions& options) override {
        startRebaseCalled = true;
        lastRebaseOptions = options;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = failureKind,
                .exitCode = 1,
                .message = "Failed start rebase",
                .command = "git rebase"
            });
        }
        return rebaseResultToReturn;
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    continueRebase(const std::filesystem::path&) override {
        continueRebaseCalled = true;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = failureKind,
                .exitCode = 1,
                .message = "Failed continue rebase",
                .command = "git rebase --continue"
            });
        }
        return rebaseResultToReturn;
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    skipRebase(const std::filesystem::path&) override {
        skipRebaseCalled = true;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = failureKind,
                .exitCode = 1,
                .message = "Failed skip rebase",
                .command = "git rebase --skip"
            });
        }
        return rebaseResultToReturn;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    abortRebase(const std::filesystem::path&) override {
        abortRebaseCalled = true;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = failureKind,
                .exitCode = 1,
                .message = "Failed abort rebase",
                .command = "git rebase --abort"
            });
        }
        return {};
    }

    std::expected<bool, gf::git::ports::GitOpsError>
    isRebaseInProgress(const std::filesystem::path&) override {
        isRebaseInProgressCalled = true;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = failureKind,
                .exitCode = 1,
                .message = "Failed checking rebase status",
                .command = "check"
            });
        }
        return rebaseInProgressToReturn;
    }

    std::expected<gf::git::bo::CherryPickResult, gf::git::ports::GitOpsError>
    cherryPick(const std::filesystem::path&, const gf::git::bo::CherryPickOptions& options) override {
        cherryPickCalled = true;
        lastCherryPickOptions = options;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = failureKind,
                .exitCode = 1,
                .message = "Failed cherry-pick",
                .command = "git cherry-pick"
            });
        }
        return cherryPickResultToReturn;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    abortCherryPick(const std::filesystem::path&) override {
        abortCherryPickCalled = true;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = failureKind,
                .exitCode = 1,
                .message = "Failed abort cherry-pick",
                .command = "git cherry-pick --abort"
            });
        }
        return {};
    }

    // Methods not relevant for this mock
    std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
    getWorkingTreeStatus(const std::filesystem::path&) override { return gf::git::bo::RepoWorkingTreeStatus{}; }
    std::expected<void, gf::git::ports::GitOpsError> stageFiles(const std::filesystem::path&, const std::vector<std::string>&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> stageAll(const std::filesystem::path&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> unstageFiles(const std::filesystem::path&, const std::vector<std::string>&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> unstageAll(const std::filesystem::path&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> discardFiles(const std::filesystem::path&, const std::vector<std::string>&) override { return {}; }
    std::expected<gf::git::bo::CommitResult, gf::git::ports::GitOpsError> createCommit(const std::filesystem::path&, const gf::git::bo::CommitOptions&) override { return gf::git::bo::CommitResult{}; }
    std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError> getFileDiff(const std::filesystem::path&, const std::string&, gf::git::bo::DiffScope) override { return gf::git::bo::FileDiff{}; }
    std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError> getAllDiffs(const std::filesystem::path&, gf::git::bo::DiffScope) override { return std::vector<gf::git::bo::FileDiff>{}; }
    std::expected<std::vector<gf::git::bo::GitBranch>, gf::git::ports::GitOpsError> listBranches(const std::filesystem::path&) override { return std::vector<gf::git::bo::GitBranch>{}; }
    std::expected<void, gf::git::ports::GitOpsError> createBranch(const std::filesystem::path&, const gf::git::bo::CreateBranchOptions&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> checkoutBranch(const std::filesystem::path&, const gf::git::bo::CheckoutOptions&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> deleteBranch(const std::filesystem::path&, const std::string&, bool) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> renameBranch(const std::filesystem::path&, const std::string&, const std::string&) override { return {}; }
    std::expected<std::vector<gf::git::bo::GitCommitLogItem>, gf::git::ports::GitOpsError> getCommitLog(const std::filesystem::path&, const gf::git::bo::CommitLogOptions&) override { return std::vector<gf::git::bo::GitCommitLogItem>{}; }
    std::expected<std::vector<gf::git::bo::GitTag>, gf::git::ports::GitOpsError> listTags(const std::filesystem::path&) override { return std::vector<gf::git::bo::GitTag>{}; }
    std::expected<void, gf::git::ports::GitOpsError> createTag(const std::filesystem::path&, const gf::git::bo::CreateTagOptions&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> deleteTag(const std::filesystem::path&, const std::string&) override { return {}; }
    std::expected<std::vector<gf::git::bo::GitRemote>, gf::git::ports::GitOpsError> listRemotes(const std::filesystem::path&) override { return std::vector<gf::git::bo::GitRemote>{}; }
    std::expected<void, gf::git::ports::GitOpsError> addRemote(const std::filesystem::path&, const std::string&, const std::string&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> removeRemote(const std::filesystem::path&, const std::string&) override { return {}; }
    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError> fetchRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override { return gf::git::bo::RemoteOperationResult{.success = true}; }
    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError> pullRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override { return gf::git::bo::RemoteOperationResult{.success = true}; }
    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError> pushRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override { return gf::git::bo::RemoteOperationResult{.success = true}; }
    std::expected<gf::git::bo::MergeResult, gf::git::ports::GitOpsError> mergeBranch(const std::filesystem::path&, const gf::git::bo::MergeOptions&) override { return gf::git::bo::MergeResult{.status = gf::git::bo::MergeStatus::Merged}; }
    std::expected<void, gf::git::ports::GitOpsError> abortMerge(const std::filesystem::path&) override { return {}; }
    std::expected<std::vector<gf::git::bo::GitStashItem>, gf::git::ports::GitOpsError> listStashes(const std::filesystem::path&) override { return std::vector<gf::git::bo::GitStashItem>{}; }
    std::expected<void, gf::git::ports::GitOpsError> stashPush(const std::filesystem::path&, const gf::git::bo::StashPushOptions&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> stashPop(const std::filesystem::path&, const gf::git::bo::StashApplyOptions&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> stashApply(const std::filesystem::path&, const gf::git::bo::StashApplyOptions&) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> stashDrop(const std::filesystem::path&, std::size_t) override { return {}; }
    std::expected<void, gf::git::ports::GitOpsError> stashClear(const std::filesystem::path&) override { return {}; }
    std::expected<gf::git::bo::ConflictFile, gf::git::ports::GitOpsError> getConflictFile(const std::filesystem::path&, const std::string&) override { return gf::git::bo::ConflictFile{}; }
    std::expected<std::string, gf::git::ports::GitOpsError> getFileStageContent(const std::filesystem::path&, const std::string&, int) override { return std::string{}; }
    std::expected<void, gf::git::ports::GitOpsError> applyConflictResolution(const std::filesystem::path&, const gf::git::bo::ApplyConflictResolutionOptions&) override { return {}; }
};

} // namespace

TEST_CASE("GitRebaseService validates parameters and paths", "[git][rebase][service]") {
    MockRebaseGitOps mockOps;
    gf::git::bl::GitRebaseService service(mockOps);

    SECTION("Percorso vuoto ritorna InvalidRepositoryPath") {
        auto res = service.startRebase("", {.upstream = "main"});
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto contRes = service.continueRebase("");
        REQUIRE_FALSE(contRes.has_value());
        CHECK(contRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto skipRes = service.skipRebase("");
        REQUIRE_FALSE(skipRes.has_value());
        CHECK(skipRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto abortRes = service.abortRebase("");
        REQUIRE_FALSE(abortRes.has_value());
        CHECK(abortRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto inProgRes = service.isRebaseInProgress("");
        REQUIRE_FALSE(inProgRes.has_value());
        CHECK(inProgRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto cpRes = service.cherryPick("", {.commitSha = "abc"});
        REQUIRE_FALSE(cpRes.has_value());
        CHECK(cpRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto cpAbortRes = service.abortCherryPick("");
        REQUIRE_FALSE(cpAbortRes.has_value());
        CHECK(cpAbortRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);
    }

    SECTION("Percorso inesistente ritorna PathNotFound") {
        auto nonExistent = std::filesystem::path("/path/that/does/not/exist/99999");
        auto res = service.startRebase(nonExistent, {.upstream = "main"});
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error().kind == gf::git::ports::GitOpsError::Kind::PathNotFound);
    }

    SECTION("Upstream vuoto in startRebase ritorna InvalidOperation") {
        auto validPath = std::filesystem::temp_directory_path();
        auto res = service.startRebase(validPath, {.upstream = ""});
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);
    }

    SECTION("CommitSha vuoto in cherryPick ritorna InvalidOperation") {
        auto validPath = std::filesystem::temp_directory_path();
        auto res = service.cherryPick(validPath, {.commitSha = ""});
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);
    }
}

TEST_CASE("GitRebaseService delegates operations to IRepositoryGitOps", "[git][rebase][service]") {
    MockRebaseGitOps mockOps;
    gf::git::bl::GitRebaseService service(mockOps);
    auto validPath = std::filesystem::temp_directory_path();

    SECTION("startRebase inoltra opzioni e ritorna risultato") {
        mockOps.rebaseResultToReturn = gf::git::bo::RebaseResult{
            .status = gf::git::bo::RebaseStatus::Applied,
            .currentStep = 2,
            .totalSteps = 5,
            .currentCommitSha = "abc1234",
            .message = "Successfully applied step 2"
        };

        gf::git::bo::RebaseOptions opts{
            .upstream = "main",
            .onto = "origin/main",
            .branch = "feature/alpha",
            .autostash = true
        };

        auto res = service.startRebase(validPath, opts);
        REQUIRE(res.has_value());
        CHECK(mockOps.startRebaseCalled);
        CHECK(mockOps.lastRebaseOptions.upstream == "main");
        CHECK(mockOps.lastRebaseOptions.onto == "origin/main");
        CHECK(mockOps.lastRebaseOptions.branch == "feature/alpha");
        CHECK(mockOps.lastRebaseOptions.autostash == true);
        CHECK(res->status == gf::git::bo::RebaseStatus::Applied);
        CHECK(res->currentStep == 2);
        CHECK(res->totalSteps == 5);
    }

    SECTION("continueRebase invoca continue") {
        mockOps.rebaseResultToReturn.status = gf::git::bo::RebaseStatus::Finished;
        auto res = service.continueRebase(validPath);
        REQUIRE(res.has_value());
        CHECK(mockOps.continueRebaseCalled);
        CHECK(res->status == gf::git::bo::RebaseStatus::Finished);
    }

    SECTION("skipRebase invoca skip") {
        mockOps.rebaseResultToReturn.status = gf::git::bo::RebaseStatus::Skipped;
        auto res = service.skipRebase(validPath);
        REQUIRE(res.has_value());
        CHECK(mockOps.skipRebaseCalled);
        CHECK(res->status == gf::git::bo::RebaseStatus::Skipped);
    }

    SECTION("abortRebase invoca abort") {
        auto res = service.abortRebase(validPath);
        REQUIRE(res.has_value());
        CHECK(mockOps.abortRebaseCalled);
    }

    SECTION("isRebaseInProgress ritorna stato booleano") {
        mockOps.rebaseInProgressToReturn = true;
        auto res = service.isRebaseInProgress(validPath);
        REQUIRE(res.has_value());
        CHECK(mockOps.isRebaseInProgressCalled);
        CHECK(*res == true);
    }

    SECTION("cherryPick inoltra commitSha e noCommit") {
        mockOps.cherryPickResultToReturn = gf::git::bo::CherryPickResult{
            .success = true,
            .hasConflicts = false,
            .newCommitSha = "fedcba987654"
        };

        gf::git::bo::CherryPickOptions opts{
            .commitSha = "1234567890",
            .noCommit = true
        };

        auto res = service.cherryPick(validPath, opts);
        REQUIRE(res.has_value());
        CHECK(mockOps.cherryPickCalled);
        CHECK(mockOps.lastCherryPickOptions.commitSha == "1234567890");
        CHECK(mockOps.lastCherryPickOptions.noCommit == true);
        CHECK(res->success == true);
        CHECK(res->newCommitSha == "fedcba987654");
    }

    SECTION("abortCherryPick invoca abort") {
        auto res = service.abortCherryPick(validPath);
        REQUIRE(res.has_value());
        CHECK(mockOps.abortCherryPickCalled);
    }

    SECTION("Propagazione errori tipizzati") {
        mockOps.failOperations = true;
        mockOps.failureKind = gf::git::ports::GitOpsError::Kind::NoRebaseInProgress;

        auto res = service.continueRebase(validPath);
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error().kind == gf::git::ports::GitOpsError::Kind::NoRebaseInProgress);

        mockOps.failureKind = gf::git::ports::GitOpsError::Kind::CommitNotFound;
        auto cpRes = service.cherryPick(validPath, {.commitSha = "invalid_sha"});
        REQUIRE_FALSE(cpRes.has_value());
        CHECK(cpRes.error().kind == gf::git::ports::GitOpsError::Kind::CommitNotFound);
    }
}
