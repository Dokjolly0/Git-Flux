#include "core/modules/git/bl/git_diff_service.hpp"
#include "core/modules/git/bl/git_history_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
    struct HistoryTestSandbox
    {
        std::filesystem::path root;

        HistoryTestSandbox()
        {
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() / ("gf_history_integ_" + std::to_string(nonce));
            std::filesystem::create_directories(root);
        }

        ~HistoryTestSandbox()
        {
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }

        void writeFile(const std::string& relativePath, const std::string& content) const
        {
            const auto filePath = root / relativePath;
            std::filesystem::create_directories(filePath.parent_path());
            std::ofstream ofs(filePath, std::ios::trunc);
            ofs << content;
        }
    };
} // namespace

TEST_CASE (
"NativeRepositoryGitOps and GitHistoryService real Git integration"
,
"[git][history][integration]"
)
 {
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

TEST_CASE("Multi-commit changes summary and range diff across real Git repository", "[integration][history][multiselect]")
{
    auto gitPath = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitPath.has_value());

    HistoryTestSandbox sandbox;
    gf::infra::git::NativeGitCommandExecutor executor(gitPath->discoveredInPath);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitHistoryService historyService(gitOps);
    gf::git::bl::GitDiffService diffService(gitOps);

    auto runGit = [&](std::vector<std::string> args)
    {
        auto res = executor.execute(gf::git::bo::GitCommand{
            .args = std::move(args),
            .workingDirectory = sandbox.root,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        });
        REQUIRE(res.has_value());
        REQUIRE(res->exitCode == 0);
        return res->stdoutBytes;
    };

    auto headSha = [&]()
    {
        std::string s = runGit({"rev-parse", "HEAD"});
        while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
        return s;
    };

    runGit({"init", "-b", "main"});
    runGit({"config", "user.name", "Tester"});
    runGit({"config", "user.email", "tester@gitflux.local"});

    // C1 (root commit): adds shared.txt + base.txt
    sandbox.writeFile("shared.txt", "line1\n");
    sandbox.writeFile("base.txt", "base\n");
    runGit({"add", "."});
    runGit({"commit", "-m", "C1: initial"});
    const std::string c1 = headSha();

    // C2: modifies shared.txt (adds line2) + adds temp.txt
    sandbox.writeFile("shared.txt", "line1\nline2\n");
    sandbox.writeFile("temp.txt", "temporary\n");
    runGit({"add", "."});
    runGit({"commit", "-m", "C2: add line2 and temp"});
    const std::string c2 = headSha();

    // Parallel branch feat: adds feat.txt
    runGit({"checkout", "-b", "feat"});
    sandbox.writeFile("feat.txt", "feature line\n");
    runGit({"add", "."});
    runGit({"commit", "-m", "F1: feature work"});
    const std::string f1 = headSha();

    // Back to main: C3 modifies shared.txt (adds line3) and removes temp.txt (net cancel of temp.txt across C2..C3!)
    runGit({"checkout", "main"});
    sandbox.writeFile("shared.txt", "line1\nline2\nline3\n");
    std::filesystem::remove(sandbox.root / "temp.txt");
    runGit({"add", "-A"});
    runGit({"commit", "-m", "C3: add line3 and drop temp"});
    const std::string c3 = headSha();

    SECTION("Contiguous range C2..C3 computes net diff cancelling temp.txt and preserves per-commit attribution")
    {
        auto res = historyService.getMultiCommitChanges(sandbox.root, {c3, c2}, true, c1, c3);
        REQUIRE(res.has_value());
        CHECK(res->isContiguousRange);
        CHECK(res->rangeBaseRevision == c1);
        CHECK(res->rangeHeadSha == c3);

        // Net diff across C1..C3 has ONLY shared.txt (+2 lines), because temp.txt was added in C2 and deleted in C3!
        REQUIRE(res->aggregatedFiles.size() == 1);
        CHECK(res->aggregatedFiles[0].path == "shared.txt");
        CHECK(res->aggregatedFiles[0].additions == 2);
        CHECK(res->aggregatedFiles[0].deletions == 0);
        // Attribution shows BOTH C3 and C2 touched shared.txt
        REQUIRE(res->aggregatedFiles[0].commits.size() == 2);
        CHECK(res->aggregatedFiles[0].commits[0].commitSha == c3);
        CHECK(res->aggregatedFiles[0].commits[1].commitSha == c2);

        // Per-commit groups preserve exact files of each commit (including temp.txt in both C3 and C2)
        REQUIRE(res->perCommitGroups.size() == 2);
        CHECK(res->perCommitGroups[0].commitSha == c3);
        CHECK(res->perCommitGroups[0].files.size() == 2);
        CHECK(res->perCommitGroups[1].commitSha == c2);
        CHECK(res->perCommitGroups[1].files.size() == 2);

        // Range diff on shared.txt shows both added lines (+line2, +line3)
        auto diffRes = diffService.getCommitRangeFileDiff(sandbox.root, c1, c3, "shared.txt", 3);
        REQUIRE(diffRes.has_value());
        CHECK(diffRes->additionsCount == 2);
        CHECK(diffRes->deletionsCount == 0);
    }

    SECTION("Contiguous range starting at root commit C1..C3 uses Git empty tree")
    {
        constexpr const char* kEmptyTree = "4b825dc642cb6eb9a060e54bf8d69288fbee4904";
        auto res = historyService.getMultiCommitChanges(sandbox.root, {c3, c2, c1}, true, kEmptyTree, c3);
        REQUIRE(res.has_value());
        REQUIRE(res->aggregatedFiles.size() == 2); // base.txt + shared.txt

        auto diffRes = diffService.getCommitRangeFileDiff(sandbox.root, kEmptyTree, c3, "shared.txt", 3);
        REQUIRE(diffRes.has_value());
        CHECK(diffRes->additionsCount == 3);
    }

    SECTION("Sparse cross-branch selection C3 + F1 aggregates via git show --no-walk")
    {
        auto res = historyService.getMultiCommitChanges(sandbox.root, {c3, f1}, false, {}, {});
        REQUIRE(res.has_value());
        CHECK_FALSE(res->isContiguousRange);
        REQUIRE(res->perCommitGroups.size() == 2);
        CHECK(res->perCommitGroups[0].commitSha == c3);
        CHECK(res->perCommitGroups[1].commitSha == f1);

        bool foundFeat = false;
        bool foundShared = false;
        for (const auto& f : res->aggregatedFiles)
        {
            if (f.path == "feat.txt")
            {
                foundFeat = true;
                REQUIRE(f.commits.size() == 1);
                CHECK(f.commits[0].commitSha == f1);
            }
            if (f.path == "shared.txt")
            {
                foundShared = true;
                REQUIRE(f.commits.size() == 1);
                CHECK(f.commits[0].commitSha == c3);
            }
        }
        CHECK(foundFeat);
        CHECK(foundShared);
    }
}
