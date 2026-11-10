#include "core/modules/git/bl/git_rebase_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
    struct RebaseTestSandbox
    {
        std::filesystem::path root;

        RebaseTestSandbox()
        {
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() / ("gf_rebase_integ_" + std::to_string(nonce));
            std::filesystem::create_directories(root);
        }

        ~RebaseTestSandbox()
        {
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }

        void writeFile(const std::string& relativePath, const std::string& content)
        {
            auto filePath = root / relativePath;
            std::filesystem::create_directories(filePath.parent_path());
            std::ofstream ofs(filePath, std::ios::trunc);
            ofs << content;
        }

        [[nodiscard]] std::string readFile(const std::string& relativePath)
        {
            auto filePath = root / relativePath;
            std::ifstream ifs(filePath);
            return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        }

        [[nodiscard]] bool fileExists(const std::string& relativePath)
        {
            std::error_code ec;
            return std::filesystem::exists(root / relativePath, ec);
        }
    };
} // namespace

TEST_CASE (
"NativeRepositoryGitOps and GitRebaseService real Git rebase & cherry-pick"
,
"[git][rebase][integration]"
)
 {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitRebaseService rebaseService(gitOps);

    RebaseTestSandbox sandbox;

    // Initialize Git repository with branch 'main'
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"init", "-b", "main"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.name", "Tester Bot"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.email", "tester@gitflux.local"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    // Initial commit
    sandbox.writeFile("base.txt", "base line 1\nbase line 2\n");
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"add", "base.txt"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"commit", "-m", "Initial commit"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    SECTION("Rebase lineare senza conflitti tra branch divergenti") {
        // Create branch 'feature'
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "-b", "feature"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        sandbox.writeFile("feature.txt", "content from feature branch\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "feature.txt"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-m", "Feature commit"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        // Switch back to main and create independent commit
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "main"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        sandbox.writeFile("main_only.txt", "content from main branch\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "main_only.txt"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-m", "Main independent commit"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        // Switch back to feature and execute rebase onto main
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "feature"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        auto inProgBefore = rebaseService.isRebaseInProgress(sandbox.root);
        REQUIRE(inProgBefore.has_value());
        CHECK_FALSE(*inProgBefore);

        auto rebaseRes = rebaseService.startRebase(sandbox.root, {.upstream = "main"});
        REQUIRE(rebaseRes.has_value());
        CHECK((rebaseRes->status == gf::git::bo::RebaseStatus::Finished ||
               rebaseRes->status == gf::git::bo::RebaseStatus::Applied));

        auto inProgAfter = rebaseService.isRebaseInProgress(sandbox.root);
        REQUIRE(inProgAfter.has_value());
        CHECK_FALSE(*inProgAfter);

        // All 3 files are present on feature
        CHECK(sandbox.fileExists("base.txt"));
        CHECK(sandbox.fileExists("feature.txt"));
        CHECK(sandbox.fileExists("main_only.txt"));
    }

    SECTION("Rebase con conflitti, risoluzione manuale e continueRebase") {
        // Create branch 'conflict_branch'
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "-b", "conflict_branch"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        sandbox.writeFile("base.txt", "conflict line from feature branch\nbase line 2\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-am", "Conflict commit on branch"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        // Switch back to main and modify the same line
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "main"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        sandbox.writeFile("base.txt", "conflict line from main branch\nbase line 2\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-am", "Conflict commit on main"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        // Switch back to conflict_branch and start rebase
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "conflict_branch"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        auto conflictRebaseRes = rebaseService.startRebase(sandbox.root, {.upstream = "main"});
        REQUIRE(conflictRebaseRes.has_value());
        CHECK(conflictRebaseRes->status == gf::git::bo::RebaseStatus::Conflict);
        REQUIRE_FALSE(conflictRebaseRes->conflictedFiles.empty());
        CHECK(conflictRebaseRes->conflictedFiles[0] == "base.txt");

        // Verify rebase is in progress
        auto inProg = rebaseService.isRebaseInProgress(sandbox.root);
        REQUIRE(inProg.has_value());
        CHECK(*inProg == true);

        // Resolve conflict manually and stage
        sandbox.writeFile("base.txt", "resolved line manually merged\nbase line 2\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "base.txt"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        // Continue rebase
        auto contRes = rebaseService.continueRebase(sandbox.root);
        REQUIRE(contRes.has_value());
        CHECK((contRes->status == gf::git::bo::RebaseStatus::Finished ||
               contRes->status == gf::git::bo::RebaseStatus::Applied));

        auto inProgFinal = rebaseService.isRebaseInProgress(sandbox.root);
        REQUIRE(inProgFinal.has_value());
        CHECK_FALSE(*inProgFinal);
        CHECK(sandbox.readFile("base.txt") == "resolved line manually merged\nbase line 2\n");
    }

    SECTION("Rebase con conflitto e abortRebase") {
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "-b", "abort_branch"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        sandbox.writeFile("base.txt", "abort test line\nbase line 2\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-am", "Abort test commit on branch"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "main"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        sandbox.writeFile("base.txt", "main diverged line\nbase line 2\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-am", "Main diverged commit"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "abort_branch"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        auto startRes = rebaseService.startRebase(sandbox.root, {.upstream = "main"});
        REQUIRE(startRes.has_value());
        CHECK(startRes->status == gf::git::bo::RebaseStatus::Conflict);
        CHECK(*rebaseService.isRebaseInProgress(sandbox.root) == true);

        // Abort
        auto abortRes = rebaseService.abortRebase(sandbox.root);
        REQUIRE(abortRes.has_value());
        CHECK(*rebaseService.isRebaseInProgress(sandbox.root) == false);

        // Check that abort_branch restored the pre-rebase version
        CHECK(sandbox.readFile("base.txt") == "abort test line\nbase line 2\n");
    }

    SECTION("Cherry-pick di un commit da un altro ramo") {
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "-b", "donor_branch"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        sandbox.writeFile("donor.txt", "donor content\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "donor.txt"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-m", "Donor commit to pick"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        auto revCmd = executor.execute(gf::git::bo::GitCommand{
            .args = {"rev-parse", "HEAD"},
            .workingDirectory = sandbox.root
        });
        REQUIRE(revCmd);
        REQUIRE(revCmd->exitCode == 0);
        std::string donorSha = revCmd->stdoutBytes;
        while (!donorSha.empty() && (donorSha.back() == '\n' || donorSha.back() == '\r')) {
            donorSha.pop_back();
        }

        // Switch back to main
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "main"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);
        CHECK_FALSE(sandbox.fileExists("donor.txt"));

        // Cherry-pick commit
        auto cpRes = rebaseService.cherryPick(sandbox.root, {.commitSha = donorSha});
        REQUIRE(cpRes.has_value());
        CHECK(cpRes->success == true);
        CHECK_FALSE(cpRes->hasConflicts);
        CHECK_FALSE(cpRes->newCommitSha.empty());
        CHECK(sandbox.fileExists("donor.txt"));
        CHECK(sandbox.readFile("donor.txt") == "donor content\n");

        // Cherry-pick with non-existent commit returns CommitNotFound
        auto invalidCp = rebaseService.cherryPick(sandbox.root, {.commitSha = "0000000000000000000000000000000000000000"});
        REQUIRE_FALSE(invalidCp.has_value());
        CHECK(invalidCp.error().kind == gf::git::ports::GitOpsError::Kind::CommitNotFound);
    }
}
