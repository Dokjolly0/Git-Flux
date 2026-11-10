#include <catch2/catch_test_macros.hpp>

#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"
#include "infrastructure/persistence/groups/sqlite_group_store.hpp"
#include "infrastructure/persistence/repositories/sqlite_repository_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/persistence/workspaces/sqlite_workspace_store.hpp"

#include <filesystem>
#include <fstream>

using namespace gf::infra::persistence::sqlite;
using namespace gf::infra::persistence::repositories;
using namespace gf::infra::persistence::groups;
using namespace gf::infra::persistence::workspaces;

using gf::groups::bl::GroupService;
using gf::groups::bl::GroupServiceError;
using gf::groups::bo::GroupId;
using gf::repositories::bl::RepositoryService;
using gf::repositories::bl::RepositoryServiceError;
using gf::repositories::bo::RepositoryId;
using gf::repositories::bo::RepositoryPathStatus;
using gf::workspaces::bl::WorkspaceService;
using gf::workspaces::bl::WorkspaceServiceError;
using gf::workspaces::bo::WorkspaceId;

namespace
{
    struct ScopedSandbox
    {
        std::filesystem::path dir;

        ScopedSandbox(std::string_view name)
        {
            dir = std::filesystem::temp_directory_path() / name;
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
            std::filesystem::create_directories(dir, ec);
        }

        ~ScopedSandbox()
        {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }
    };

    SqliteConnection createMemoryDb()
    {
        auto connRes = SqliteConnection::openInMemory();
        REQUIRE(connRes.has_value());
        REQUIRE(SchemaMigrator::migrateToLatest(*connRes).has_value());
        return std::move(*connRes);
    }

    void makeFakeGitRepo(const std::filesystem::path& path)
    {
        std::error_code ec;
        std::filesystem::create_directories(path / ".git", ec);
        std::ofstream f(path / ".git/config");
        f << "[core]\n";
    }
} // namespace

TEST_CASE (
"RepositoryService gestisce validazione, registrazione e riconnessione path"
,
"[unit][services][repository]"
)
 {
    auto conn = createMemoryDb();
    SqliteRepositoryStore store(conn);
    RepositoryService service(store);

    ScopedSandbox sandbox("gf_test_repo_service");
    const auto validRepoPath = sandbox.dir / "my_project";
    makeFakeGitRepo(validRepoPath);

    const auto notGitPath = sandbox.dir / "regular_folder";
    std::filesystem::create_directories(notGitPath);

    // 1. Empty path -> InvalidPath
    auto emptyRes = service.registerRepository({.localPath = {}, .displayName = std::nullopt});
    REQUIRE_FALSE(emptyRes.has_value());
    REQUIRE(emptyRes.error().kind == RepositoryServiceError::Kind::InvalidPath);

    // 2. Non-existent path -> PathNotFound
    auto nonExistentRes = service.registerRepository({.localPath = sandbox.dir / "ghost_folder", .displayName = std::nullopt});
    REQUIRE_FALSE(nonExistentRes.has_value());
    REQUIRE(nonExistentRes.error().kind == RepositoryServiceError::Kind::PathNotFound);

    // 3. Folder without .git -> NotAGitRepository
    auto notGitRes = service.registerRepository({.localPath = notGitPath, .displayName = std::nullopt});
    REQUIRE_FALSE(notGitRes.has_value());
    REQUIRE(notGitRes.error().kind == RepositoryServiceError::Kind::NotAGitRepository);

    // 4. Register valid repository (without explicit display name: computed from folder name)
    auto regRes = service.registerRepository({.localPath = validRepoPath, .displayName = std::nullopt});
    REQUIRE(regRes.has_value());
    REQUIRE(regRes->displayName == "my_project");
    REQUIRE(regRes->pathStatus == RepositoryPathStatus::Valid);
    REQUIRE(regRes->registeredAtUnixEpochMs > 0);

    const auto repoId = regRes->id;

    // 5. Attempt to re-register the same path -> AlreadyRegistered
    auto dupRes = service.registerRepository({.localPath = validRepoPath, .displayName = std::nullopt});
    REQUIRE_FALSE(dupRes.has_value());
    REQUIRE(dupRes.error().kind == RepositoryServiceError::Kind::AlreadyRegistered);

    // 6. Verify status with existing path
    auto statusCheck = service.checkRepositoryStatus(repoId);
    REQUIRE(statusCheck.has_value());
    REQUIRE(statusCheck->pathStatus == RepositoryPathStatus::Valid);

    // 7. Simulate directory move (Missing Repo)
    const auto movedRepoPath = sandbox.dir / "project_renamed";
    std::filesystem::rename(validRepoPath, movedRepoPath);

    auto statusMissing = service.checkRepositoryStatus(repoId);
    REQUIRE(statusMissing.has_value());
    REQUIRE(statusMissing->pathStatus == RepositoryPathStatus::Missing);

    // 8. Reconnect path (retaining the SAME RepositoryId!)
    auto reconnectRes = service.reconnectRepositoryPath(repoId, movedRepoPath);
    REQUIRE(reconnectRes.has_value());
    REQUIRE(reconnectRes->id == repoId);
    REQUIRE(reconnectRes->pathStatus == RepositoryPathStatus::Valid);
    REQUIRE(reconnectRes->localPath == std::filesystem::weakly_canonical(movedRepoPath));

    // 9. Unregister repository
    REQUIRE(service.unregisterRepository(repoId).value_or(false));
    REQUIRE(service.getAllRepositories()->empty());
}

