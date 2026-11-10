#include "core/modules/git/bl/git_clone_service.hpp"
#include "infrastructure/persistence/repositories/sqlite_repository_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace
{
    class MockCloneGitOps final : public gf::git::ports::IRepositoryGitOps
    {
    public:
        bool cloneCalled = false;
        gf::git::bo::CloneRepositoryOptions lastOptions;
        std::expected<gf::git::bo::CloneRepositoryResult, gf::git::ports::GitOpsError> resultToReturn =
            gf::git::bo::CloneRepositoryResult{};

        std::expected<gf::git::bo::CloneRepositoryResult, gf::git::ports::GitOpsError>
        cloneRepository(const gf::git::bo::CloneRepositoryOptions& options) override
        {
            cloneCalled = true;
            lastOptions = options;
            return resultToReturn;
        }

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
            return gf::git::bo::RemoteOperationResult{};
        }

        std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError> pullRemote(
            const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override
        {
            return gf::git::bo::RemoteOperationResult{};
        }

        std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError> pushRemote(
            const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override
        {
            return gf::git::bo::RemoteOperationResult{};
        }

        std::expected<gf::git::bo::MergeResult, gf::git::ports::GitOpsError> mergeBranch(
            const std::filesystem::path&, const gf::git::bo::MergeOptions&) override
        {
            return gf::git::bo::MergeResult{};
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

        std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError> startRebase(
            const std::filesystem::path&, const gf::git::bo::RebaseOptions&) override
        {
            return gf::git::bo::RebaseResult{};
        }

        std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
        continueRebase(const std::filesystem::path&) override { return gf::git::bo::RebaseResult{}; }

        std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
        skipRebase(const std::filesystem::path&) override { return gf::git::bo::RebaseResult{}; }

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
            return gf::git::bo::CherryPickResult{};
        }

        std::expected<void, gf::git::ports::GitOpsError> abortCherryPick(const std::filesystem::path&) override
        {
            return {};
        }

        std::expected<gf::git::bo::ConflictFile, gf::git::ports::GitOpsError> getConflictFile(
            const std::filesystem::path&, const std::string&) override { return gf::git::bo::ConflictFile{}; }

        std::expected<std::string, gf::git::ports::GitOpsError> getFileStageContent(
            const std::filesystem::path&, const std::string&, int) override { return ""; }

        std::expected<void, gf::git::ports::GitOpsError> applyConflictResolution(
            const std::filesystem::path&, const gf::git::bo::ApplyConflictResolutionOptions&) override { return {}; }
    };

    struct CloneTestSetup
    {
        gf::infra::persistence::sqlite::SqliteConnection conn;
        gf::infra::persistence::repositories::SqliteRepositoryStore repoStore;
        gf::repositories::bl::RepositoryService repoService;
        MockCloneGitOps mockGitOps;
        gf::git::bl::GitCloneService cloneService;
        std::filesystem::path tempDir;

        CloneTestSetup()
            : conn([]()
              {
                  auto c = gf::infra::persistence::sqlite::SqliteConnection::openInMemory();
                  auto migRes = gf::infra::persistence::sqlite::SchemaMigrator::migrateToLatest(*c);
                  (void)migRes;
                  return std::move(*c);
              }()),
              repoStore(conn),
              repoService(repoStore),
              mockGitOps(),
              cloneService(mockGitOps, repoService),
              tempDir(std::filesystem::temp_directory_path() / ("gf_clone_unit_" + std::to_string(
                  std::chrono::steady_clock::now().time_since_epoch().count())))
        {
            std::error_code ec;
            std::filesystem::create_directories(tempDir / ".git", ec);
        }

        ~CloneTestSetup()
        {
            std::error_code ec;
            std::filesystem::remove_all(tempDir, ec);
        }
    };
} // namespace

TEST_CASE (
"GitCloneService::extractRepositoryNameFromUrl parses various remote URLs"
,
"[git][clone]"
)
 {
    using gf::git::bl::GitCloneService;

    CHECK(GitCloneService::extractRepositoryNameFromUrl("https://github.com/torvalds/linux.git") == "linux");
    CHECK(GitCloneService::extractRepositoryNameFromUrl("https://github.com/torvalds/linux/") == "linux");
    CHECK(GitCloneService::extractRepositoryNameFromUrl("https://github.com/torvalds/linux") == "linux");
    CHECK(GitCloneService::extractRepositoryNameFromUrl("git@github.com:torvalds/linux.git") == "linux");
    CHECK(GitCloneService::extractRepositoryNameFromUrl("ssh://git@host:port/path/repo.git/") == "repo");
    CHECK(GitCloneService::extractRepositoryNameFromUrl("https://gitlab.com/group/subgroup/my-cool-project") == "my-cool-project");
    CHECK(GitCloneService::extractRepositoryNameFromUrl("/home/user/workspace/repo.git") == "repo");
    CHECK(GitCloneService::extractRepositoryNameFromUrl("") == "repository");
    CHECK(GitCloneService::extractRepositoryNameFromUrl("   ") == "repository");
}

TEST_CASE (
"GitCloneService::cloneAndRegister delegates to GitOps and registers in repository catalog"
,
"[git][clone]"
)
 {
    CloneTestSetup setup;

    SECTION("Successful clone registers repository into RepositoryService") {
        setup.mockGitOps.resultToReturn = gf::git::bo::CloneRepositoryResult{
            .repositoryPath = setup.tempDir,
            .displayName = "linux",
            .initialBranch = "master"
        };

        gf::git::bo::CloneRepositoryOptions opts{
            .remoteUrl = "https://github.com/torvalds/linux.git",
            .destinationPath = setup.tempDir
        };

        auto res = setup.cloneService.cloneAndRegister(opts);
        REQUIRE(res.has_value());
        CHECK(setup.mockGitOps.cloneCalled);
        CHECK(res->displayName == "linux");
        CHECK(res->localPath == setup.tempDir);

        // Verify repository is present in database
        auto allRepos = setup.repoService.getAllRepositories();
        REQUIRE(allRepos.has_value());
        REQUIRE(allRepos->size() == 1);
        CHECK((*allRepos)[0].displayName == "linux");
    }

    SECTION("Error during clone returns GitOpsError without registering repository") {
        setup.mockGitOps.resultToReturn = std::unexpected(gf::git::ports::GitOpsError{
            .kind = gf::git::ports::GitOpsError::Kind::DestinationAlreadyExists,
            .exitCode = -1,
            .message = "Destination already exists",
            .command = "git clone"
        });

        gf::git::bo::CloneRepositoryOptions opts{
            .remoteUrl = "https://github.com/torvalds/linux.git",
            .destinationPath = setup.tempDir
        };

        auto res = setup.cloneService.cloneAndRegister(opts);
        REQUIRE_FALSE(res.has_value());
        CHECK(res.error().kind == gf::git::ports::GitOpsError::Kind::DestinationAlreadyExists);

        auto allRepos = setup.repoService.getAllRepositories();
        REQUIRE(allRepos.has_value());
        CHECK(allRepos->empty());
    }
}
