#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bo/repo_working_tree_status.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "core/modules/git/ports/git_command_executor.hpp"

#include <filesystem>
#include <memory>

using namespace gf::git::bo;
using namespace gf::git::ports;
using namespace gf::git::bl;
using namespace gf::infra::git;

namespace
{
    class MockGitCommandExecutor final : public IGitCommandExecutor
    {
    public:
        std::string stdoutToReturn;
        std::string stderrToReturn;
        int exitCodeToReturn = 0;
        mutable GitCommand lastCommand;

        [[nodiscard]] std::string_view adapterName() const noexcept override
        {
            return "mock-executor";
        }

        using IGitCommandExecutor::execute;

        [[nodiscard]] std::expected<GitProcessResult, GitExecutionError>
        execute(const GitCommand& command, std::stop_token) override
        {
            lastCommand = command;
            return GitProcessResult{
                .exitCode = exitCodeToReturn,
                .stdoutBytes = stdoutToReturn,
                .stderrBytes = stderrToReturn,
                .elapsed = std::chrono::microseconds{1000},
                .cancelled = false,
                .timedOut = false
            };
        }
    };

    class MockRepositoryGitOps final : public IRepositoryGitOps
    {
    public:
        std::expected<RepoWorkingTreeStatus, GitOpsError> resultToReturn{RepoWorkingTreeStatus{}};
        std::filesystem::path lastQueriedPath;

