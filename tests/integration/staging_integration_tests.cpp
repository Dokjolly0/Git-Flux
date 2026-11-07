#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

struct StagingTestSandbox {
    std::filesystem::path root;

    StagingTestSandbox() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() / ("gf_staging_integ_" + std::to_string(nonce));
        std::filesystem::create_directories(root);
    }

    ~StagingTestSandbox() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void writeFile(const std::string& relativePath, const std::string& content) {
        auto filePath = root / relativePath;
        std::filesystem::create_directories(filePath.parent_path());
        std::ofstream ofs(filePath, std::ios::trunc);
        ofs << content;
    }

    std::string readFile(const std::string& relativePath) {
        auto filePath = root / relativePath;
        std::ifstream ifs(filePath);
        return std::string((std::istreambuf_iterator<char>(ifs)),
                            std::istreambuf_iterator<char>());
    }

    void removeFile(const std::string& relativePath) {
        std::filesystem::remove(root / relativePath);
    }
};

} // namespace

TEST_CASE("NativeRepositoryGitOps and GitStagingService real Git integration", "[git][staging][integration]") {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitStagingService stagingService(gitOps);
    gf::git::bl::GitWorkingTreeService statusService(gitOps);

    StagingTestSandbox sandbox;

    // Inizializza repository Git
    gf::git::bo::GitCommand initCmd{
        .args = {"init", "-b", "main"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    auto initRes = executor.execute(initCmd);
    REQUIRE(initRes.has_value());
    REQUIRE(initRes->exitCode == 0);

    // Configura autore locale di test
    gf::git::bo::GitCommand cfgNameCmd{
        .args = {"config", "user.name", "GitFlux TestBot"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    auto cfgNameRes = executor.execute(cfgNameCmd);
    REQUIRE(cfgNameRes.has_value());

    gf::git::bo::GitCommand cfgEmailCmd{
        .args = {"config", "user.email", "testbot@gitflux.local"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    auto cfgEmailRes = executor.execute(cfgEmailCmd);
    REQUIRE(cfgEmailRes.has_value());

    SECTION("Ciclo completo: Stage, Unstage su unborn HEAD, Stage All, Unstage All") {
        sandbox.writeFile("file_a.txt", "Contenuto A");
        sandbox.writeFile("file_b.txt", "Contenuto B");

        // 1. Stage solo di file_a.txt
        auto stageRes1 = stagingService.stageFiles(sandbox.root, {"file_a.txt"});
        REQUIRE(stageRes1.has_value());

        auto status1 = statusService.getWorkingTreeStatus(sandbox.root);
        REQUIRE(status1.has_value());
        REQUIRE(status1->stagedFiles.size() == 1);
        CHECK(status1->stagedFiles[0].path == "file_a.txt");
        REQUIRE(status1->untrackedFiles.size() == 1);
        CHECK(status1->untrackedFiles[0].path == "file_b.txt");

        // 2. Unstage di file_a.txt su unborn HEAD (test fallback git rm --cached)
        auto unstageRes1 = stagingService.unstageFiles(sandbox.root, {"file_a.txt"});
        REQUIRE(unstageRes1.has_value());

        auto status2 = statusService.getWorkingTreeStatus(sandbox.root);
        REQUIRE(status2.has_value());
        CHECK(status2->stagedFiles.empty());
        CHECK(status2->untrackedFiles.size() == 2);

        // 3. Stage All
        auto stageAllRes = stagingService.stageAll(sandbox.root);
        REQUIRE(stageAllRes.has_value());

        auto status3 = statusService.getWorkingTreeStatus(sandbox.root);
        REQUIRE(status3.has_value());
        CHECK(status3->stagedFiles.size() == 2);
        CHECK(status3->untrackedFiles.empty());

        // 4. Unstage All su unborn HEAD
        auto unstageAllRes = stagingService.unstageAll(sandbox.root);
        REQUIRE(unstageAllRes.has_value());

        auto status4 = statusService.getWorkingTreeStatus(sandbox.root);
        REQUIRE(status4.has_value());
        CHECK(status4->stagedFiles.empty());
        CHECK(status4->untrackedFiles.size() == 2);
    }

    SECTION("Commit e NothingToCommit") {
        sandbox.writeFile("first.txt", "Initial data");

        // Tentativo di commit con staging vuoto
        auto emptyCommit = stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{
            .message = "Tentativo a vuoto"
        });
        REQUIRE_FALSE(emptyCommit.has_value());
        CHECK(emptyCommit.error().kind == gf::git::ports::GitOpsError::Kind::NothingToCommit);

        // Mette in staging e committa
        auto stageRes = stagingService.stageAll(sandbox.root);
        REQUIRE(stageRes.has_value());

        auto commitRes = stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{
            .message = "feat(test): initial commit\n\nCorpo dettagliato del commit",
            .author = gf::git::bo::CommitAuthor{
                .name = "Alice Developer",
                .email = "alice@example.org"
            }
        });
        REQUIRE(commitRes.has_value());
        CHECK(commitRes->commitSha.size() == 40);
        CHECK(commitRes->shortSha.size() >= 7);
        CHECK(commitRes->summary == "feat(test): initial commit");
        CHECK(commitRes->branch == "main");

        // Working tree deve ora essere pulito
        auto statusAfterCommit = statusService.getWorkingTreeStatus(sandbox.root);
        REQUIRE(statusAfterCommit.has_value());
        CHECK(statusAfterCommit->isClean());
    }

    SECTION("Discard file tracciato modificato o eliminato") {
        sandbox.writeFile("tracked.txt", "Versione 1.0");
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());
        REQUIRE(stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "c1"}).has_value());

        // Modifica file tracciato
        sandbox.writeFile("tracked.txt", "Modifica non salvata");
        auto statusModified = statusService.getWorkingTreeStatus(sandbox.root);
        REQUIRE(statusModified.has_value());
        REQUIRE(statusModified->unstagedFiles.size() == 1);

        // Discard
        auto discardRes = stagingService.discardFiles(sandbox.root, {"tracked.txt"});
        REQUIRE(discardRes.has_value());
        CHECK(sandbox.readFile("tracked.txt") == "Versione 1.0");

        auto statusRestored = statusService.getWorkingTreeStatus(sandbox.root);
        REQUIRE(statusRestored.has_value());
        CHECK(statusRestored->isClean());

        // Cancellazione file tracciato
        sandbox.removeFile("tracked.txt");
        CHECK_FALSE(std::filesystem::exists(sandbox.root / "tracked.txt"));

        // Discard del file cancellato -> deve ricomparire con il suo contenuto originale!
        auto discardDelRes = stagingService.discardFiles(sandbox.root, {"tracked.txt"});
        REQUIRE(discardDelRes.has_value());
        CHECK(std::filesystem::exists(sandbox.root / "tracked.txt"));
        CHECK(sandbox.readFile("tracked.txt") == "Versione 1.0");
    }

    SECTION("Discard file untracked") {
        sandbox.writeFile("dummy.txt", "dummy");
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());
        REQUIRE(stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "init"}).has_value());

        // Crea file non tracciato
        sandbox.writeFile("untracked_temp.txt", "File da cestinare");
        CHECK(std::filesystem::exists(sandbox.root / "untracked_temp.txt"));

        auto discardUntracked = stagingService.discardFiles(sandbox.root, {"untracked_temp.txt"});
        REQUIRE(discardUntracked.has_value());
        CHECK_FALSE(std::filesystem::exists(sandbox.root / "untracked_temp.txt"));

        auto cleanStatus = statusService.getWorkingTreeStatus(sandbox.root);
        REQUIRE(cleanStatus.has_value());
        CHECK(cleanStatus->isClean());
    }

    SECTION("Amend commit") {
        sandbox.writeFile("file.txt", "v1");
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());
        auto c1 = stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "Commit iniziale errato"});
        REQUIRE(c1.has_value());

        // Modifica e amend
        sandbox.writeFile("file.txt", "v1 revisionata");
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());

        auto c2 = stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{
            .message = "Commit iniziale corretto",
            .amend = true
        });
        REQUIRE(c2.has_value());
        CHECK(c2->summary == "Commit iniziale corretto");

        // Verifica che ci sia solo 1 commit nella storia del branch
        gf::git::bo::GitCommand logCmd{
            .args = {"rev-list", "--count", "HEAD"},
            .workingDirectory = sandbox.root,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
        };
        auto logRes = executor.execute(logCmd);
        REQUIRE(logRes.has_value());
        REQUIRE(logRes->exitCode == 0);
        CHECK(logRes->stdoutBytes.starts_with("1"));
    }
}
