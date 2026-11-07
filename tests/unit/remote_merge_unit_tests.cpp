#include "core/modules/git/bl/git_remote_merge_service.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace {

class MockRemoteMergeGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    std::vector<gf::git::bo::GitRemote> remotesToReturn;
    gf::git::bo::RemoteOperationResult remoteResultToReturn{.success = true};
    gf::git::bo::MergeResult mergeResultToReturn{.status = gf::git::bo::MergeStatus::Merged};

    std::string lastAddRemoteName;
    std::string lastAddRemoteUrl;
    std::string lastRemoveRemoteName;
    gf::git::bo::RemoteOperationOptions lastFetchOptions;
    gf::git::bo::RemoteOperationOptions lastPullOptions;
    gf::git::bo::RemoteOperationOptions lastPushOptions;
    gf::git::bo::MergeOptions lastMergeOptions;

    bool listRemotesCalled = false;
    bool addRemoteCalled = false;
    bool removeRemoteCalled = false;
    bool fetchRemoteCalled = false;
    bool pullRemoteCalled = false;
    bool pushRemoteCalled = false;
    bool mergeBranchCalled = false;
    bool abortMergeCalled = false;

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
        return std::vector<gf::git::bo::GitBranch>{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    createBranch(const std::filesystem::path&, const gf::git::bo::CreateBranchOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    checkoutBranch(const std::filesystem::path&, const gf::git::bo::CheckoutOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    deleteBranch(const std::filesystem::path&, const std::string&, bool) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    renameBranch(const std::filesystem::path&, const std::string&, const std::string&) override {
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
        listRemotesCalled = true;
        return remotesToReturn;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    addRemote(const std::filesystem::path&, const std::string& name, const std::string& url) override {
        addRemoteCalled = true;
        lastAddRemoteName = name;
        lastAddRemoteUrl = url;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    removeRemote(const std::filesystem::path&, const std::string& name) override {
        removeRemoteCalled = true;
        lastRemoveRemoteName = name;
        return {};
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    fetchRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions& options) override {
        fetchRemoteCalled = true;
        lastFetchOptions = options;
        return remoteResultToReturn;
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    pullRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions& options) override {
        pullRemoteCalled = true;
        lastPullOptions = options;
        return remoteResultToReturn;
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    pushRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions& options) override {
        pushRemoteCalled = true;
        lastPushOptions = options;
        return remoteResultToReturn;
    }

    std::expected<gf::git::bo::MergeResult, gf::git::ports::GitOpsError>
    mergeBranch(const std::filesystem::path&, const gf::git::bo::MergeOptions& options) override {
        mergeBranchCalled = true;
        lastMergeOptions = options;
        return mergeResultToReturn;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    abortMerge(const std::filesystem::path&) override {
        abortMergeCalled = true;
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
};

} // namespace

TEST_CASE("GitRemoteMergeService validates remote names", "[git][remote][validation]") {
    using gf::git::bl::GitRemoteMergeService;

    SECTION("Nomi validi") {
        CHECK(GitRemoteMergeService::isValidRemoteName("origin"));
        CHECK(GitRemoteMergeService::isValidRemoteName("upstream"));
        CHECK(GitRemoteMergeService::isValidRemoteName("backup_server"));
        CHECK(GitRemoteMergeService::isValidRemoteName("git-mirror-2"));
        CHECK(GitRemoteMergeService::isValidRemoteName("github"));
    }

    SECTION("Nomi non validi") {
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName(""));
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName("has space"));
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName("has/slash"));
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName("has\\backslash"));
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName("has:colon"));
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName("has~tilde"));
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName("has^caret"));
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName("has?question"));
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName("has*star"));
        CHECK_FALSE(GitRemoteMergeService::isValidRemoteName("has[bracket"));
    }
}

