#include "core/modules/git/bl/conflict_marker_parser.hpp"
#include "core/modules/git/bl/git_conflict_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

struct ConflictTestSandbox {
    std::filesystem::path root;

    ConflictTestSandbox() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() / ("gf_conflict_integ_" + std::to_string(nonce));
        std::filesystem::create_directories(root);
    }

    ~ConflictTestSandbox() {
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
};

} // namespace

TEST_CASE("NativeRepositoryGitOps and GitConflictService 3-way conflict extraction and resolution", "[git][conflict][integration]") {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitConflictService conflictService(gitOps);

    ConflictTestSandbox sandbox;

    // 1. Inizializza repo Git
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"init", "-b", "main"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.name", "Conflict Tester"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.email", "conflict@gitflux.local"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    // Initial commit
    sandbox.writeFile("conflict.txt", "line header\ncommon base content\nline footer\n");
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"add", "conflict.txt"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"commit", "-m", "Initial commit"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    // 2. Branch 'feature_branch' con modifica divergente
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"checkout", "-b", "feature_branch"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    sandbox.writeFile("conflict.txt", "line header\ntheirs feature branch content\nline footer\n");
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"commit", "-am", "Feature branch commit"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    // 3. Torna su main e fai modifica confliggente
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"checkout", "main"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    sandbox.writeFile("conflict.txt", "line header\nours main branch content\nline footer\n");
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"commit", "-am", "Main branch commit"},
        .workingDirectory = sandbox.root
    })->exitCode == 0);

    // 4. Esegui il merge che genera conflitto
    auto mergeRes = gitOps.mergeBranch(sandbox.root, {.sourceBranchOrCommit = "feature_branch"});
    REQUIRE(mergeRes.has_value());
    CHECK(mergeRes->status == gf::git::bo::MergeStatus::Conflict);
    REQUIRE_FALSE(mergeRes->conflictedFiles.empty());
    CHECK(mergeRes->conflictedFiles[0] == "conflict.txt");

    // 5. Ispezione con GitConflictService
    auto cFileRes = conflictService.getConflictFile(sandbox.root, "conflict.txt");
    REQUIRE(cFileRes.has_value());
    const auto& cFile = *cFileRes;
    CHECK(cFile.filePath == "conflict.txt");
    CHECK_FALSE(cFile.isBinary);
    REQUIRE(cFile.totalHunks() == 1);
    CHECK(cFile.hunks[0].oursContent.find("ours main branch content") != std::string::npos);
    CHECK(cFile.hunks[0].theirsContent.find("theirs feature branch content") != std::string::npos);

    // 6. Recupero dei singoli stage (1=base, 2=ours, 3=theirs)
    auto stage1 = conflictService.getFileStageContent(sandbox.root, "conflict.txt", 1);
    REQUIRE(stage1.has_value());
    CHECK(stage1->find("common base content") != std::string::npos);

    auto stage2 = conflictService.getFileStageContent(sandbox.root, "conflict.txt", 2);
    REQUIRE(stage2.has_value());
    CHECK(stage2->find("ours main branch content") != std::string::npos);

    auto stage3 = conflictService.getFileStageContent(sandbox.root, "conflict.txt", 3);
    REQUIRE(stage3.has_value());
    CHECK(stage3->find("theirs feature branch content") != std::string::npos);

    // 7. Risoluzione guidata del conflitto
    auto hunks = cFile.hunks;
    hunks[0].isResolved = true;
    hunks[0].chosenSide = gf::git::bo::ConflictSide::Custom;
    hunks[0].customContent = "resolved combined content line\n";

    auto recRes = gf::git::bl::ConflictMarkerParser::reconstruct(cFile.fullContentWithMarkers, hunks);
    REQUIRE(recRes.has_value());
    std::string resolvedFileText = *recRes;
    CHECK(resolvedFileText == "line header\nresolved combined content line\nline footer\n");

    // 8. Applicazione della risoluzione con auto-stage
    auto applyRes = conflictService.applyConflictResolution(sandbox.root, {
        .filePath = "conflict.txt",
        .resolvedContent = resolvedFileText,
        .autoStage = true
    });
    REQUIRE(applyRes.has_value());

    // Verifica file sul disco
    CHECK(sandbox.readFile("conflict.txt") == resolvedFileText);

    // Verifica status del working tree: nessun conflitto residuo, file staged
    auto wtStatus = gitOps.getWorkingTreeStatus(sandbox.root);
    REQUIRE(wtStatus.has_value());
    CHECK(wtStatus->conflictedFiles.empty());
    CHECK(wtStatus->stagedFiles.size() == 1);
    CHECK(wtStatus->stagedFiles[0].path == "conflict.txt");

    // 9. Conclude il merge commit
    auto commitRes = gitOps.createCommit(sandbox.root, {.message = "Merge branch 'feature_branch' resolved"});
    REQUIRE(commitRes.has_value());
    CHECK_FALSE(commitRes->commitSha.empty());

    // Working tree completamente pulito
    auto cleanStatus = gitOps.getWorkingTreeStatus(sandbox.root);
    REQUIRE(cleanStatus.has_value());
    CHECK(cleanStatus->isClean());
}
