#include "core/modules/git/bl/git_branch_service.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace {

class MockBranchGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    std::vector<gf::git::bo::GitBranch> branchesToReturn;
    gf::git::bo::CreateBranchOptions lastCreateOptions;
    gf::git::bo::CheckoutOptions lastCheckoutOptions;
    std::string lastDeleteBranchName;
    bool lastDeleteForce = false;
    std::string lastRenameOld;
    std::string lastRenameNew;

    bool listCalled = false;
    bool createCalled = false;
    bool checkoutCalled = false;
    bool deleteCalled = false;
    bool renameCalled = false;

    std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
    getWorkingTreeStatus(const std::filesystem::path&) override {
        return gf::git::bo::RepoWorkingTreeStatus{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stageFiles(const std::filesystem::path&, const std::vector<std::string>&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stageAll(const std::filesystem::path&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    unstageFiles(const std::filesystem::path&, const std::vector<std::string>&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    unstageAll(const std::filesystem::path&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    discardFiles(const std::filesystem::path&, const std::vector<std::string>&) override {
        return {};
    }

    std::expected<gf::git::bo::CommitResult, gf::git::ports::GitOpsError>
    createCommit(const std::filesystem::path&, const gf::git::bo::CommitOptions&) override {
        return gf::git::bo::CommitResult{};
    }

    std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
    getFileDiff(const std::filesystem::path&, const std::string&, gf::git::bo::DiffScope) override {
        return gf::git::bo::FileDiff{};
    }

    std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError>
    getAllDiffs(const std::filesystem::path&, gf::git::bo::DiffScope) override {
        return std::vector<gf::git::bo::FileDiff>{};
    }

    std::expected<std::vector<gf::git::bo::GitBranch>, gf::git::ports::GitOpsError>
    listBranches(const std::filesystem::path&) override {
        listCalled = true;
        return branchesToReturn;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    createBranch(const std::filesystem::path&,
                 const gf::git::bo::CreateBranchOptions& options) override {
        createCalled = true;
        lastCreateOptions = options;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    checkoutBranch(const std::filesystem::path&,
                   const gf::git::bo::CheckoutOptions& options) override {
        checkoutCalled = true;
        lastCheckoutOptions = options;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    deleteBranch(const std::filesystem::path&,
                 const std::string& branchName,
                 bool force) override {
        deleteCalled = true;
        lastDeleteBranchName = branchName;
        lastDeleteForce = force;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    renameBranch(const std::filesystem::path&,
                 const std::string& oldName,
                 const std::string& newName) override {
        renameCalled = true;
        lastRenameOld = oldName;
        lastRenameNew = newName;
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

TEST_CASE("GitBranchService validates branch names according to git specifications", "[git][branch][validation]") {
    using gf::git::bl::GitBranchService;

    SECTION("Nomi validi") {
        CHECK(GitBranchService::isValidBranchName("main"));
        CHECK(GitBranchService::isValidBranchName("master"));
        CHECK(GitBranchService::isValidBranchName("feature/login"));
        CHECK(GitBranchService::isValidBranchName("fix-issue-42"));
        CHECK(GitBranchService::isValidBranchName("release_v1.0"));
        CHECK(GitBranchService::isValidBranchName("user/john/wip"));
        CHECK(GitBranchService::isValidBranchName("a"));
    }

    SECTION("Nomi non validi") {
        CHECK_FALSE(GitBranchService::isValidBranchName(""));
        CHECK_FALSE(GitBranchService::isValidBranchName("@"));
        CHECK_FALSE(GitBranchService::isValidBranchName(".hidden"));
        CHECK_FALSE(GitBranchService::isValidBranchName("-flag"));
        CHECK_FALSE(GitBranchService::isValidBranchName("/root"));
        CHECK_FALSE(GitBranchService::isValidBranchName("trailing."));
        CHECK_FALSE(GitBranchService::isValidBranchName("trailing/"));
        CHECK_FALSE(GitBranchService::isValidBranchName("mybranch.lock"));
        CHECK_FALSE(GitBranchService::isValidBranchName("feat/../traversal"));
        CHECK_FALSE(GitBranchService::isValidBranchName("feat//double"));
        CHECK_FALSE(GitBranchService::isValidBranchName("feat@{upstream}"));
        CHECK_FALSE(GitBranchService::isValidBranchName("has space"));
        CHECK_FALSE(GitBranchService::isValidBranchName("has~tilde"));
        CHECK_FALSE(GitBranchService::isValidBranchName("has^caret"));
        CHECK_FALSE(GitBranchService::isValidBranchName("has:colon"));
        CHECK_FALSE(GitBranchService::isValidBranchName("has?question"));
        CHECK_FALSE(GitBranchService::isValidBranchName("has*star"));
        CHECK_FALSE(GitBranchService::isValidBranchName("has[bracket]"));
        CHECK_FALSE(GitBranchService::isValidBranchName("has\\backslash"));
    }
}

TEST_CASE("GitBranchService business logic and delegation", "[git][branch][service]") {
    MockBranchGitOps mockOps;
    gf::git::bl::GitBranchService branchService(mockOps);

    auto validTempDir = std::filesystem::temp_directory_path();

    SECTION("Rifiuta repository path non valido") {
        auto emptyRes = branchService.listBranches(std::filesystem::path{});
        REQUIRE_FALSE(emptyRes.has_value());
        CHECK(emptyRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto nonExist = branchService.listBranches("/path/does/not/exist_gf_123");
        REQUIRE_FALSE(nonExist.has_value());
        CHECK(nonExist.error().kind == gf::git::ports::GitOpsError::Kind::PathNotFound);
    }

    SECTION("listBranches e filtraggio local/remote/current") {
        mockOps.branchesToReturn = {
            gf::git::bo::GitBranch{
                .name = "main",
                .shortName = "main",
                .fullName = "refs/heads/main",
                .targetCommitSha = "1111",
                .upstream = "origin/main",
                .ahead = 1,
                .behind = 0,
                .isCurrent = true,
                .isDetached = false,
                .type = gf::git::bo::BranchType::Local
            },
            gf::git::bo::GitBranch{
                .name = "feature/ui",
                .shortName = "feature/ui",
                .fullName = "refs/heads/feature/ui",
                .targetCommitSha = "2222",
                .upstream = {},
                .ahead = 0,
                .behind = 0,
                .isCurrent = false,
                .isDetached = false,
                .type = gf::git::bo::BranchType::Local
            },
            gf::git::bo::GitBranch{
                .name = "origin/main",
                .shortName = "main",
                .fullName = "refs/remotes/origin/main",
                .targetCommitSha = "1111",
                .upstream = {},
                .ahead = 0,
                .behind = 0,
                .isCurrent = false,
                .isDetached = false,
                .type = gf::git::bo::BranchType::Remote
            }
        };

        auto all = branchService.listBranches(validTempDir);
        REQUIRE(all.has_value());
        CHECK(all->size() == 3);

        auto locals = branchService.getLocalBranches(validTempDir);
        REQUIRE(locals.has_value());
        CHECK(locals->size() == 2);

        auto remotes = branchService.getRemoteBranches(validTempDir);
        REQUIRE(remotes.has_value());
        CHECK(remotes->size() == 1);

        auto current = branchService.getCurrentBranch(validTempDir);
        REQUIRE(current.has_value());
        REQUIRE(current->has_value());
        CHECK((*current)->name == "main");
    }

    SECTION("createBranch convalida nome e delega") {
        auto invalidRes = branchService.createBranch(validTempDir, {.name = "has space"});
        REQUIRE_FALSE(invalidRes.has_value());
        CHECK(invalidRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidBranchName);

        auto validRes = branchService.createBranch(validTempDir, {
            .name = "feature/awesome",
            .startPoint = "main",
            .checkoutAfterCreate = true
        });
        REQUIRE(validRes.has_value());
        CHECK(mockOps.createCalled);
        CHECK(mockOps.lastCreateOptions.name == "feature/awesome");
        CHECK(mockOps.lastCreateOptions.startPoint == "main");
        CHECK(mockOps.lastCreateOptions.checkoutAfterCreate);
    }

    SECTION("checkoutBranch convalida opzioni e delega") {
        auto emptyCheckout = branchService.checkoutBranch(validTempDir, {.branchOrCommit = ""});
        REQUIRE_FALSE(emptyCheckout.has_value());
        CHECK(emptyCheckout.error().kind == gf::git::ports::GitOpsError::Kind::InvalidBranchName);

        auto validCheckout = branchService.checkoutBranch(validTempDir, {
            .branchOrCommit = "feature/awesome",
            .createNewBranch = false,
            .force = false
        });
        REQUIRE(validCheckout.has_value());
        CHECK(mockOps.checkoutCalled);
        CHECK(mockOps.lastCheckoutOptions.branchOrCommit == "feature/awesome");
    }

    SECTION("deleteBranch e renameBranch") {
        auto delEmpty = branchService.deleteBranch(validTempDir, "");
        REQUIRE_FALSE(delEmpty.has_value());

        auto delValid = branchService.deleteBranch(validTempDir, "feature/old", true);
        REQUIRE(delValid.has_value());
        CHECK(mockOps.deleteCalled);
        CHECK(mockOps.lastDeleteBranchName == "feature/old");
        CHECK(mockOps.lastDeleteForce);

        auto renInvalid = branchService.renameBranch(validTempDir, "feature/old", "invalid name~");
        REQUIRE_FALSE(renInvalid.has_value());

        auto renValid = branchService.renameBranch(validTempDir, "feature/old", "feature/new");
        REQUIRE(renValid.has_value());
        CHECK(mockOps.renameCalled);
        CHECK(mockOps.lastRenameOld == "feature/old");
        CHECK(mockOps.lastRenameNew == "feature/new");
    }
}