TEST_CASE (
"RepositoryService resolves .git suffix and handles folder or .git selection seamlessly"
,
"[unit][services][repository]"
)
 {
    auto conn = createMemoryDb();
    SqliteRepositoryStore store(conn);
    RepositoryService service(store);

    ScopedSandbox sandbox("gf_test_repo_service_dotgit");
    const auto projectPath = sandbox.dir / "AwesomeApp";
    makeFakeGitRepo(projectPath);

    // 1. isGitRepositoryDir works with project root, .git folder, and trailing slash
    CHECK(RepositoryService::isGitRepositoryDir(projectPath));
    CHECK(RepositoryService::isGitRepositoryDir(projectPath / ".git"));
    CHECK(RepositoryService::isGitRepositoryDir(projectPath / ".git/"));

    // 2. Register repository pointing directly to .git
    auto dotGitRes = service.registerRepository({
        .localPath = projectPath / ".git",
        .displayName = std::nullopt
    });
    REQUIRE(dotGitRes.has_value());
    // localPath must be stripped of .git!
    CHECK(dotGitRes->localPath == std::filesystem::weakly_canonical(projectPath));
    // displayName must be derived from parent folder ("AwesomeApp"), NOT ".git"!
    CHECK(dotGitRes->displayName == "AwesomeApp");

    // 3. Attempting to register via normal path is recognized as duplicate
    auto dupNormalRes = service.registerRepository({
        .localPath = projectPath,
        .displayName = std::nullopt
    });
    REQUIRE_FALSE(dupNormalRes.has_value());
    CHECK(dupNormalRes.error().kind == RepositoryServiceError::Kind::AlreadyRegistered);

    // 4. Attempting to register again via .git is also recognized as duplicate
    auto dupDotGitRes = service.registerRepository({
        .localPath = projectPath / ".git",
        .displayName = std::nullopt
    });
    REQUIRE_FALSE(dupDotGitRes.has_value());
    CHECK(dupDotGitRes.error().kind == RepositoryServiceError::Kind::AlreadyRegistered);

    // 5. Reconnection pointing to .git also works and normalizes path
    const auto movedPath = sandbox.dir / "AwesomeAppRelocated";
    std::filesystem::rename(projectPath, movedPath);

    auto reconnectRes = service.reconnectRepositoryPath(dotGitRes->id, movedPath / ".git");
    REQUIRE(reconnectRes.has_value());
    CHECK(reconnectRes->localPath == std::filesystem::weakly_canonical(movedPath));
    CHECK(reconnectRes->pathStatus == RepositoryPathStatus::Valid);
}

TEST_CASE (
"GroupService gestisce membership e deduplicazione deterministica per Quick Open"
,
"[unit][services][group]"
)
 {
    auto conn = createMemoryDb();
    SqliteRepositoryStore repoStore(conn);
    SqliteGroupStore groupStore(conn);
    RepositoryService repoService(repoStore);
    GroupService groupService(groupStore, repoStore);

    ScopedSandbox sandbox("gf_test_group_service");
    auto p1 = sandbox.dir / "repo_a";
    auto p2 = sandbox.dir / "repo_b";
    auto p3 = sandbox.dir / "repo_c";
    makeFakeGitRepo(p1);
    makeFakeGitRepo(p2);
    makeFakeGitRepo(p3);

    auto r1 = repoService.registerRepository({.localPath = p1, .displayName = "Repo A"}).value();
    auto r2 = repoService.registerRepository({.localPath = p2, .displayName = "Repo B"}).value();
    auto r3 = repoService.registerRepository({.localPath = p3, .displayName = "Repo C"}).value();

    // 1. Empty name validation
    auto emptyRes = groupService.createGroup("");
    REQUIRE_FALSE(emptyRes.has_value());
    REQUIRE(emptyRes.error().kind == GroupServiceError::Kind::InvalidName);

    // 2. Create groups with shared repository (r2 is in both g1 and g2)
    auto g1 = groupService.createGroup("Cliente Alpha", {r1.id, r2.id});
    REQUIRE(g1.has_value());
    REQUIRE(g1->repositoryIds.size() == 2);

    auto g2 = groupService.createGroup("Frontend Guild", {r2.id, r3.id});
    REQUIRE(g2.has_value());
    REQUIRE(g2->repositoryIds.size() == 2);

    // 3. Duplicate name rejection
    auto dupNameRes = groupService.createGroup("Cliente Alpha");
    REQUIRE_FALSE(dupNameRes.has_value());
    REQUIRE(dupNameRes.error().kind == GroupServiceError::Kind::DuplicateName);

    // 4. Modify group members (add, remove, reorder)
    auto reordered = groupService.reorderRepositories(g1->id, {r2.id, r1.id});
    REQUIRE(reordered.has_value());
    REQUIRE(reordered->repositoryIds[0] == r2.id);
    REQUIRE(reordered->repositoryIds[1] == r1.id);

    // 5. Multi-group Quick Open context resolution with deduplication!
    auto quickOpenCtx = groupService.resolveQuickOpenContext({g1->id, g2->id});
    REQUIRE(quickOpenCtx.has_value());
    REQUIRE(quickOpenCtx->selectedGroupIds.size() == 2);
    REQUIRE(quickOpenCtx->totalRepositoryReferences == 4);

    // Unique repositories must be 3 (r2 appears only once, matching Page 03 wireframe)
    REQUIRE(quickOpenCtx->repositories.size() == 3);
    REQUIRE(quickOpenCtx->repositories[0].id == r2.id);
    REQUIRE(quickOpenCtx->repositories[1].id == r1.id);
    REQUIRE(quickOpenCtx->repositories[2].id == r3.id);
}