        [[nodiscard]] std::expected<RepoWorkingTreeStatus, GitOpsError>
        getWorkingTreeStatus(const std::filesystem::path& repoPath) override
        {
            lastQueriedPath = repoPath;
            return resultToReturn;
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        stageFiles(const std::filesystem::path&, const std::vector<std::string>&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        stageAll(const std::filesystem::path&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        unstageFiles(const std::filesystem::path&, const std::vector<std::string>&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        unstageAll(const std::filesystem::path&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        discardFiles(const std::filesystem::path&, const std::vector<std::string>&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<CommitResult, GitOpsError>
        createCommit(const std::filesystem::path&, const CommitOptions&) override
        {
            return CommitResult{};
        }

        [[nodiscard]] std::expected<FileDiff, GitOpsError>
        getFileDiff(const std::filesystem::path&,
                    const std::string&,
                    DiffScope) override
        {
            return FileDiff{};
        }

        [[nodiscard]] std::expected<std::vector<FileDiff>, GitOpsError>
        getAllDiffs(const std::filesystem::path&,
                    DiffScope) override
        {
            return std::vector<FileDiff>{};
        }

        [[nodiscard]] std::expected<std::vector<GitBranch>, GitOpsError>
        listBranches(const std::filesystem::path&) override
        {
            return std::vector<GitBranch>{};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        createBranch(const std::filesystem::path&,
                     const CreateBranchOptions&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        checkoutBranch(const std::filesystem::path&,
                       const CheckoutOptions&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        deleteBranch(const std::filesystem::path&,
                     const std::string&,
                     bool) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        renameBranch(const std::filesystem::path&,
                     const std::string&,
                     const std::string&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<std::vector<GitCommitLogItem>, GitOpsError>
        getCommitLog(const std::filesystem::path&, const CommitLogOptions&) override
        {
            return std::vector<GitCommitLogItem>{};
        }

        [[nodiscard]] std::expected<std::vector<GitTag>, GitOpsError>
        listTags(const std::filesystem::path&) override
        {
            return std::vector<GitTag>{};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        createTag(const std::filesystem::path&, const CreateTagOptions&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        deleteTag(const std::filesystem::path&, const std::string&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<std::vector<GitRemote>, GitOpsError>
        listRemotes(const std::filesystem::path&) override
        {
            return std::vector<GitRemote>{};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        addRemote(const std::filesystem::path&, const std::string&, const std::string&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        removeRemote(const std::filesystem::path&, const std::string&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<RemoteOperationResult, GitOpsError>
        fetchRemote(const std::filesystem::path&, const RemoteOperationOptions&) override
        {
            return RemoteOperationResult{.success = true};
        }

        [[nodiscard]] std::expected<RemoteOperationResult, GitOpsError>
        pullRemote(const std::filesystem::path&, const RemoteOperationOptions&) override
        {
            return RemoteOperationResult{.success = true};
        }

        [[nodiscard]] std::expected<RemoteOperationResult, GitOpsError>
        pushRemote(const std::filesystem::path&, const RemoteOperationOptions&) override
        {
            return RemoteOperationResult{.success = true};
        }

        [[nodiscard]] std::expected<MergeResult, GitOpsError>
        mergeBranch(const std::filesystem::path&, const MergeOptions&) override
        {
            return MergeResult{.status = MergeStatus::Merged};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        abortMerge(const std::filesystem::path&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<std::vector<GitStashItem>, GitOpsError>
        listStashes(const std::filesystem::path&) override
        {
            return std::vector<GitStashItem>{};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        stashPush(const std::filesystem::path&, const StashPushOptions&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        stashPop(const std::filesystem::path&, const StashApplyOptions&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        stashApply(const std::filesystem::path&, const StashApplyOptions&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        stashDrop(const std::filesystem::path&, std::size_t) override
        {
            return {};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        stashClear(const std::filesystem::path&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<RebaseResult, GitOpsError>
        startRebase(const std::filesystem::path&, const RebaseOptions&) override
        {
            return RebaseResult{.status = RebaseStatus::Applied};
        }

        [[nodiscard]] std::expected<RebaseResult, GitOpsError>
        continueRebase(const std::filesystem::path&) override
        {
            return RebaseResult{.status = RebaseStatus::Finished};
        }

        [[nodiscard]] std::expected<RebaseResult, GitOpsError>
        skipRebase(const std::filesystem::path&) override
        {
            return RebaseResult{.status = RebaseStatus::Skipped};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        abortRebase(const std::filesystem::path&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<bool, GitOpsError>
        isRebaseInProgress(const std::filesystem::path&) override
        {
            return false;
        }

        [[nodiscard]] std::expected<CherryPickResult, GitOpsError>
        cherryPick(const std::filesystem::path&, const CherryPickOptions&) override
        {
            return CherryPickResult{.success = true};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        abortCherryPick(const std::filesystem::path&) override
        {
            return {};
        }

        [[nodiscard]] std::expected<ConflictFile, GitOpsError>
        getConflictFile(const std::filesystem::path&, const std::string&) override
        {
            return ConflictFile{};
        }

        [[nodiscard]] std::expected<std::string, GitOpsError>
        getFileStageContent(const std::filesystem::path&, const std::string&, int) override
        {
            return std::string{};
        }

        [[nodiscard]] std::expected<void, GitOpsError>
        applyConflictResolution(const std::filesystem::path&,
                                const ApplyConflictResolutionOptions&) override
        {
            return {};
        }
    };
} // namespace

TEST_CASE (
"RepoWorkingTreeStatus calculates summary statistics correctly"
,
"[git][status]"
)
 {
    RepoWorkingTreeStatus status;
    CHECK(status.isClean());
    CHECK(status.totalChangesCount() == 0);
    CHECK(status.stagedCount() == 0);
    CHECK(status.unstagedCount() == 0);
    CHECK(status.untrackedCount() == 0);
    CHECK(status.conflictedCount() == 0);

    status.stagedFiles.push_back(WorkingTreeFileItem{
        .path = "src/main.cpp",
        .originalPath = {},
        .kind = FileChangeKind::Modified,
        .statusCode = 'M'
    });

    status.unstagedFiles.push_back(WorkingTreeFileItem{
        .path = "README.md",
        .originalPath = {},
        .kind = FileChangeKind::Modified,
        .statusCode = 'M'
    });

    status.untrackedFiles.push_back(WorkingTreeFileItem{
        .path = "scratch.txt",
        .originalPath = {},
        .kind = FileChangeKind::Untracked,
        .statusCode = '?'
    });

    status.conflictedFiles.push_back(WorkingTreeFileItem{
        .path = "config.json",
        .originalPath = {},
        .kind = FileChangeKind::Conflicted,
        .statusCode = 'U'
    });

    CHECK_FALSE(status.isClean());
    CHECK(status.totalChangesCount() == 4);
    CHECK(status.stagedCount() == 1);
    CHECK(status.unstagedCount() == 1);
    CHECK(status.untrackedCount() == 1);
    CHECK(status.conflictedCount() == 1);
}

TEST_CASE (
"GitWorkingTreeService validates paths and queries operations"
,
"[git][service]"
)
 {
    MockRepositoryGitOps mockOps;
    GitWorkingTreeService service(mockOps);

    SECTION("Empty path fails with InvalidRepositoryPath") {
        auto result = service.getWorkingTreeStatus(std::filesystem::path{});
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().kind == GitOpsError::Kind::InvalidRepositoryPath);
    }

    SECTION("Non-existent path fails with PathNotFound") {
        auto result = service.getWorkingTreeStatus("/path/that/does/not/exist_12345");
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().kind == GitOpsError::Kind::PathNotFound);
    }

    SECTION("Valid temp path delegates to git ops and evaluates clean and branch") {
        const auto tempPath = std::filesystem::temp_directory_path();

        RepoWorkingTreeStatus fakeStatus;
        fakeStatus.branch = GitBranchHead{
            .oid = "a1b2c3d4e5",
            .headName = "feature/phase2",
            .upstream = "origin/feature/phase2",
            .ahead = 2,
            .behind = 1,
            .detached = false,
            .unborn = false
        };
        mockOps.resultToReturn = fakeStatus;

        auto statusRes = service.getWorkingTreeStatus(tempPath);
        REQUIRE(statusRes.has_value());
        CHECK(statusRes->isClean());
        CHECK(statusRes->branch.headName == "feature/phase2");
        CHECK(statusRes->branch.ahead == 2);
        CHECK(statusRes->branch.behind == 1);

        auto cleanRes = service.isRepositoryClean(tempPath);
        REQUIRE(cleanRes.has_value());
        CHECK(*cleanRes == true);

        auto branchRes = service.getCurrentBranch(tempPath);
        REQUIRE(branchRes.has_value());
        CHECK(branchRes->headName == "feature/phase2");
        CHECK(branchRes->upstream == "origin/feature/phase2");
    }
}

TEST_CASE (
"NativeRepositoryGitOps parses porcelain v2 and classifies working tree entries"
,
"[git][native]"
)
 {
    MockGitCommandExecutor mockExecutor;
    NativeRepositoryGitOps gitOps(mockExecutor);

    std::string payload;
    auto addRec = [&](const std::string_view s) {
        payload.append(s);
        payload.push_back('\0');
    };

    addRec("# branch.oid 4b825dc642cb6eb9a060e54bf8d69288fbee4904");
    addRec("# branch.head main");
    addRec("# branch.upstream origin/main");
    addRec("# branch.ab +3 -0");
    addRec("1 M. N... 100644 100644 100644 aaaaaaa bbbbbbb staged.txt");
    addRec("1 .M N... 100644 100644 100644 ccccccc ddddddd unstaged.txt");
    addRec("1 MM N... 100644 100644 100644 eeeeeee fffffff both.txt");
    addRec("2 R. N... 100644 100644 100644 ggggggg hhhhhhh R100 dest_renamed.txt");
    addRec("orig_renamed.txt");
    addRec("? untracked.cpp");
    addRec("u UU N... 100644 100644 100644 100644 111 222 333 conflict.hpp");

    mockExecutor.stdoutToReturn = payload;
    mockExecutor.exitCodeToReturn = 0;

    auto result = gitOps.getWorkingTreeStatus("/fake/repo");
    REQUIRE(result.has_value());

    const auto& [branch, stagedFiles, unstagedFiles, untrackedFiles, conflictedFiles] = *result;
    // Branch checks
    CHECK(branch.headName == "main");
    CHECK(branch.upstream == "origin/main");
    CHECK(branch.ahead == 3);
    CHECK(branch.behind == 0);

    // Staged checks: staged.txt (M), both.txt (M), dest_renamed.txt (R) -> 3 files
    REQUIRE(stagedFiles.size() == 3);
    CHECK(stagedFiles[0].path == "staged.txt");
    CHECK(stagedFiles[0].kind == FileChangeKind::Modified);

    CHECK(stagedFiles[1].path == "both.txt");
    CHECK(stagedFiles[1].kind == FileChangeKind::Modified);

    CHECK(stagedFiles[2].path == "dest_renamed.txt");
    CHECK(stagedFiles[2].originalPath == "orig_renamed.txt");
    CHECK(stagedFiles[2].kind == FileChangeKind::Renamed);

    // Unstaged checks: unstaged.txt (M), both.txt (M) -> 2 files
    REQUIRE(unstagedFiles.size() == 2);
    CHECK(unstagedFiles[0].path == "unstaged.txt");
    CHECK(unstagedFiles[0].kind == FileChangeKind::Modified);

    CHECK(unstagedFiles[1].path == "both.txt");
    CHECK(unstagedFiles[1].kind == FileChangeKind::Modified);

    // Untracked checks: untracked.cpp -> 1 file
    REQUIRE(untrackedFiles.size() == 1);
    CHECK(untrackedFiles[0].path == "untracked.cpp");
    CHECK(untrackedFiles[0].kind == FileChangeKind::Untracked);

    // Conflicted checks: conflict.hpp -> 1 file
    REQUIRE(conflictedFiles.size() == 1);
    CHECK(conflictedFiles[0].path == "conflict.hpp");
    CHECK(conflictedFiles[0].kind == FileChangeKind::Conflicted);
}
