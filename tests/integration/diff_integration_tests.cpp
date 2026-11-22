#include "core/modules/git/bl/git_diff_service.hpp"
#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
    struct DiffTestSandbox
    {
        std::filesystem::path root;

        DiffTestSandbox()
        {
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() / ("gf_diff_integ_" + std::to_string(nonce));
            std::filesystem::create_directories(root);
        }

        ~DiffTestSandbox()
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

        void removeFile(const std::string& relativePath)
        {
            std::filesystem::remove(root / relativePath);
        }
    };
} // namespace

TEST_CASE (
"NativeRepositoryGitOps and GitDiffService real Git diff extraction"
,
"[git][diff][integration]"
)
 {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitStagingService stagingService(gitOps);
    gf::git::bl::GitDiffService diffService(gitOps);

    DiffTestSandbox sandbox;

    // Initialize Git repository
    gf::git::bo::GitCommand initCmd{
        .args = {"init", "-b", "main"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    auto initRes = executor.execute(initCmd);
    REQUIRE(initRes.has_value());
    REQUIRE(initRes->exitCode == 0);

    // Configure local test author identity
    gf::git::bo::GitCommand cfgNameCmd{
        .args = {"config", "user.name", "Diff Bot"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    auto cfgNameRes = executor.execute(cfgNameCmd);
    REQUIRE(cfgNameRes.has_value());

    gf::git::bo::GitCommand cfgEmailCmd{
        .args = {"config", "user.email", "diffbot@gitflux.local"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    auto cfgEmailRes = executor.execute(cfgEmailCmd);
    REQUIRE(cfgEmailRes.has_value());

    SECTION("Staged diff su nuovo file in repository con unborn HEAD") {
        sandbox.writeFile("hello.txt", "Alpha\nBeta\nGamma\n");
        REQUIRE(stagingService.stageFiles(sandbox.root, {"hello.txt"}).has_value());

        auto diffRes = diffService.getStagedFileDiff(sandbox.root, "hello.txt");
        REQUIRE(diffRes.has_value());
        CHECK(diffRes->additionsCount == 3);
        CHECK(diffRes->deletionsCount == 0);
        REQUIRE_FALSE(diffRes->hunks.empty());
        CHECK(diffRes->hunks[0].lines.size() == 3);
        CHECK(diffRes->hunks[0].lines[0].content == "Alpha");
        CHECK(diffRes->hunks[0].lines[0].newLineNumber == 1);
    }

    SECTION("Unstaged diff su file tracciato modificato") {
        sandbox.writeFile("code.cpp", "int a = 1;\nint b = 2;\nint c = 3;\n");
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());
        REQUIRE(stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "c1"}).has_value());

        // Modify second line
        sandbox.writeFile("code.cpp", "int a = 1;\nint b = 200;\nint c = 3;\n");

        auto diffRes = diffService.getUnstagedFileDiff(sandbox.root, "code.cpp");
        REQUIRE(diffRes.has_value());
        CHECK(diffRes->additionsCount == 1);
        CHECK(diffRes->deletionsCount == 1);
        REQUIRE_FALSE(diffRes->hunks.empty());

        const auto& lines = diffRes->hunks[0].lines;
        bool foundDeletion = false;
        bool foundAddition = false;
        for (const auto& l : lines) {
            if (l.kind == gf::git::bo::DiffLineKind::Deletion && l.content == "int b = 2;") {
                foundDeletion = true;
                CHECK(l.oldLineNumber == 2);
            }
            if (l.kind == gf::git::bo::DiffLineKind::Addition && l.content == "int b = 200;") {
                foundAddition = true;
                CHECK(l.newLineNumber == 2);
            }
        }
        CHECK(foundDeletion);
        CHECK(foundAddition);
    }

    SECTION("Untracked file diff") {
        sandbox.writeFile("init.txt", "init");
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());
        REQUIRE(stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "init"}).has_value());

        sandbox.writeFile("notes.md", "# Titolo\nTesto della nota\n");

        auto diffRes = diffService.getUntrackedFileDiff(sandbox.root, "notes.md");
        REQUIRE(diffRes.has_value());
        CHECK(diffRes->additionsCount == 2);
        CHECK(diffRes->deletionsCount == 0);
        REQUIRE_FALSE(diffRes->hunks.empty());
        CHECK(diffRes->hunks[0].lines[0].content == "# Titolo");
    }

    SECTION("Unstaged diff su file cancellato") {
        sandbox.writeFile("temp.txt", "Eliminami\n");
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());
        REQUIRE(stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "c1"}).has_value());

        sandbox.removeFile("temp.txt");

        auto diffRes = diffService.getUnstagedFileDiff(sandbox.root, "temp.txt");
        REQUIRE(diffRes.has_value());
        CHECK(diffRes->deletionsCount == 1);
        CHECK(diffRes->additionsCount == 0);
    }

    SECTION("getAllDiffs estrae tutti i diff presenti") {
        sandbox.writeFile("f1.txt", "v1\n");
        sandbox.writeFile("f2.txt", "v1\n");
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());
        REQUIRE(stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "c1"}).has_value());

        sandbox.writeFile("f1.txt", "v1 modified\n");
        sandbox.writeFile("f2.txt", "v1 modified\n");

        auto allDiffs = diffService.getAllDiffs(sandbox.root, gf::git::bo::DiffScope::Unstaged);
        REQUIRE(allDiffs.has_value());
        CHECK(allDiffs->size() == 2);
    }
}


