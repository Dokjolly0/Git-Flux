#include "core/modules/git/bl/git_stash_service.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace {

class MockStashGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    bool listStashesCalled = false;
    bool stashPushCalled = false;
    bool stashPopCalled = false;
    bool stashApplyCalled = false;
    bool stashDropCalled = false;
    bool stashClearCalled = false;

    gf::git::bo::StashPushOptions lastPushOptions;
    gf::git::bo::StashApplyOptions lastPopOptions;
    gf::git::bo::StashApplyOptions lastApplyOptions;
    std::size_t lastDropIndex{999};

    std::vector<gf::git::bo::GitStashItem> stashesToReturn;
    bool failOperations = false;

    // Metodi stash sotto test
    std::expected<std::vector<gf::git::bo::GitStashItem>, gf::git::ports::GitOpsError>
    listStashes(const std::filesystem::path&) override {
        listStashesCalled = true;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = 1,
                .message = "Failed to list stashes",
                .command = "git stash list"
            });
        }
        return stashesToReturn;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashPush(const std::filesystem::path&, const gf::git::bo::StashPushOptions& options) override {
        stashPushCalled = true;
        lastPushOptions = options;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::NothingToStash,
                .exitCode = 0,
                .message = "No local changes to save",
                .command = "git stash push"
            });
        }
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashPop(const std::filesystem::path&, const gf::git::bo::StashApplyOptions& options) override {
        stashPopCalled = true;
        lastPopOptions = options;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::StashNotFound,
                .exitCode = 1,
                .message = "Stash entry not found",
                .command = "git stash pop"
            });
        }
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashApply(const std::filesystem::path&, const gf::git::bo::StashApplyOptions& options) override {
        stashApplyCalled = true;
        lastApplyOptions = options;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::MergeConflict,
                .exitCode = 1,
                .message = "Conflict present",
                .command = "git stash apply"
            });
        }
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashDrop(const std::filesystem::path&, std::size_t stashIndex) override {
        stashDropCalled = true;
        lastDropIndex = stashIndex;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::StashNotFound,
                .exitCode = 1,
                .message = "Stash entry not found",
                .command = "git stash drop"
            });
        }
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashClear(const std::filesystem::path&) override {
        stashClearCalled = true;
        if (failOperations) {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                .exitCode = 1,
                .message = "Failed to clear",
                .command = "git stash clear"
            });
        }
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

    // Metodi non rilevanti per questo mock
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
};

} // namespace

