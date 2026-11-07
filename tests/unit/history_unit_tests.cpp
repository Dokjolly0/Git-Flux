#include "core/modules/git/bl/commit_log_parser.hpp"
#include "core/modules/git/bl/git_history_service.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace {

class MockHistoryGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    std::vector<gf::git::bo::GitCommitLogItem> logToReturn;
    std::vector<gf::git::bo::GitTag> tagsToReturn;
    gf::git::bo::CommitLogOptions lastLogOptions;
    gf::git::bo::CreateTagOptions lastCreateTagOptions;
    std::string lastDeleteTagName;

    bool getCommitLogCalled = false;
    bool listTagsCalled = false;
    bool createTagCalled = false;
    bool deleteTagCalled = false;

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
    getCommitLog(const std::filesystem::path&, const gf::git::bo::CommitLogOptions& options) override {
        getCommitLogCalled = true;
        lastLogOptions = options;
        return logToReturn;
    }

    std::expected<std::vector<gf::git::bo::GitTag>, gf::git::ports::GitOpsError>
    listTags(const std::filesystem::path&) override {
        listTagsCalled = true;
        return tagsToReturn;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    createTag(const std::filesystem::path&, const gf::git::bo::CreateTagOptions& options) override {
        createTagCalled = true;
        lastCreateTagOptions = options;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    deleteTag(const std::filesystem::path&, const std::string& tagName) override {
        deleteTagCalled = true;
        lastDeleteTagName = tagName;
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

TEST_CASE("CommitLogParser parses structured git log output", "[git][history][parser]") {
    using gf::git::bl::CommitLogParser;

    SECTION("Stringa vuota genera zero commit") {
        auto commits = CommitLogParser::parse("");
        CHECK(commits.empty());
    }

    SECTION("Parsing di singolo commit con corpo e decorazioni") {
        std::string raw;
        raw += "e5a32b1c8f4d9e0a1b2c3d4e5f6a7b8c9d0e1f2a"; // %H
        raw += '\0';
        raw += "e5a32b1"; // %h
        raw += '\0';
        raw += "1111222233334444555566667777888899990000"; // %P
        raw += '\0';
        raw += "Mario Rossi"; // %an
        raw += '\0';
        raw += "mario@example.com"; // %ae
        raw += '\0';
        raw += "2026-11-07T10:00:00+01:00"; // %aI
        raw += '\0';
        raw += "GitFlux Bot"; // %cn
        raw += '\0';
        raw += "bot@example.com"; // %ce
        raw += '\0';
        raw += "2026-11-07T10:05:00+01:00"; // %cI
        raw += '\0';
        raw += "HEAD -> main, tag: v1.0.0, origin/main"; // %D
        raw += '\0';
        raw += "feat(core): initial release"; // %s
        raw += '\0';
        raw += "First line of body\nSecond line of body\n"; // %b
        raw += '\x1e';
        raw += '\n';

        auto commits = CommitLogParser::parse(raw);
        REQUIRE(commits.size() == 1);

        const auto& c = commits.front();
        CHECK(c.sha == "e5a32b1c8f4d9e0a1b2c3d4e5f6a7b8c9d0e1f2a");
        CHECK(c.shortSha == "e5a32b1");
        REQUIRE(c.parents.size() == 1);
        CHECK(c.parents[0] == "1111222233334444555566667777888899990000");
        CHECK(c.authorName == "Mario Rossi");
        CHECK(c.authorEmail == "mario@example.com");
        CHECK(c.authorDateIso == "2026-11-07T10:00:00+01:00");
        CHECK(c.committerName == "GitFlux Bot");
        CHECK(c.committerEmail == "bot@example.com");
        CHECK(c.committerDateIso == "2026-11-07T10:05:00+01:00");
        REQUIRE(c.refDecorations.size() == 3);
        CHECK(c.refDecorations[0] == "HEAD -> main");
        CHECK(c.refDecorations[1] == "tag: v1.0.0");
        CHECK(c.refDecorations[2] == "origin/main");
        CHECK(c.subject == "feat(core): initial release");
        CHECK(c.body == "First line of body\nSecond line of body");
    }

    SECTION("Parsing di merge commit con 2 genitori e nessun body") {
        std::string raw;
        raw += "aaaa0000bbbb1111cccc2222dddd3333eeee4444";
        raw += '\0';
        raw += "aaaa000";
        raw += '\0';
        raw += "parent1sha parent2sha";
        raw += '\0';
        raw += "Dev";
        raw += '\0';
        raw += "dev@corp.local";
        raw += '\0';
        raw += "2026-11-07T11:00:00+01:00";
        raw += '\0';
        raw += "Dev";
        raw += '\0';
        raw += "dev@corp.local";
        raw += '\0';
        raw += "2026-11-07T11:00:00+01:00";
        raw += '\0';
        raw += ""; // nessuna decorazione
        raw += '\0';
        raw += "Merge branch 'feature'";
        raw += '\0';
        raw += ""; // body vuoto
        raw += '\x1e';

        auto commits = CommitLogParser::parse(raw);
        REQUIRE(commits.size() == 1);
        CHECK(commits[0].parents.size() == 2);
        CHECK(commits[0].parents[0] == "parent1sha");
        CHECK(commits[0].parents[1] == "parent2sha");
        CHECK(commits[0].refDecorations.empty());
        CHECK(commits[0].body.empty());
    }
}

TEST_CASE("GitHistoryService validates tag names", "[git][history][tag-validation]") {
    using gf::git::bl::GitHistoryService;

    SECTION("Tag validi") {
        CHECK(GitHistoryService::isValidTagName("v1.0.0"));
        CHECK(GitHistoryService::isValidTagName("release-2026.1"));
        CHECK(GitHistoryService::isValidTagName("v0.1-alpha.3"));
        CHECK(GitHistoryService::isValidTagName("build_42"));
        CHECK(GitHistoryService::isValidTagName("milestone/phase1"));
    }

    SECTION("Tag non validi") {
        CHECK_FALSE(GitHistoryService::isValidTagName(""));
        CHECK_FALSE(GitHistoryService::isValidTagName("@"));
        CHECK_FALSE(GitHistoryService::isValidTagName(".hidden"));
        CHECK_FALSE(GitHistoryService::isValidTagName("-v1"));
        CHECK_FALSE(GitHistoryService::isValidTagName("/root"));
        CHECK_FALSE(GitHistoryService::isValidTagName("trailing."));
        CHECK_FALSE(GitHistoryService::isValidTagName("trailing/"));
        CHECK_FALSE(GitHistoryService::isValidTagName("tag.lock"));
        CHECK_FALSE(GitHistoryService::isValidTagName("bad..tag"));
        CHECK_FALSE(GitHistoryService::isValidTagName("bad//tag"));
        CHECK_FALSE(GitHistoryService::isValidTagName("has space"));
        CHECK_FALSE(GitHistoryService::isValidTagName("has~tilde"));
        CHECK_FALSE(GitHistoryService::isValidTagName("has^caret"));
        CHECK_FALSE(GitHistoryService::isValidTagName("has:colon"));
        CHECK_FALSE(GitHistoryService::isValidTagName("has?question"));
        CHECK_FALSE(GitHistoryService::isValidTagName("has*star"));
        CHECK_FALSE(GitHistoryService::isValidTagName("has[bracket]"));
        CHECK_FALSE(GitHistoryService::isValidTagName("has\\backslash"));
    }
}

TEST_CASE("GitHistoryService business logic and delegation", "[git][history][service]") {
    MockHistoryGitOps mockOps;
    gf::git::bl::GitHistoryService historyService(mockOps);

    auto validTempDir = std::filesystem::temp_directory_path();

    SECTION("Rifiuta repository path non validi") {
        auto emptyRes = historyService.getCommitHistory(std::filesystem::path{});
        REQUIRE_FALSE(emptyRes.has_value());
        CHECK(emptyRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto nonExist = historyService.listTags("/path/does/not/exist_gf_history_999");
        REQUIRE_FALSE(nonExist.has_value());
        CHECK(nonExist.error().kind == gf::git::ports::GitOpsError::Kind::PathNotFound);
    }

    SECTION("getCommitDetails ritorna commit trovato o nullopt") {
        mockOps.logToReturn = {
            gf::git::bo::GitCommitLogItem{
                .sha = "1234567890abcdef1234567890abcdef12345678",
                .shortSha = "1234567",
                .subject = "Test commit"
            }
        };

        auto foundRes = historyService.getCommitDetails(validTempDir, "1234567");
        REQUIRE(foundRes.has_value());
        REQUIRE(foundRes->has_value());
        CHECK((*foundRes)->shortSha == "1234567");
        CHECK(mockOps.getCommitLogCalled);
        CHECK(mockOps.lastLogOptions.maxCount == 1);
        CHECK(mockOps.lastLogOptions.revisionRange == "1234567");

        // Commit vuoto
        auto emptyShaRes = historyService.getCommitDetails(validTempDir, "");
        REQUIRE_FALSE(emptyShaRes.has_value());
        CHECK(emptyShaRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);

        // Commit non trovato
        mockOps.logToReturn.clear();
        auto notFoundRes = historyService.getCommitDetails(validTempDir, "nonexistent");
        REQUIRE(notFoundRes.has_value());
        CHECK_FALSE(notFoundRes->has_value());
    }

    SECTION("createTag valida il nome prima della delega") {
        auto invalidRes = historyService.createTag(validTempDir, gf::git::bo::CreateTagOptions{
            .name = "invalid..tag"
        });
        REQUIRE_FALSE(invalidRes.has_value());
        CHECK(invalidRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidTagName);
        CHECK_FALSE(mockOps.createTagCalled);

        auto validRes = historyService.createTag(validTempDir, gf::git::bo::CreateTagOptions{
            .name = "v1.0.0",
            .message = "Initial release"
        });
        REQUIRE(validRes.has_value());
        CHECK(mockOps.createTagCalled);
        CHECK(mockOps.lastCreateTagOptions.name == "v1.0.0");
        CHECK(mockOps.lastCreateTagOptions.message == "Initial release");
    }

    SECTION("deleteTag valida che il nome non sia vuoto") {
        auto emptyRes = historyService.deleteTag(validTempDir, "");
        REQUIRE_FALSE(emptyRes.has_value());
        CHECK(emptyRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidTagName);
        CHECK_FALSE(mockOps.deleteTagCalled);

        auto validRes = historyService.deleteTag(validTempDir, "v1.0.0");
        REQUIRE(validRes.has_value());
        CHECK(mockOps.deleteTagCalled);
        CHECK(mockOps.lastDeleteTagName == "v1.0.0");
    }
}