TEST_CASE("GitRemoteMergeService business logic and delegations", "[git][remote][merge][service]") {
    MockRemoteMergeGitOps mockOps;
    gf::git::bl::GitRemoteMergeService service(mockOps);

    auto validTempDir = std::filesystem::temp_directory_path();

    SECTION("Verifica percorsi repository non validi") {
        auto emptyRes = service.listRemotes(std::filesystem::path{});
        REQUIRE_FALSE(emptyRes.has_value());
        CHECK(emptyRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto nonExist = service.listRemotes("/path/does/not/exist_gf_remotes_888");
        REQUIRE_FALSE(nonExist.has_value());
        CHECK(nonExist.error().kind == gf::git::ports::GitOpsError::Kind::PathNotFound);
    }

    SECTION("addRemote valida nome e URL") {
        auto badName = service.addRemote(validTempDir, "invalid name", "https://example.com/repo.git");
        REQUIRE_FALSE(badName.has_value());
        CHECK(badName.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);
        CHECK_FALSE(mockOps.addRemoteCalled);

        auto badUrl = service.addRemote(validTempDir, "origin", "");
        REQUIRE_FALSE(badUrl.has_value());
        CHECK(badUrl.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);
        CHECK_FALSE(mockOps.addRemoteCalled);

        auto ok = service.addRemote(validTempDir, "origin", "https://github.com/org/repo.git");
        REQUIRE(ok.has_value());
        CHECK(mockOps.addRemoteCalled);
        CHECK(mockOps.lastAddRemoteName == "origin");
        CHECK(mockOps.lastAddRemoteUrl == "https://github.com/org/repo.git");
    }

    SECTION("removeRemote delega con successo") {
        auto emptyName = service.removeRemote(validTempDir, "");
        REQUIRE_FALSE(emptyName.has_value());
        CHECK(emptyName.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);

        auto ok = service.removeRemote(validTempDir, "origin");
        REQUIRE(ok.has_value());
        CHECK(mockOps.removeRemoteCalled);
        CHECK(mockOps.lastRemoveRemoteName == "origin");
    }

    SECTION("fetch, pull, push delegano correttamente le opzioni") {
        gf::git::bo::RemoteOperationOptions opts{
            .remoteName = "upstream",
            .branchName = "main",
            .prune = true,
            .setUpstream = true,
            .force = true,
            .tags = true
        };

        auto fetchRes = service.fetchRemote(validTempDir, opts);
        REQUIRE(fetchRes.has_value());
        CHECK(mockOps.fetchRemoteCalled);
        CHECK(mockOps.lastFetchOptions.remoteName == "upstream");
        CHECK(mockOps.lastFetchOptions.prune == true);

        auto pullRes = service.pullRemote(validTempDir, opts);
        REQUIRE(pullRes.has_value());
        CHECK(mockOps.pullRemoteCalled);
        CHECK(mockOps.lastPullOptions.remoteName == "upstream");

        auto pushRes = service.pushRemote(validTempDir, opts);
        REQUIRE(pushRes.has_value());
        CHECK(mockOps.pushRemoteCalled);
        CHECK(mockOps.lastPushOptions.remoteName == "upstream");
        CHECK(mockOps.lastPushOptions.setUpstream == true);
    }

    SECTION("mergeBranch valida source e delega le opzioni") {
        auto badSource = service.mergeBranch(validTempDir, gf::git::bo::MergeOptions{});
        REQUIRE_FALSE(badSource.has_value());
        CHECK(badSource.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);

        gf::git::bo::MergeOptions mergeOpts{
            .sourceBranchOrCommit = "feature/login",
            .message = "Merge login feature",
            .noFastForward = true,
            .squash = false
        };

        auto mergeRes = service.mergeBranch(validTempDir, mergeOpts);
        REQUIRE(mergeRes.has_value());
        CHECK(mockOps.mergeBranchCalled);
        CHECK(mockOps.lastMergeOptions.sourceBranchOrCommit == "feature/login");
        CHECK(mockOps.lastMergeOptions.noFastForward == true);
        CHECK(mockOps.lastMergeOptions.message == "Merge login feature");

        auto abortRes = service.abortMerge(validTempDir);
        REQUIRE(abortRes.has_value());
        CHECK(mockOps.abortMergeCalled);
    }
}
