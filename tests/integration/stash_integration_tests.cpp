#include "core/modules/git/bl/git_stash_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

struct StashTestSandbox {
    std::filesystem::path root;

    StashTestSandbox() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() / ("gf_stash_integ_" + std::to_string(nonce));
        std::filesystem::create_directories(root);
    }

    ~StashTestSandbox() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void writeFile(const std::string& relativePath, const std::string& content) {
        auto filePath = root / relativePath;
        std::filesystem::create_directories(filePath.parent_path());
        std::ofstream ofs(filePath, std::ios::trunc);
        ofs << content;
    }

    [[nodiscard]] std::string readFile(const std::string& relativePath) {
        auto filePath = root / relativePath;
        std::ifstream ifs(filePath);
        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    }

    [[nodiscard]] bool fileExists(const std::string& relativePath) {
        std::error_code ec;
        return std::filesystem::exists(root / relativePath, ec);
    }
};

} // namespace

TEST_CASE("NativeRepositoryGitOps and GitStashService real Git lifecycle", "[git][stash][integration]") {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitStashService stashService(gitOps);

    StashTestSandbox sandbox;

    // 1. Inizializza repository Git con branch 'main'
    gf::git::bo::GitCommand initCmd{
        .args = {"init", "-b", "main"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    REQUIRE(executor.execute(initCmd)->exitCode == 0);

    // Configura autore locale
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.name", "Tester Bot"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.email", "tester@gitflux.local"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    // Commit iniziale
    sandbox.writeFile("base.txt", "line 1\nline 2\n");
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"add", "base.txt"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"commit", "-m", "Initial commit"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    SECTION("Stash push quando il repository è pulito ritorna NothingToStash") {
        auto emptyList = stashService.listStashes(sandbox.root);
        REQUIRE(emptyList.has_value());
        CHECK(emptyList->empty());

        auto pushRes = stashService.stashPush(sandbox.root, {.message = "Should fail"});
        REQUIRE_FALSE(pushRes.has_value());
        CHECK(pushRes.error().kind == gf::git::ports::GitOpsError::Kind::NothingToStash);
    }

    SECTION("Ciclo completo di stash: push, list, apply, drop, pop e clear") {
        // Modifica file tracciato e aggiungi file non tracciato
        sandbox.writeFile("base.txt", "line 1\nmodified line 2\n");
        sandbox.writeFile("untracked.txt", "new untracked file\n");

        // 1. Stash push con includeUntracked e messaggio personalizzato
        gf::git::bo::StashPushOptions pushOpts1{
            .message = "WIP feature alpha",
            .includeUntracked = true
        };
        auto pushRes1 = stashService.stashPush(sandbox.root, pushOpts1);
        REQUIRE(pushRes1.has_value());

        // Verifica che l'albero di lavoro sia pulito (file ripristinato e untracked rimosso)
        CHECK(sandbox.readFile("base.txt") == "line 1\nline 2\n");
        CHECK_FALSE(sandbox.fileExists("untracked.txt"));

        // 2. List stashes verifica record parsing
        auto list1 = stashService.listStashes(sandbox.root);
        REQUIRE(list1.has_value());
        REQUIRE(list1->size() == 1);
        const auto& item1 = (*list1)[0];
        CHECK(item1.index == 0);
        CHECK(item1.selector == "stash@{0}");
        CHECK(item1.branchName == "main");
        CHECK(item1.message.find("WIP feature alpha") != std::string::npos);
        CHECK_FALSE(item1.commitSha.empty());
        CHECK_FALSE(item1.dateIso.empty());

        // 3. Secondo stash
        sandbox.writeFile("second.txt", "second file content\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "second.txt"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        gf::git::bo::StashPushOptions pushOpts2{
            .message = "Second quick stash",
            .includeUntracked = true
        };
        auto pushRes2 = stashService.stashPush(sandbox.root, pushOpts2);
        REQUIRE(pushRes2.has_value());
        CHECK_FALSE(sandbox.fileExists("second.txt"));

        // Ora ci sono 2 stash
        auto list2 = stashService.listStashes(sandbox.root);
        REQUIRE(list2.has_value());
        REQUIRE(list2->size() == 2);
        CHECK((*list2)[0].index == 0);
        CHECK((*list2)[0].message.find("Second quick stash") != std::string::npos);
        CHECK((*list2)[1].index == 1);
        CHECK((*list2)[1].message.find("WIP feature alpha") != std::string::npos);

        // 4. Stash apply su stash@{0} (Second quick stash) senza rimuoverlo
        auto applyRes = stashService.stashApply(sandbox.root, {.stashIndex = 0});
        REQUIRE(applyRes.has_value());
        CHECK(sandbox.fileExists("second.txt"));

        // La lista ha ancora 2 stash
        auto listAfterApply = stashService.listStashes(sandbox.root);
        REQUIRE(listAfterApply.has_value());
        CHECK(listAfterApply->size() == 2);

        // Scarta la modifica ripristinata per pulire
        std::error_code ec;
        std::filesystem::remove(sandbox.root / "second.txt", ec);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"reset", "--hard", "HEAD"},
            .workingDirectory = sandbox.root
        })->exitCode == 0);

        // 5. Stash drop di stash@{0}
        auto dropRes = stashService.stashDrop(sandbox.root, 0);
        REQUIRE(dropRes.has_value());

        // Ora rimane solo lo stash@{0} originario (WIP feature alpha)
        auto listAfterDrop = stashService.listStashes(sandbox.root);
        REQUIRE(listAfterDrop.has_value());
        REQUIRE(listAfterDrop->size() == 1);
        CHECK((*listAfterDrop)[0].message.find("WIP feature alpha") != std::string::npos);

        // 6. Stash pop su stash@{0}
        auto popRes = stashService.stashPop(sandbox.root, {.stashIndex = 0});
        REQUIRE(popRes.has_value());

        // Sia base.txt che untracked.txt sono tornati!
        CHECK(sandbox.readFile("base.txt") == "line 1\nmodified line 2\n");
        CHECK(sandbox.fileExists("untracked.txt"));
        CHECK(sandbox.readFile("untracked.txt") == "new untracked file\n");

        // Ora la lista è vuota
        auto listAfterPop = stashService.listStashes(sandbox.root);
        REQUIRE(listAfterPop.has_value());
        CHECK(listAfterPop->empty());

        // 7. Stash clear
        // Creiamo un nuovo stash per verificare clear
        REQUIRE(stashService.stashPush(sandbox.root, {.message = "Temp for clear"}).has_value());
        CHECK(stashService.listStashes(sandbox.root)->size() == 1);

        auto clearRes = stashService.stashClear(sandbox.root);
        REQUIRE(clearRes.has_value());
        CHECK(stashService.listStashes(sandbox.root)->empty());

        // 8. StashNotFound su pop/drop di indice inesistente
        auto invalidDrop = stashService.stashDrop(sandbox.root, 42);
        REQUIRE_FALSE(invalidDrop.has_value());
        CHECK(invalidDrop.error().kind == gf::git::ports::GitOpsError::Kind::StashNotFound);

        auto invalidPop = stashService.stashPop(sandbox.root, {.stashIndex = 42});
        REQUIRE_FALSE(invalidPop.has_value());
        CHECK(invalidPop.error().kind == gf::git::ports::GitOpsError::Kind::StashNotFound);
    }
}
