#include "core/modules/git/bl/git_history_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

struct HistoryTestSandbox {
    std::filesystem::path root;

    HistoryTestSandbox() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() / ("gf_history_integ_" + std::to_string(nonce));
        std::filesystem::create_directories(root);
    }

    ~HistoryTestSandbox() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void writeFile(const std::string& relativePath, const std::string& content) {
        auto filePath = root / relativePath;
        std::filesystem::create_directories(filePath.parent_path());
        std::ofstream ofs(filePath, std::ios::trunc);
        ofs << content;
    }
};

} // namespace

TEST_CASE("NativeRepositoryGitOps and GitHistoryService real Git integration", "[git][history][integration]") {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitHistoryService historyService(gitOps);

    HistoryTestSandbox sandbox;

    // Initialize Git repository with branch 'main'
    gf::git::bo::GitCommand initCmd{
        .args = {"init", "-b", "main"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    REQUIRE(executor.execute(initCmd)->exitCode == 0);

    // Configure user identity
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.name", "Tester Bot"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.email", "tester@gitflux.local"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    // Commit 1: file1.txt with subject and body
    sandbox.writeFile("file1.txt", "Contenuto iniziale 1\n");
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"add", "file1.txt"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"commit", "-m", "feat: first commit\n\nThis is a multiline commit body for testing."},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    // Commit 2: file2.txt
    sandbox.writeFile("file2.txt", "Contenuto 2\n");
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"add", "file2.txt"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"commit", "-m", "feat: second commit"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    // Create feature branch and Commit 3
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"checkout", "-b", "feature/branch-a"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    sandbox.writeFile("file3.txt", "Contenuto da feature branch\n");
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"add", "file3.txt"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"commit", "-m", "feat: branch commit"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    // Switch to main and merge --no-ff to create merge commit with 2 parents
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"checkout", "main"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"merge", "--no-ff", "feature/branch-a", "-m", "merge: integrate branch-a"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    SECTION("1. Lettura cronologia completa e commit di merge") {
        auto logRes = historyService.getCommitHistory(sandbox.root);
        REQUIRE(logRes.has_value());
        REQUIRE(logRes->size() == 4);

        // The first returned commit (HEAD) is the merge commit
        const auto& mergeCommit = (*logRes)[0];
        CHECK(mergeCommit.subject == "merge: integrate branch-a");
        REQUIRE(mergeCommit.parents.size() == 2);
        CHECK(mergeCommit.authorName == "Tester Bot");
        CHECK(mergeCommit.authorEmail == "tester@gitflux.local");
        CHECK_FALSE(mergeCommit.authorDateIso.empty());

        // The last commit is the initial commit
        const auto& initialCommit = (*logRes)[3];
        CHECK(initialCommit.subject == "feat: first commit");
        CHECK(initialCommit.body == "This is a multiline commit body for testing.");
        CHECK(initialCommit.parents.empty());
    }

    SECTION("2. Paginazione (maxCount e skip)") {
        auto fullLog = historyService.getCommitHistory(sandbox.root);
        REQUIRE(fullLog.has_value());
        REQUIRE(fullLog->size() == 4);

        gf::git::bo::CommitLogOptions page1;
        page1.maxCount = 2;
        page1.skip = 0;

        auto p1Res = historyService.getCommitHistory(sandbox.root, page1);
        REQUIRE(p1Res.has_value());
        REQUIRE(p1Res->size() == 2);
        CHECK((*p1Res)[0].sha == (*fullLog)[0].sha);
        CHECK((*p1Res)[1].sha == (*fullLog)[1].sha);

        gf::git::bo::CommitLogOptions page2;
        page2.maxCount = 2;
        page2.skip = 2;

        auto p2Res = historyService.getCommitHistory(sandbox.root, page2);
        REQUIRE(p2Res.has_value());
        REQUIRE(p2Res->size() == 2);
        CHECK((*p2Res)[0].sha == (*fullLog)[2].sha);
        CHECK((*p2Res)[1].sha == (*fullLog)[3].sha);
    }

    SECTION("3. Filtro per percorso di file") {
        gf::git::bo::CommitLogOptions pathFilter;
        pathFilter.pathFilter = "file1.txt";

        auto filteredRes = historyService.getCommitHistory(sandbox.root, pathFilter);
        REQUIRE(filteredRes.has_value());
        REQUIRE(filteredRes->size() == 1);
        CHECK((*filteredRes)[0].subject == "feat: first commit");
    }

    SECTION("4. Dettaglio singolo commit tramite getCommitDetails") {
        auto logRes = historyService.getCommitHistory(sandbox.root);
        REQUIRE(logRes.has_value());
        REQUIRE(!logRes->empty());

        const auto targetSha = (*logRes)[1].sha;
        auto details = historyService.getCommitDetails(sandbox.root, targetSha);
        REQUIRE(details.has_value());
        REQUIRE(details->has_value());
        CHECK((*details)->sha == targetSha);
        CHECK((*details)->subject == (*logRes)[1].subject);
    }

    SECTION("5. Creazione, elenco ed eliminazione tag (Lightweight e Annotated)") {
        // Lightweight tag v0.1.0 on first commit
        auto commits = historyService.getCommitHistory(sandbox.root);
        REQUIRE(commits.has_value());
        const auto firstSha = (*commits)[3].sha;

        auto lwRes = historyService.createTag(sandbox.root, gf::git::bo::CreateTagOptions{
            .name = "v0.1.0",
            .targetCommit = firstSha
        });
        REQUIRE(lwRes.has_value());

        // Annotated tag v1.0.0 on HEAD
        auto annRes = historyService.createTag(sandbox.root, gf::git::bo::CreateTagOptions{
            .name = "v1.0.0",
            .message = "Release version 1.0.0"
        });
        REQUIRE(annRes.has_value());

        // Verify tag list
        auto tagsRes = historyService.listTags(sandbox.root);
        REQUIRE(tagsRes.has_value());
        REQUIRE(tagsRes->size() == 2);

        bool foundLw = false;
        bool foundAnn = false;
        for (const auto& tag : *tagsRes) {
            if (tag.name == "v0.1.0") {
                foundLw = true;
                CHECK(tag.type == gf::git::bo::TagType::Lightweight);
                CHECK(tag.targetSha == firstSha);
            }
            if (tag.name == "v1.0.0") {
                foundAnn = true;
                CHECK(tag.type == gf::git::bo::TagType::Annotated);
                CHECK(tag.message == "Release version 1.0.0");
                CHECK(tag.taggerName == "Tester Bot");
                CHECK(tag.taggerEmail == "tester@gitflux.local");
            }
        }
        CHECK(foundLw);
        CHECK(foundAnn);

        // Error on duplicate tag
        auto dupRes = historyService.createTag(sandbox.root, gf::git::bo::CreateTagOptions{
            .name = "v1.0.0"
        });
        REQUIRE_FALSE(dupRes.has_value());
        CHECK(dupRes.error().kind == gf::git::ports::GitOpsError::Kind::TagAlreadyExists);

        // Delete tag
        auto delRes = historyService.deleteTag(sandbox.root, "v0.1.0");
        REQUIRE(delRes.has_value());

        auto remainingTags = historyService.listTags(sandbox.root);
        REQUIRE(remainingTags.has_value());
        REQUIRE(remainingTags->size() == 1);
        CHECK((*remainingTags)[0].name == "v1.0.0");

        // Delete non-existent tag
        auto delBad = historyService.deleteTag(sandbox.root, "nonexistent-tag");
        REQUIRE_FALSE(delBad.has_value());
        CHECK(delBad.error().kind == gf::git::ports::GitOpsError::Kind::TagNotFound);
    }
}
