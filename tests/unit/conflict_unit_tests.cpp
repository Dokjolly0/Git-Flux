#include "core/modules/git/bl/conflict_marker_parser.hpp"
#include "core/modules/git/bl/git_conflict_service.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    class MockConflictGitOps final : public gf::git::ports::IRepositoryGitOps
    {
    public:
        bool getConflictFileCalled = false;
        bool getFileStageContentCalled = false;
        bool applyConflictResolutionCalled = false;

        std::string lastConflictFilePath;
        int lastStageRequested = 0;
        gf::git::bo::ApplyConflictResolutionOptions lastApplyOptions;

        gf::git::bo::ConflictFile conflictFileToReturn;
        std::string stageContentToReturn;
        bool failOperations = false;

        std::expected<gf::git::bo::ConflictFile, gf::git::ports::GitOpsError>
        getConflictFile(const std::filesystem::path&, const std::string& filePath) override
        {
            getConflictFileCalled = true;
            lastConflictFilePath = filePath;
            if (failOperations)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::PathNotFound,
                    .exitCode = 1,
                    .message = "File not found",
                    .command = "conflict"
                });
            }
            return conflictFileToReturn;
        }

        std::expected<std::string, gf::git::ports::GitOpsError>
        getFileStageContent(const std::filesystem::path&, const std::string& filePath, int stage) override
        {
            getFileStageContentCalled = true;
            lastConflictFilePath = filePath;
            lastStageRequested = stage;
            if (failOperations)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::ExecutionFailed,
                    .exitCode = 1,
                    .message = "Failed stage content",
                    .command = "git show"
                });
            }
            return stageContentToReturn;
        }

        std::expected<void, gf::git::ports::GitOpsError>
        applyConflictResolution(const std::filesystem::path&,
                                const gf::git::bo::ApplyConflictResolutionOptions& options) override
        {
            applyConflictResolutionCalled = true;
            lastApplyOptions = options;
            if (failOperations)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::StorageError,
                    .exitCode = 1,
                    .message = "Failed writing file",
                    .command = "conflict-resolve"
                });
            }
            return {};
        }

        // Methods not relevant for this mock
        std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
        getWorkingTreeStatus(const std::filesystem::path&) override { return gf::git::bo::RepoWorkingTreeStatus{}; }

        std::expected<void, gf::git::ports::GitOpsError> stageFiles(const std::filesystem::path&,
                                                                    const std::vector<std::string>&) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError> stageAll(const std::filesystem::path&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError> unstageFiles(const std::filesystem::path&,
                                                                      const std::vector<std::string>&) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError> unstageAll(const std::filesystem::path&) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError> discardFiles(const std::filesystem::path&,
                                                                      const std::vector<std::string>&) override
        {
            return {};
        }

        std::expected<gf::git::bo::CommitResult, gf::git::ports::GitOpsError> createCommit(
            const std::filesystem::path&, const gf::git::bo::CommitOptions&) override
        {
            return gf::git::bo::CommitResult{};
        }

        std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError> getFileDiff(
            const std::filesystem::path&, const std::string&, gf::git::bo::DiffScope) override
        {
            return gf::git::bo::FileDiff{};
        }

        std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError> getAllDiffs(
            const std::filesystem::path&, gf::git::bo::DiffScope) override
        {
            return std::vector<gf::git::bo::FileDiff>{};
        }

        std::expected<std::vector<gf::git::bo::GitBranch>, gf::git::ports::GitOpsError>
        listBranches(const std::filesystem::path&) override { return std::vector<gf::git::bo::GitBranch>{}; }

        std::expected<void, gf::git::ports::GitOpsError> createBranch(const std::filesystem::path&,
                                                                      const gf::git::bo::CreateBranchOptions&) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError> checkoutBranch(const std::filesystem::path&,
                                                                        const gf::git::bo::CheckoutOptions&) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError> deleteBranch(const std::filesystem::path&, const std::string&,
                                                                      bool) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError> renameBranch(const std::filesystem::path&, const std::string&,
                                                                      const std::string&) override { return {}; }

        std::expected<std::vector<gf::git::bo::GitCommitLogItem>, gf::git::ports::GitOpsError> getCommitLog(
            const std::filesystem::path&, const gf::git::bo::CommitLogOptions&) override
        {
            return std::vector<gf::git::bo::GitCommitLogItem>{};
        }

        std::expected<std::vector<gf::git::bo::GitTag>, gf::git::ports::GitOpsError>
        listTags(const std::filesystem::path&) override { return std::vector<gf::git::bo::GitTag>{}; }

        std::expected<void, gf::git::ports::GitOpsError> createTag(const std::filesystem::path&,
                                                                   const gf::git::bo::CreateTagOptions&) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError>
        deleteTag(const std::filesystem::path&, const std::string&) override { return {}; }

        std::expected<std::vector<gf::git::bo::GitRemote>, gf::git::ports::GitOpsError>
        listRemotes(const std::filesystem::path&) override { return std::vector<gf::git::bo::GitRemote>{}; }

        std::expected<void, gf::git::ports::GitOpsError> addRemote(const std::filesystem::path&, const std::string&,
                                                                   const std::string&) override { return {}; }

        std::expected<void, gf::git::ports::GitOpsError>
        removeRemote(const std::filesystem::path&, const std::string&) override { return {}; }

        std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError> fetchRemote(
            const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override
        {
            return gf::git::bo::RemoteOperationResult{.success = true};
        }

        std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError> pullRemote(
            const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override
        {
            return gf::git::bo::RemoteOperationResult{.success = true};
        }

        std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError> pushRemote(
            const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override
        {
            return gf::git::bo::RemoteOperationResult{.success = true};
        }

        std::expected<gf::git::bo::MergeResult, gf::git::ports::GitOpsError>
        mergeBranch(const std::filesystem::path&, const gf::git::bo::MergeOptions&) override
        {
            return gf::git::bo::MergeResult{.status = gf::git::bo::MergeStatus::Merged};
        }

        std::expected<void, gf::git::ports::GitOpsError> abortMerge(const std::filesystem::path&) override
        {
            return {};
        }

        std::expected<std::vector<gf::git::bo::GitStashItem>, gf::git::ports::GitOpsError>
        listStashes(const std::filesystem::path&) override { return std::vector<gf::git::bo::GitStashItem>{}; }

        std::expected<void, gf::git::ports::GitOpsError> stashPush(const std::filesystem::path&,
                                                                   const gf::git::bo::StashPushOptions&) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError> stashPop(const std::filesystem::path&,
                                                                  const gf::git::bo::StashApplyOptions&) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError> stashApply(const std::filesystem::path&,
                                                                    const gf::git::bo::StashApplyOptions&) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError> stashDrop(const std::filesystem::path&, std::size_t) override
        {
            return {};
        }

        std::expected<void, gf::git::ports::GitOpsError> stashClear(const std::filesystem::path&) override
        {
            return {};
        }

        std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
        startRebase(const std::filesystem::path&, const gf::git::bo::RebaseOptions&) override
        {
            return gf::git::bo::RebaseResult{.status = gf::git::bo::RebaseStatus::Applied};
        }

        std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
        continueRebase(const std::filesystem::path&) override
        {
            return gf::git::bo::RebaseResult{.status = gf::git::bo::RebaseStatus::Finished};
        }

        std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
        skipRebase(const std::filesystem::path&) override
        {
            return gf::git::bo::RebaseResult{.status = gf::git::bo::RebaseStatus::Skipped};
        }

        std::expected<void, gf::git::ports::GitOpsError> abortRebase(const std::filesystem::path&) override
        {
            return {};
        }

        std::expected<bool, gf::git::ports::GitOpsError> isRebaseInProgress(const std::filesystem::path&) override
        {
            return false;
        }

        std::expected<gf::git::bo::CherryPickResult, gf::git::ports::GitOpsError> cherryPick(
            const std::filesystem::path&, const gf::git::bo::CherryPickOptions&) override
        {
            return gf::git::bo::CherryPickResult{.success = true};
        }

        std::expected<void, gf::git::ports::GitOpsError> abortCherryPick(const std::filesystem::path&) override
        {
            return {};
        }
    };
} // namespace

TEST_CASE (
"ConflictMarkerParser parses Git conflict blocks and reconstructs content"
,
"[git][conflict][parser]"
)
 {
    using gf::git::bl::ConflictMarkerParser;
    using gf::git::bo::ConflictSide;

    SECTION("Stringa vuota e stringa senza conflitti") {
        CHECK(ConflictMarkerParser::parseHunks("").empty());
        CHECK_FALSE(ConflictMarkerParser::hasConflictMarkers("int main() { return 0; }\n"));

        auto emptyHunks = ConflictMarkerParser::parseHunks("line 1\nline 2\nline 3\n");
        CHECK(emptyHunks.empty());
    }

    SECTION("Conflitto standard a 2 vie (Ours vs Theirs)") {
        constexpr std::string_view conflictText =
            "pre-context\n"
            "<<<<<<< HEAD\n"
            "our changes line 1\n"
            "our changes line 2\n"
            "=======\n"
            "their changes line A\n"
            ">>>>>>> feature/auth\n"
            "post-context\n";

        CHECK(ConflictMarkerParser::hasConflictMarkers(conflictText));
        auto hunks = ConflictMarkerParser::parseHunks(conflictText);
        REQUIRE(hunks.size() == 1);
        CHECK(hunks[0].hunkIndex == 0);
        CHECK(hunks[0].startLine == 2);
        CHECK(hunks[0].oursContent == "our changes line 1\nour changes line 2\n");
        CHECK(hunks[0].theirsContent == "their changes line A\n");
        CHECK(hunks[0].baseContent.empty());

        // Risolvi scegliendo Ours
        hunks[0].isResolved = true;
        hunks[0].chosenSide = ConflictSide::Ours;
        auto recOurs = ConflictMarkerParser::reconstruct(conflictText, hunks);
        REQUIRE(recOurs.has_value());
        CHECK(*recOurs == "pre-context\nour changes line 1\nour changes line 2\npost-context\n");

        // Resolve choosing Theirs
        hunks[0].chosenSide = ConflictSide::Theirs;
        auto recTheirs = ConflictMarkerParser::reconstruct(conflictText, hunks);
        REQUIRE(recTheirs.has_value());
        CHECK(*recTheirs == "pre-context\ntheir changes line A\npost-context\n");

        // Resolve with Custom text
        hunks[0].chosenSide = ConflictSide::Custom;
        hunks[0].customContent = "custom merged content\n";
        auto recCustom = ConflictMarkerParser::reconstruct(conflictText, hunks);
        REQUIRE(recCustom.has_value());
        CHECK(*recCustom == "pre-context\ncustom merged content\npost-context\n");
    }

    SECTION("Conflitto a 3 vie stile diff3 (Base, Ours, Theirs)") {
        constexpr std::string_view diff3Text =
            "start\n"
            "<<<<<<< HEAD\n"
            "ours\n"
            "||||||| merged common ancestors\n"
            "base\n"
            "=======\n"
            "theirs\n"
            ">>>>>>> branch\n"
            "end\n";

        auto hunks = ConflictMarkerParser::parseHunks(diff3Text);
        REQUIRE(hunks.size() == 1);
        CHECK(hunks[0].oursContent == "ours\n");
        CHECK(hunks[0].baseContent == "base\n");
        CHECK(hunks[0].theirsContent == "theirs\n");

        // Resolve choosing Base
        hunks[0].isResolved = true;
        hunks[0].chosenSide = ConflictSide::Base;
        auto recBase = ConflictMarkerParser::reconstruct(diff3Text, hunks);
        REQUIRE(recBase.has_value());
        CHECK(*recBase == "start\nbase\nend\n");
    }

    SECTION("Molteplici blocchi di conflitto nello stesso file") {
        constexpr std::string_view multiText =
            "header\n"
            "<<<<<<< HEAD\n"
            "block1 ours\n"
            "=======\n"
            "block1 theirs\n"
            ">>>>>>> feat\n"
            "middle\n"
            "<<<<<<< HEAD\n"
            "block2 ours\n"
            "=======\n"
            "block2 theirs\n"
            ">>>>>>> feat\n"
            "footer\n";

        auto hunks = ConflictMarkerParser::parseHunks(multiText);
        REQUIRE(hunks.size() == 2);
        CHECK(hunks[0].hunkIndex == 0);
        CHECK(hunks[1].hunkIndex == 1);

        // Error if a hunk is unresolved
        hunks[0].isResolved = true;
        hunks[0].chosenSide = ConflictSide::Ours;
        hunks[1].isResolved = false;

        auto errRes = ConflictMarkerParser::reconstruct(multiText, hunks);
        REQUIRE_FALSE(errRes.has_value());

        // Resolve the second hunk as well
        hunks[1].isResolved = true;
        hunks[1].chosenSide = ConflictSide::Theirs;
        auto succRes = ConflictMarkerParser::reconstruct(multiText, hunks);
        REQUIRE(succRes.has_value());
        CHECK(*succRes == "header\nblock1 ours\nmiddle\nblock2 theirs\nfooter\n");
    }
}

TEST_CASE (
"GitConflictService validates parameters and delegates to IRepositoryGitOps"
,
"[git][conflict][service]"
)
 {
    MockConflictGitOps mockOps;
    gf::git::bl::GitConflictService service(mockOps);
    auto validPath = std::filesystem::temp_directory_path();

    SECTION("Validazione percorso repository") {
        auto res = service.getConflictFile("", "file.txt");
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto stageRes = service.getFileStageContent("", "file.txt", 1);
        REQUIRE_FALSE(stageRes.has_value());
        CHECK(stageRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);

        auto applyRes = service.applyConflictResolution("", {.filePath = "file.txt"});
        REQUIRE_FALSE(applyRes.has_value());
        CHECK(applyRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidRepositoryPath);
    }

    SECTION("Validazione parametri file e stage") {
        auto emptyFile = service.getConflictFile(validPath, "");
        REQUIRE_FALSE(emptyFile.has_value());
        CHECK(emptyFile.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);

        auto invalidStage0 = service.getFileStageContent(validPath, "file.txt", 0);
        REQUIRE_FALSE(invalidStage0.has_value());
        CHECK(invalidStage0.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);

        auto invalidStage4 = service.getFileStageContent(validPath, "file.txt", 4);
        REQUIRE_FALSE(invalidStage4.has_value());
        CHECK(invalidStage4.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);

        auto emptyApply = service.applyConflictResolution(validPath, {.filePath = ""});
        REQUIRE_FALSE(emptyApply.has_value());
        CHECK(emptyApply.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);
    }

    SECTION("Delega a IRepositoryGitOps") {
        mockOps.conflictFileToReturn.filePath = "src/app.cpp";
        mockOps.conflictFileToReturn.isBinary = false;
        mockOps.conflictFileToReturn.hunks = {
            gf::git::bo::ConflictHunk{.hunkIndex = 0, .startLine = 5, .isResolved = true}
        };

        auto cFile = service.getConflictFile(validPath, "src/app.cpp");
        REQUIRE(cFile.has_value());
        CHECK(mockOps.getConflictFileCalled);
        CHECK(mockOps.lastConflictFilePath == "src/app.cpp");
        CHECK(cFile->totalHunks() == 1);
        CHECK(cFile->allHunksResolved() == true);

        mockOps.stageContentToReturn = "stage 2 ours text";
        auto sContent = service.getFileStageContent(validPath, "src/app.cpp", 2);
        REQUIRE(sContent.has_value());
        CHECK(mockOps.getFileStageContentCalled);
        CHECK(mockOps.lastStageRequested == 2);
        CHECK(*sContent == "stage 2 ours text");

        gf::git::bo::ApplyConflictResolutionOptions applyOpts{
            .filePath = "src/app.cpp",
            .resolvedContent = "clean resolved file",
            .autoStage = true
        };
        auto applyRes = service.applyConflictResolution(validPath, applyOpts);
        REQUIRE(applyRes.has_value());
        CHECK(mockOps.applyConflictResolutionCalled);
        CHECK(mockOps.lastApplyOptions.filePath == "src/app.cpp");
        CHECK(mockOps.lastApplyOptions.autoStage == true);
    }
}