TEST_CASE (
"WorkspaceService gestisce composizione gruppi/repo, deduplicazione e sessioni"
,
"[unit][services][workspace]"
)
 {
    auto conn = createMemoryDb();
    SqliteRepositoryStore repoStore(conn);
    SqliteGroupStore groupStore(conn);
    SqliteWorkspaceStore wsStore(conn);

    RepositoryService repoService(repoStore);
    GroupService groupService(groupStore, repoStore);
    WorkspaceService wsService(wsStore, groupStore, repoStore);

    ScopedSandbox sandbox("gf_test_ws_service");
    auto p1 = sandbox.dir / "repo_1";
    auto p2 = sandbox.dir / "repo_2";
    auto p3 = sandbox.dir / "repo_3";
    makeFakeGitRepo(p1);
    makeFakeGitRepo(p2);
    makeFakeGitRepo(p3);

    auto r1 = repoService.registerRepository({.localPath = p1, .displayName = "Repo 1"}).value();
    auto r2 = repoService.registerRepository({.localPath = p2, .displayName = "Repo 2"}).value();
    auto r3 = repoService.registerRepository({.localPath = p3, .displayName = "Repo 3"}).value();

    auto grp = groupService.createGroup("Backend Group", {r1.id, r2.id}).value();

    // 1. Create Workspace with 1 group (r1, r2) and 1 overlapping direct repo (r2) + 1 new (r3)
    auto wsRes = wsService.createWorkspace("Lavoro Alpha", {grp.id}, {r2.id, r3.id});
    REQUIRE(wsRes.has_value());
    REQUIRE(wsRes->name == "Lavoro Alpha");

    const auto wsId = wsRes->id;

    // 2. Resolve effective repositories (Deduplicated: r1, r2 from group + r3 direct)
    auto effectiveRepos = wsService.resolveWorkspaceRepositories(wsId);
    REQUIRE(effectiveRepos.has_value());
    REQUIRE(effectiveRepos->size() == 3);
    REQUIRE((*effectiveRepos)[0].id == r1.id);
    REQUIRE((*effectiveRepos)[1].id == r2.id);
    REQUIRE((*effectiveRepos)[2].id == r3.id);

    // 3. Verify initial session automatically created
    auto session = wsService.loadOrCreateSession(wsId);
    REQUIRE(session.has_value());
    REQUIRE(session->workspaceId == wsId);
    REQUIRE(session->activeRepositoryId == r1.id);
    REQUIRE(session->openedRepositoryIds.size() == 1);
    REQUIRE(session->openedRepositoryIds[0] == r1.id);

    // 4. Modify and save session
    session->openedRepositoryIds.push_back(r3.id);
    session->activeRepositoryId = r3.id;
    session->layout.activeView = "changes";
    session->layout.leftSidebarCollapsed = true;
    REQUIRE(wsService.saveSession(*session).has_value());

    auto reloadedSession = wsService.loadOrCreateSession(wsId);
    REQUIRE(reloadedSession.has_value());
    REQUIRE(reloadedSession->openedRepositoryIds.size() == 2);
    REQUIRE(reloadedSession->activeRepositoryId == r3.id);
    REQUIRE(reloadedSession->layout.leftSidebarCollapsed);
    REQUIRE(reloadedSession->layout.activeView == "changes");

    // 5. Delete workspace
    REQUIRE(wsService.deleteWorkspace(wsId).value_or(false));
    REQUIRE(wsService.getAllWorkspaces()->empty());
}
