#include "core/modules/git/bl/git_diff_service.hpp"
#include "core/modules/git/bl/unified_diff_parser.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string_view>

namespace {

class MockDiffGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    std::filesystem::path lastRepoPath;
    std::string lastFilePath;
    gf::git::bo::DiffScope lastScope = gf::git::bo::DiffScope::Unstaged;
    bool getFileDiffCalled = false;
    bool getAllDiffsCalled = false;

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
    getFileDiff(const std::filesystem::path& repoPath,
                const std::string& filePath,
                gf::git::bo::DiffScope scope) override {
        getFileDiffCalled = true;
        lastRepoPath = repoPath;
        lastFilePath = filePath;
        lastScope = scope;
        return gf::git::bo::FileDiff{
            .oldPath = filePath,
            .newPath = filePath,
            .additionsCount = 2,
            .deletionsCount = 1
        };
    }

    std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError>
    getAllDiffs(const std::filesystem::path& repoPath,
                gf::git::bo::DiffScope scope) override {
        getAllDiffsCalled = true;
        lastRepoPath = repoPath;
        lastScope = scope;
        return std::vector<gf::git::bo::FileDiff>{
            gf::git::bo::FileDiff{.oldPath = "a.txt", .newPath = "a.txt"}
        };
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

TEST_CASE("UnifiedDiffParser parses unified diff payloads accurately", "[git][diff][parser]") {
    SECTION("Parse modifica standard a singolo hunk") {
        constexpr std::string_view diffText =
            "diff --git a/src/app.cpp b/src/app.cpp\n"
            "index e69de29..488cd7c 100644\n"
            "--- a/src/app.cpp\n"
            "+++ b/src/app.cpp\n"
            "@@ -1,4 +1,5 @@ int main() {\n"
            " line1\n"
            "-line2\n"
            "+line2 modified\n"
            "+line3 added\n"
            " line4\n";

        auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(diffText);
        REQUIRE(diffOpt.has_value());
        CHECK(diffOpt->oldPath == "src/app.cpp");
        CHECK(diffOpt->newPath == "src/app.cpp");
        CHECK_FALSE(diffOpt->isBinary);
        CHECK_FALSE(diffOpt->isNewFile);
        CHECK_FALSE(diffOpt->isDeletedFile);
        CHECK(diffOpt->additionsCount == 2);
        CHECK(diffOpt->deletionsCount == 1);

        REQUIRE(diffOpt->hunks.size() == 1);
        const auto& hunk = diffOpt->hunks.front();
        CHECK(hunk.oldStartLine == 1);
        CHECK(hunk.oldCount == 4);
        CHECK(hunk.newStartLine == 1);
        CHECK(hunk.newCount == 5);
        REQUIRE(hunk.lines.size() == 5);

        // line1: context
        CHECK(hunk.lines[0].kind == gf::git::bo::DiffLineKind::Context);
        CHECK(hunk.lines[0].oldLineNumber == 1);
        CHECK(hunk.lines[0].newLineNumber == 1);
        CHECK(hunk.lines[0].content == "line1");

        // -line2: deletion
        CHECK(hunk.lines[1].kind == gf::git::bo::DiffLineKind::Deletion);
        CHECK(hunk.lines[1].oldLineNumber == 2);
        CHECK_FALSE(hunk.lines[1].newLineNumber.has_value());
        CHECK(hunk.lines[1].content == "line2");

        // +line2 modified: addition
        CHECK(hunk.lines[2].kind == gf::git::bo::DiffLineKind::Addition);
        CHECK_FALSE(hunk.lines[2].oldLineNumber.has_value());
        CHECK(hunk.lines[2].newLineNumber == 2);
        CHECK(hunk.lines[2].content == "line2 modified");

        // +line3 added: addition
        CHECK(hunk.lines[3].kind == gf::git::bo::DiffLineKind::Addition);
        CHECK_FALSE(hunk.lines[3].oldLineNumber.has_value());
        CHECK(hunk.lines[3].newLineNumber == 3);
        CHECK(hunk.lines[3].content == "line3 added");

        // line4: context
        CHECK(hunk.lines[4].kind == gf::git::bo::DiffLineKind::Context);
        CHECK(hunk.lines[4].oldLineNumber == 3);
        CHECK(hunk.lines[4].newLineNumber == 4);
        CHECK(hunk.lines[4].content == "line4");
    }

    SECTION("Parse nuovo file aggiunto") {
        constexpr std::string_view diffText =
            "diff --git a/newfile.txt b/newfile.txt\n"
            "new file mode 100644\n"
            "index 0000000..94954ab\n"
            "--- /dev/null\n"
            "+++ b/newfile.txt\n"
            "@@ -0,0 +1,2 @@\n"
            "+hello\n"
            "+world\n";

        auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(diffText);
        REQUIRE(diffOpt.has_value());
        CHECK(diffOpt->isNewFile);
        CHECK(diffOpt->newPath == "newfile.txt");
        CHECK(diffOpt->additionsCount == 2);
        CHECK(diffOpt->deletionsCount == 0);
    }

    SECTION("Parse file eliminato") {
        constexpr std::string_view diffText =
            "diff --git a/deleted.txt b/deleted.txt\n"
            "deleted file mode 100644\n"
            "index 94954ab..0000000\n"
            "--- a/deleted.txt\n"
            "+++ /dev/null\n"
            "@@ -1,2 +0,0 @@\n"
            "-hello\n"
            "-world\n";

        auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(diffText);
        REQUIRE(diffOpt.has_value());
        CHECK(diffOpt->isDeletedFile);
        CHECK(diffOpt->oldPath == "deleted.txt");
        CHECK(diffOpt->additionsCount == 0);
        CHECK(diffOpt->deletionsCount == 2);
    }

    SECTION("Parse rename") {
        constexpr std::string_view diffText =
            "diff --git a/old_name.txt b/new_name.txt\n"
            "similarity index 100%\n"
            "rename from old_name.txt\n"
            "rename to new_name.txt\n";

        auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(diffText);
        REQUIRE(diffOpt.has_value());
        CHECK(diffOpt->isRenamed);
        CHECK(diffOpt->oldPath == "old_name.txt");
        CHECK(diffOpt->newPath == "new_name.txt");
        CHECK(diffOpt->hunks.empty());
    }

    SECTION("Parse file binario") {
        constexpr std::string_view diffText =
            "diff --git a/logo.png b/logo.png\n"
            "index 1111111..2222222 100644\n"
            "Binary files a/logo.png and b/logo.png differ\n";

        auto diffOpt = gf::git::bl::UnifiedDiffParser::parseSingle(diffText);
        REQUIRE(diffOpt.has_value());
        CHECK(diffOpt->isBinary);
        CHECK(diffOpt->oldPath == "logo.png");
        CHECK(diffOpt->newPath == "logo.png");
        CHECK(diffOpt->hunks.empty());
    }

    SECTION("Parse stream multi-file") {
        constexpr std::string_view multiDiff =
            "diff --git a/file1.txt b/file1.txt\n"
            "--- a/file1.txt\n"
            "+++ b/file1.txt\n"
            "@@ -1 +1 @@\n"
            "-a\n"
            "+b\n"
            "diff --git a/file2.txt b/file2.txt\n"
            "--- a/file2.txt\n"
            "+++ b/file2.txt\n"
            "@@ -1 +1 @@\n"
            "+line\n";

        auto all = gf::git::bl::UnifiedDiffParser::parse(multiDiff);
        REQUIRE(all.size() == 2);
        CHECK(all[0].newPath == "file1.txt");
        CHECK(all[1].newPath == "file2.txt");
    }
}

TEST_CASE("GitDiffService validates paths and delegates queries", "[git][diff][service]") {
    MockDiffGitOps mockOps;
    gf::git::bl::GitDiffService service(mockOps);

    auto validTempDir = std::filesystem::temp_directory_path();

    SECTION("Rifiuta repository path vuoto o non esistente") {
        auto resEmpty = service.getFileDiff(std::filesystem::path{}, "file.txt", gf::git::bo::DiffScope::Unstaged);
        REQUIRE_FALSE(resEmpty.has_value());
        CHECK(resEmpty.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto resNonExist = service.getFileDiff("/invalid/repo/123_gf", "file.txt", gf::git::bo::DiffScope::Unstaged);
        REQUIRE_FALSE(resNonExist.has_value());
        CHECK(resNonExist.error().kind == gf::git::ports::GitOpsError::Kind::PathNotFound);
    }

    SECTION("Rifiuta file path vuoto, assoluto o con traversal") {
        auto resEmpty = service.getFileDiff(validTempDir, "", gf::git::bo::DiffScope::Unstaged);
        REQUIRE_FALSE(resEmpty.has_value());
        CHECK(resEmpty.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto resAbs = service.getFileDiff(validTempDir, "/etc/hosts", gf::git::bo::DiffScope::Unstaged);
        REQUIRE_FALSE(resAbs.has_value());
        CHECK(resAbs.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto resTrav = service.getFileDiff(validTempDir, "dir/../../secret", gf::git::bo::DiffScope::Unstaged);
        REQUIRE_FALSE(resTrav.has_value());
        CHECK(resTrav.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);
    }

    SECTION("Delega corretta di getFileDiff e shortcut") {
        auto res = service.getStagedFileDiff(validTempDir, "src/main.cpp");
        REQUIRE(res.has_value());
        CHECK(mockOps.getFileDiffCalled);
        CHECK(mockOps.lastFilePath == "src/main.cpp");
        CHECK(mockOps.lastScope == gf::git::bo::DiffScope::Staged);

        auto resUnstaged = service.getUnstagedFileDiff(validTempDir, "README.md");
        REQUIRE(resUnstaged.has_value());
        CHECK(mockOps.lastScope == gf::git::bo::DiffScope::Unstaged);

        auto resUntracked = service.getUntrackedFileDiff(validTempDir, "new.txt");
        REQUIRE(resUntracked.has_value());
        CHECK(mockOps.lastScope == gf::git::bo::DiffScope::Untracked);
    }

    SECTION("Delega corretta di getAllDiffs") {
        auto res = service.getAllDiffs(validTempDir, gf::git::bo::DiffScope::Staged);
        REQUIRE(res.has_value());
        CHECK(mockOps.getAllDiffsCalled);
        CHECK(mockOps.lastScope == gf::git::bo::DiffScope::Staged);
        CHECK(res->size() == 1);
    }
}


TEST_CASE("GitDiffService valida e delega getFileDiff con contextLines, getCommitFileDiff e getFileContent", "[git][diff][service][context]") {
    MockDiffGitOps mockOps;
    gf::git::bl::GitDiffService service(mockOps);
    auto validTempDir = std::filesystem::temp_directory_path();

    SECTION("Overload a 4 argomenti di getFileDiff delega tramite IRepositoryGitOps") {
        auto res = service.getFileDiff(validTempDir, "README.md", gf::git::bo::DiffScope::Unstaged, 25);
        REQUIRE(res.has_value());
        CHECK(mockOps.getFileDiffCalled);
        CHECK(mockOps.lastFilePath == "README.md");
    }

    SECTION("getFileDiff con contesto personalizzato rifiuta traversal e path assoluti") {
        auto bad = service.getFileDiff(validTempDir, "../secret.md", gf::git::bo::DiffScope::Unstaged, 10);
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);
    }

    SECTION("getCommitFileDiff e getFileContent validano i parametri") {
        auto badRepo = service.getFileContent(std::filesystem::path{}, "HEAD", "README.md");
        REQUIRE_FALSE(badRepo.has_value());
        CHECK(badRepo.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto badPath = service.getFileContent(validTempDir, "HEAD", "../evil.txt");
        REQUIRE_FALSE(badPath.has_value());
        CHECK(badPath.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto okContent = service.getFileContent(validTempDir, "HEAD", "README.md");
        REQUIRE(okContent.has_value());

        auto okCommitDiff = service.getCommitFileDiff(validTempDir, "HEAD", "README.md", 10);
        REQUIRE(okCommitDiff.has_value());
    }
}
