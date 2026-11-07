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

namespace {

class MockGitCommandExecutor final : public IGitCommandExecutor {
public:
    std::string stdoutToReturn;
    std::string stderrToReturn;
    int exitCodeToReturn = 0;
    mutable GitCommand lastCommand;

    [[nodiscard]] std::string_view adapterName() const noexcept override {
        return "mock-executor";
    }

    [[nodiscard]] std::expected<GitProcessResult, GitExecutionError>
    execute(const GitCommand& command, std::stop_token) override {
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

class MockRepositoryGitOps final : public IRepositoryGitOps {
public:
    std::expected<RepoWorkingTreeStatus, GitOpsError> resultToReturn{RepoWorkingTreeStatus{}};
    std::filesystem::path lastQueriedPath;

    [[nodiscard]] std::expected<RepoWorkingTreeStatus, GitOpsError>
    getWorkingTreeStatus(const std::filesystem::path& repoPath) override {
        lastQueriedPath = repoPath;
        return resultToReturn;
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    stageFiles(const std::filesystem::path&, const std::vector<std::string>&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    stageAll(const std::filesystem::path&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    unstageFiles(const std::filesystem::path&, const std::vector<std::string>&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    unstageAll(const std::filesystem::path&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    discardFiles(const std::filesystem::path&, const std::vector<std::string>&) override {
        return {};
    }

    [[nodiscard]] std::expected<gf::git::bo::CommitResult, GitOpsError>
    createCommit(const std::filesystem::path&, const gf::git::bo::CommitOptions&) override {
        return gf::git::bo::CommitResult{};
    }

    [[nodiscard]] std::expected<gf::git::bo::FileDiff, GitOpsError>
    getFileDiff(const std::filesystem::path&,
                const std::string&,
                gf::git::bo::DiffScope) override {
        return gf::git::bo::FileDiff{};
    }

    [[nodiscard]] std::expected<std::vector<gf::git::bo::FileDiff>, GitOpsError>
    getAllDiffs(const std::filesystem::path&,
                gf::git::bo::DiffScope) override {
        return std::vector<gf::git::bo::FileDiff>{};
    }

    [[nodiscard]] std::expected<std::vector<gf::git::bo::GitBranch>, GitOpsError>
    listBranches(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitBranch>{};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    createBranch(const std::filesystem::path&,
                 const gf::git::bo::CreateBranchOptions&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    checkoutBranch(const std::filesystem::path&,
                   const gf::git::bo::CheckoutOptions&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    deleteBranch(const std::filesystem::path&,
                 const std::string&,
                 bool) override {
        return {};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    renameBranch(const std::filesystem::path&,
                 const std::string&,
                 const std::string&) override {
        return {};
    }

    [[nodiscard]] std::expected<std::vector<gf::git::bo::GitCommitLogItem>, GitOpsError>
    getCommitLog(const std::filesystem::path&, const gf::git::bo::CommitLogOptions&) override {
        return std::vector<gf::git::bo::GitCommitLogItem>{};
    }

    [[nodiscard]] std::expected<std::vector<gf::git::bo::GitTag>, GitOpsError>
    listTags(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitTag>{};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    createTag(const std::filesystem::path&, const gf::git::bo::CreateTagOptions&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    deleteTag(const std::filesystem::path&, const std::string&) override {
        return {};
    }

    [[nodiscard]] std::expected<std::vector<gf::git::bo::GitRemote>, GitOpsError>
    listRemotes(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitRemote>{};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    addRemote(const std::filesystem::path&, const std::string&, const std::string&) override {
        return {};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    removeRemote(const std::filesystem::path&, const std::string&) override {
        return {};
    }

    [[nodiscard]] std::expected<gf::git::bo::RemoteOperationResult, GitOpsError>
    fetchRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override {
        return gf::git::bo::RemoteOperationResult{.success = true};
    }

    [[nodiscard]] std::expected<gf::git::bo::RemoteOperationResult, GitOpsError>
    pullRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override {
        return gf::git::bo::RemoteOperationResult{.success = true};
    }

    [[nodiscard]] std::expected<gf::git::bo::RemoteOperationResult, GitOpsError>
    pushRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override {
        return gf::git::bo::RemoteOperationResult{.success = true};
    }

    [[nodiscard]] std::expected<gf::git::bo::MergeResult, GitOpsError>
    mergeBranch(const std::filesystem::path&, const gf::git::bo::MergeOptions&) override {
        return gf::git::bo::MergeResult{.status = gf::git::bo::MergeStatus::Merged};
    }

    [[nodiscard]] std::expected<void, GitOpsError>
    abortMerge(const std::filesystem::path&) override {
        return {};
    }
};

} // namespace

TEST_CASE("RepoWorkingTreeStatus calculates summary statistics correctly", "[git][status]") {
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

TEST_CASE("GitWorkingTreeService validates paths and queries operations", "[git][service]") {
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

TEST_CASE("NativeRepositoryGitOps parses porcelain v2 and classifies working tree entries", "[git][native]") {
    MockGitCommandExecutor mockExecutor;
    NativeRepositoryGitOps gitOps(mockExecutor);

    std::string payload;
    auto addRec = [&](std::string_view s) {
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

    const auto& status = *result;
    // Branch checks
    CHECK(status.branch.headName == "main");
    CHECK(status.branch.upstream == "origin/main");
    CHECK(status.branch.ahead == 3);
    CHECK(status.branch.behind == 0);

    // Staged checks: staged.txt (M), both.txt (M), dest_renamed.txt (R) -> 3 files
    REQUIRE(status.stagedFiles.size() == 3);
    CHECK(status.stagedFiles[0].path == "staged.txt");
    CHECK(status.stagedFiles[0].kind == FileChangeKind::Modified);

    CHECK(status.stagedFiles[1].path == "both.txt");
    CHECK(status.stagedFiles[1].kind == FileChangeKind::Modified);

    CHECK(status.stagedFiles[2].path == "dest_renamed.txt");
    CHECK(status.stagedFiles[2].originalPath == "orig_renamed.txt");
    CHECK(status.stagedFiles[2].kind == FileChangeKind::Renamed);

    // Unstaged checks: unstaged.txt (M), both.txt (M) -> 2 files
    REQUIRE(status.unstagedFiles.size() == 2);
    CHECK(status.unstagedFiles[0].path == "unstaged.txt");
    CHECK(status.unstagedFiles[0].kind == FileChangeKind::Modified);

    CHECK(status.unstagedFiles[1].path == "both.txt");
    CHECK(status.unstagedFiles[1].kind == FileChangeKind::Modified);

    // Untracked checks: untracked.cpp -> 1 file
    REQUIRE(status.untrackedFiles.size() == 1);
    CHECK(status.untrackedFiles[0].path == "untracked.cpp");
    CHECK(status.untrackedFiles[0].kind == FileChangeKind::Untracked);

    // Conflicted checks: conflict.hpp -> 1 file
    REQUIRE(status.conflictedFiles.size() == 1);
    CHECK(status.conflictedFiles[0].path == "conflict.hpp");
    CHECK(status.conflictedFiles[0].kind == FileChangeKind::Conflicted);
}