TEST_CASE (
"NativeRepositoryGitOps context lines (-U<n>, full file) and getFileContent"
,
"[git][diff][integration][context]"
)
 {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitStagingService stagingService(gitOps);
    gf::git::bl::GitDiffService diffService(gitOps);

    DiffTestSandbox sandbox;

    REQUIRE(executor.execute({.args = {"init", "-b", "main"}, .workingDirectory = sandbox.root}).has_value());
    REQUIRE(executor.execute({.args = {"config", "user.name", "Diff Bot"}, .workingDirectory = sandbox.root}).has_value());
    REQUIRE(executor.execute({.args = {"config", "user.email", "diffbot@gitflux.local"}, .workingDirectory = sandbox.root}).has_value());

    std::string initial;
    for (int i = 1; i <= 30; ++i) {
        initial += "Line " + std::to_string(i) + "\n";
    }
    sandbox.writeFile("docs/manual.md", initial);
    REQUIRE(stagingService.stageAll(sandbox.root).has_value());
    auto c1Res = stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "initial manual"});
    REQUIRE(c1Res.has_value());

    // Modify only line 15 in working tree
    std::string updated;
    for (int i = 1; i <= 30; ++i) {
        if (i == 15) {
            updated += "Line 15 UPDATED ├── tree\n";
        } else {
            updated += "Line " + std::to_string(i) + "\n";
        }
    }
    {
        auto filePath = sandbox.root / "docs/manual.md";
        std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc);
        ofs.write(updated.data(), static_cast<std::streamsize>(updated.size()));
    }

    SECTION("Context lines configurabili: -U3 vs -U10 vs file completo (-1)") {
        auto diffU3 = diffService.getFileDiff(sandbox.root, "docs/manual.md", gf::git::bo::DiffScope::Unstaged, 3);
        REQUIRE(diffU3.has_value());
        REQUIRE(diffU3->hunks.size() == 1);
        // 3 before + 1 del + 1 add + 3 after = 8 lines
        CHECK(diffU3->hunks[0].lines.size() == 8);

        auto diffU10 = diffService.getFileDiff(sandbox.root, "docs/manual.md", gf::git::bo::DiffScope::Unstaged, 10);
        REQUIRE(diffU10.has_value());
        REQUIRE(diffU10->hunks.size() == 1);
        // 10 before + 1 del + 1 add + 10 after = 22 lines
        CHECK(diffU10->hunks[0].lines.size() == 22);

        auto diffFull = diffService.getFileDiff(sandbox.root, "docs/manual.md", gf::git::bo::DiffScope::Unstaged, -1);
        REQUIRE(diffFull.has_value());
        REQUIRE(diffFull->hunks.size() == 1);
        // All 29 unchanged context lines + 1 deletion + 1 addition = 31 lines
        CHECK(diffFull->hunks[0].lines.size() == 31);
    }

    SECTION("getFileContent legge UTF-8 box-drawing da worktree, indice (:0) e commit HEAD") {
        auto wtContent = diffService.getFileContent(sandbox.root, "", "docs/manual.md");
        REQUIRE(wtContent.has_value());
        CHECK(wtContent->find("Line 15 UPDATED ├── tree") != std::string::npos);

        auto indexContent = diffService.getFileContent(sandbox.root, ":0", "docs/manual.md");
        REQUIRE(indexContent.has_value());
        CHECK(indexContent->find("Line 15\n") != std::string::npos);
        CHECK(indexContent->find("UPDATED") == std::string::npos);

        auto commitContent = diffService.getFileContent(sandbox.root, c1Res->commitSha, "docs/manual.md");
        REQUIRE(commitContent.has_value());
        CHECK(commitContent->find("Line 30") != std::string::npos);
    }

    SECTION("getCommitFileDiff con contextLines su secondo commit") {
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());
        auto c2Res = stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "update line 15"});
        REQUIRE(c2Res.has_value());

        auto commitDiffU10 = diffService.getCommitFileDiff(sandbox.root, c2Res->commitSha, "docs/manual.md", 10);
        REQUIRE(commitDiffU10.has_value());
        REQUIRE(commitDiffU10->hunks.size() == 1);
        CHECK(commitDiffU10->hunks[0].lines.size() == 22);

        auto commitDiffFull = diffService.getCommitFileDiff(sandbox.root, c2Res->commitSha, "docs/manual.md", -1);
        REQUIRE(commitDiffFull.has_value());
        REQUIRE(commitDiffFull->hunks.size() == 1);
        CHECK(commitDiffFull->hunks[0].lines.size() == 31);
    }

    SECTION("getFileBlob legge byte binari (con NUL) da HEAD, indice (:0), worktree e rispetta il limite maxBytes") {
        const std::string v1("\x89PNG\r\n\x1a\n\x00\x00\x00\x0D" "v1-payload", 22);
        const std::string v2("\x89PNG\r\n\x1a\n\x00\x00\x00\x0D" "v2-payload-longer", 29);

        auto binPath = sandbox.root / "assets" / "pic.png";
        std::filesystem::create_directories(binPath.parent_path());
        {
            std::ofstream ofs(binPath, std::ios::binary | std::ios::trunc);
            ofs.write(v1.data(), static_cast<std::streamsize>(v1.size()));
        }
        REQUIRE(stagingService.stageAll(sandbox.root).has_value());
        auto cBin = stagingService.createCommit(sandbox.root, gf::git::bo::CommitOptions{.message = "add binary"});
        REQUIRE(cBin.has_value());

        // Overwrite in working tree
        {
            std::ofstream ofs(binPath, std::ios::binary | std::ios::trunc);
            ofs.write(v2.data(), static_cast<std::streamsize>(v2.size()));
        }

        auto diffRes = diffService.getUnstagedFileDiff(sandbox.root, "assets/pic.png");
        REQUIRE(diffRes.has_value());
        CHECK(diffRes->isBinary);

        auto headBlob = diffService.getFileBlob(sandbox.root, "HEAD", "assets/pic.png");
        REQUIRE(headBlob.has_value());
        CHECK_FALSE(headBlob->truncated);
        CHECK(headBlob->totalSize == v1.size());
        CHECK(headBlob->bytes == v1);

        auto indexBlob = diffService.getFileBlob(sandbox.root, ":0", "assets/pic.png");
        REQUIRE(indexBlob.has_value());
        CHECK(indexBlob->bytes == v1);

        auto wtBlob = diffService.getFileBlob(sandbox.root, "WORKTREE", "assets/pic.png");
        REQUIRE(wtBlob.has_value());
        CHECK_FALSE(wtBlob->truncated);
        CHECK(wtBlob->totalSize == v2.size());
        CHECK(wtBlob->bytes == v2);

        // Truncation check when maxBytes is smaller than totalSize
        auto cappedWt = diffService.getFileBlob(sandbox.root, "WORKTREE", "assets/pic.png", 8);
        REQUIRE(cappedWt.has_value());
        CHECK(cappedWt->truncated);
        CHECK(cappedWt->totalSize == v2.size());
        CHECK(cappedWt->bytes.size() == 8);

        auto cappedHead = diffService.getFileBlob(sandbox.root, "HEAD", "assets/pic.png", 8);
        REQUIRE(cappedHead.has_value());
        CHECK(cappedHead->truncated);
        CHECK(cappedHead->totalSize == v1.size());
    }
}
