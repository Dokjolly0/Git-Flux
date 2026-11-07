#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <filesystem>
#include <fstream>
#include <memory>

using namespace gf::git::bo;
using namespace gf::git::ports;
using namespace gf::git::bl;
using namespace gf::infra::git;

namespace {

void writeTestFile(const std::filesystem::path& path, std::string_view content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

struct GitTestRepoFixture {
    std::filesystem::path repoDir;
    std::filesystem::path gitExe;
    std::unique_ptr<NativeGitCommandExecutor> executor;
    std::unique_ptr<NativeRepositoryGitOps> gitOps;
    std::unique_ptr<GitWorkingTreeService> service;

    GitTestRepoFixture() {
        auto gitDiscovery = GitDiscovery::discover();
        REQUIRE(gitDiscovery.has_value());
        gitExe = gitDiscovery->directBinaryPath.empty() ? gitDiscovery->discoveredInPath : gitDiscovery->directBinaryPath;

        repoDir = std::filesystem::temp_directory_path() / "gf_working_tree_integ_repo";
        std::error_code ec;
        std::filesystem::remove_all(repoDir, ec);
        std::filesystem::create_directories(repoDir);

        executor = std::make_unique<NativeGitCommandExecutor>(gitExe);
        gitOps = std::make_unique<NativeRepositoryGitOps>(*executor);
        service = std::make_unique<GitWorkingTreeService>(*gitOps);
    }

    ~GitTestRepoFixture() {
        std::error_code ec;
        std::filesystem::remove_all(repoDir, ec);
    }

    void runGit(std::vector<std::string> args) {
        GitCommand cmd{
            .args = std::move(args),
            .workingDirectory = repoDir,
            .lockPolicy = GitLockPolicy::Standard,
            .timeout = std::chrono::milliseconds{10'000}
        };
        auto res = executor->execute(cmd);
        REQUIRE(res.has_value());
        REQUIRE(res->exitCode == 0);
    }
};

} // namespace

TEST_CASE("GitWorkingTreeService accurately reports working tree state on real git repos", "[git][integration]") {
    GitTestRepoFixture fix;

    SECTION("Newly initialized git repository has unborn HEAD and clean working tree") {
        fix.runGit({"init", "-b", "main"});

        auto statusRes = fix.service->getWorkingTreeStatus(fix.repoDir);
        REQUIRE(statusRes.has_value());

        const auto& status = *statusRes;
        CHECK(status.isClean());
        CHECK(status.branch.headName == "main");
        CHECK(status.branch.unborn == true);
    }

    SECTION("Commit workflow transitions working tree between staged, clean, modified and untracked") {
        fix.runGit({"init", "-b", "main"});
        fix.runGit({"config", "user.name", "GitFlux Tester"});
        fix.runGit({"config", "user.email", "tester@gitflux.local"});

        // 1. Create file1.txt and file2.txt
        writeTestFile(fix.repoDir / "file1.txt", "Hello World\n");
        writeTestFile(fix.repoDir / "file2.txt", "Initial content\n");

        auto status1 = fix.service->getWorkingTreeStatus(fix.repoDir);
        REQUIRE(status1.has_value());
        CHECK(status1->untrackedCount() == 2);
        CHECK(status1->stagedCount() == 0);
        CHECK(status1->unstagedCount() == 0);

        // 2. Stage both files and commit
        fix.runGit({"add", "."});
        auto status2 = fix.service->getWorkingTreeStatus(fix.repoDir);
        REQUIRE(status2.has_value());
        CHECK(status2->stagedCount() == 2);
        CHECK(status2->untrackedCount() == 0);

        fix.runGit({"commit", "-m", "Initial commit"});

        // 3. Clean working tree after commit
        auto statusClean = fix.service->getWorkingTreeStatus(fix.repoDir);
        REQUIRE(statusClean.has_value());
        CHECK(statusClean->isClean());
        CHECK(statusClean->branch.unborn == false);
        CHECK(statusClean->branch.oid.size() >= 40);

        // 4. Create mixed state:
        // - file1.txt modified and staged
        // - file2.txt modified and unstaged
        // - file3.txt created and untracked
        writeTestFile(fix.repoDir / "file1.txt", "Updated content staged\n");
        fix.runGit({"add", "file1.txt"});

        writeTestFile(fix.repoDir / "file2.txt", "Updated content unstaged\n");
        writeTestFile(fix.repoDir / "file3.txt", "New untracked file\n");

        auto statusMixed = fix.service->getWorkingTreeStatus(fix.repoDir);
        REQUIRE(statusMixed.has_value());
        CHECK_FALSE(statusMixed->isClean());
        CHECK(statusMixed->totalChangesCount() == 3);

        REQUIRE(statusMixed->stagedCount() == 1);
        CHECK(statusMixed->stagedFiles[0].path == "file1.txt");
        CHECK(statusMixed->stagedFiles[0].kind == FileChangeKind::Modified);

        REQUIRE(statusMixed->unstagedCount() == 1);
        CHECK(statusMixed->unstagedFiles[0].path == "file2.txt");
        CHECK(statusMixed->unstagedFiles[0].kind == FileChangeKind::Modified);

        REQUIRE(statusMixed->untrackedCount() == 1);
        CHECK(statusMixed->untrackedFiles[0].path == "file3.txt");
        CHECK(statusMixed->untrackedFiles[0].kind == FileChangeKind::Untracked);
    }
}