TEST_CASE("GitStashService validates repository path", "[git][stash][service]") {
    MockStashGitOps mockOps;
    gf::git::bl::GitStashService service(mockOps);

    SECTION("Percorso vuoto ritorna InvalidRepositoryPath") {
        auto res = service.listStashes("");
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto pushRes = service.stashPush("");
        REQUIRE_FALSE(pushRes.has_value());
        CHECK(pushRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto popRes = service.stashPop("");
        REQUIRE_FALSE(popRes.has_value());
        CHECK(popRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto applyRes = service.stashApply("");
        REQUIRE_FALSE(applyRes.has_value());
        CHECK(applyRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto dropRes = service.stashDrop("", 0);
        REQUIRE_FALSE(dropRes.has_value());
        CHECK(dropRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto clearRes = service.stashClear("");
        REQUIRE_FALSE(clearRes.has_value());
        CHECK(clearRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);
    }

    SECTION("Percorso inesistente ritorna PathNotFound") {
        auto nonExistent = std::filesystem::path("/path/that/does/not/exist/99999");
        auto res = service.listStashes(nonExistent);
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error().kind == gf::git::ports::GitOpsError::Kind::PathNotFound);
    }
}

TEST_CASE("GitStashService delegates stash operations to IRepositoryGitOps", "[git][stash][service]") {
    MockStashGitOps mockOps;
    gf::git::bl::GitStashService service(mockOps);
    auto validPath = std::filesystem::temp_directory_path();

    SECTION("listStashes ritorna la lista di stash") {
        mockOps.stashesToReturn = {
            gf::git::bo::GitStashItem{
                .index = 0,
                .selector = "stash@{0}",
                .commitSha = "abcdef1234567890",
                .dateIso = "2026-11-07T10:00:00Z",
                .message = "Work in progress feature",
                .branchName = "main"
            },
            gf::git::bo::GitStashItem{
                .index = 1,
                .selector = "stash@{1}",
                .commitSha = "1234567890abcdef",
                .dateIso = "2026-11-06T15:30:00Z",
                .message = "Quick fix temp stash",
                .branchName = "feature/auth"
            }
        };

        auto res = service.listStashes(validPath);
        REQUIRE(res.has_value());
        CHECK(mockOps.listStashesCalled);
        REQUIRE(res->size() == 2);
        CHECK((*res)[0].selector == "stash@{0}");
        CHECK((*res)[0].branchName == "main");
        CHECK((*res)[1].selector == "stash@{1}");
        CHECK((*res)[1].branchName == "feature/auth");
    }

    SECTION("stashPush inoltra le opzioni correttamente") {
        gf::git::bo::StashPushOptions opts{
            .message = "Saved temporary progress",
            .includeUntracked = true,
            .keepIndex = true,
            .paths = {"file1.txt", "src/file2.cpp"}
        };

        auto res = service.stashPush(validPath, opts);
        REQUIRE(res.has_value());
        CHECK(mockOps.stashPushCalled);
        CHECK(mockOps.lastPushOptions.message == "Saved temporary progress");
        CHECK(mockOps.lastPushOptions.includeUntracked == true);
        CHECK(mockOps.lastPushOptions.keepIndex == true);
        REQUIRE(mockOps.lastPushOptions.paths.size() == 2);
    }

    SECTION("stashPop inoltra le opzioni correttamente") {
        gf::git::bo::StashApplyOptions opts{
            .stashIndex = 2,
            .restoreIndex = true
        };

        auto res = service.stashPop(validPath, opts);
        REQUIRE(res.has_value());
        CHECK(mockOps.stashPopCalled);
        CHECK(mockOps.lastPopOptions.stashIndex == 2);
        CHECK(mockOps.lastPopOptions.restoreIndex == true);
    }

    SECTION("stashApply inoltra le opzioni correttamente") {
        gf::git::bo::StashApplyOptions opts{
            .stashIndex = 1,
            .restoreIndex = false
        };

        auto res = service.stashApply(validPath, opts);
        REQUIRE(res.has_value());
        CHECK(mockOps.stashApplyCalled);
        CHECK(mockOps.lastApplyOptions.stashIndex == 1);
        CHECK(mockOps.lastApplyOptions.restoreIndex == false);
    }

    SECTION("stashDrop inoltra l'indice specificato") {
        auto res = service.stashDrop(validPath, 3);
        REQUIRE(res.has_value());
        CHECK(mockOps.stashDropCalled);
        CHECK(mockOps.lastDropIndex == 3);
    }

    SECTION("stashClear invoca clear") {
        auto res = service.stashClear(validPath);
        REQUIRE(res.has_value());
        CHECK(mockOps.stashClearCalled);
    }

    SECTION("Propagazione errori dal backend") {
        mockOps.failOperations = true;

        auto pushRes = service.stashPush(validPath);
        REQUIRE_FALSE(pushRes.has_value());
        CHECK(pushRes.error().kind == gf::git::ports::GitOpsError::Kind::NothingToStash);

        auto popRes = service.stashPop(validPath);
        REQUIRE_FALSE(popRes.has_value());
        CHECK(popRes.error().kind == gf::git::ports::GitOpsError::Kind::StashNotFound);

        auto applyRes = service.stashApply(validPath);
        REQUIRE_FALSE(applyRes.has_value());
        CHECK(applyRes.error().kind == gf::git::ports::GitOpsError::Kind::MergeConflict);
    }
}
