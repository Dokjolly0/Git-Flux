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

namespace {

struct ScopedSandbox {
    std::filesystem::path dir;
    ScopedSandbox(std::string_view name) {
        dir = std::filesystem::temp_directory_path() / name;
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        std::filesystem::create_directories(dir, ec);
    }
    ~ScopedSandbox() {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
};

SqliteConnection createMemoryDb() {
    auto connRes = SqliteConnection::openInMemory();
    REQUIRE(connRes.has_value());
    REQUIRE(SchemaMigrator::migrateToLatest(*connRes).has_value());
    return std::move(*connRes);
}

void makeFakeGitRepo(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::create_directories(path / ".git", ec);
    std::ofstream f(path / ".git/config");
    f << "[core]\n";
}

} // namespace

TEST_CASE("RepositoryService gestisce validazione, registrazione e riconnessione path", "[unit][services][repository]") {
    auto conn = createMemoryDb();
    SqliteRepositoryStore store(conn);
    RepositoryService service(store);

    ScopedSandbox sandbox("gf_test_repo_service");
    const auto validRepoPath = sandbox.dir / "my_project";
    makeFakeGitRepo(validRepoPath);

    const auto notGitPath = sandbox.dir / "regular_folder";
    std::filesystem::create_directories(notGitPath);

    // 1. Percorso vuoto -> InvalidPath
    auto emptyRes = service.registerRepository({.localPath = {}, .displayName = std::nullopt});
    REQUIRE_FALSE(emptyRes.has_value());
    REQUIRE(emptyRes.error().kind == RepositoryServiceError::Kind::InvalidPath);

    // 2. Percorso inesistente -> PathNotFound
    auto nonExistentRes = service.registerRepository({.localPath = sandbox.dir / "ghost_folder", .displayName = std::nullopt});
    REQUIRE_FALSE(nonExistentRes.has_value());
    REQUIRE(nonExistentRes.error().kind == RepositoryServiceError::Kind::PathNotFound);

    // 3. Cartella senza .git -> NotAGitRepository
    auto notGitRes = service.registerRepository({.localPath = notGitPath, .displayName = std::nullopt});
    REQUIRE_FALSE(notGitRes.has_value());
    REQUIRE(notGitRes.error().kind == RepositoryServiceError::Kind::NotAGitRepository);

    // 4. Registrazione repository valido (senza display name esplicito: calcola da folder name)
    auto regRes = service.registerRepository({.localPath = validRepoPath, .displayName = std::nullopt});
    REQUIRE(regRes.has_value());
    REQUIRE(regRes->displayName == "my_project");
    REQUIRE(regRes->pathStatus == RepositoryPathStatus::Valid);
    REQUIRE(regRes->registeredAtUnixEpochMs > 0);

    const auto repoId = regRes->id;

    // 5. Tentativo di ri-registrare lo stesso path -> AlreadyRegistered
    auto dupRes = service.registerRepository({.localPath = validRepoPath, .displayName = std::nullopt});
    REQUIRE_FALSE(dupRes.has_value());
    REQUIRE(dupRes.error().kind == RepositoryServiceError::Kind::AlreadyRegistered);

    // 6. Verifica stato con path esistente
    auto statusCheck = service.checkRepositoryStatus(repoId);
    REQUIRE(statusCheck.has_value());
    REQUIRE(statusCheck->pathStatus == RepositoryPathStatus::Valid);

    // 7. Simula spostamento cartella (Missing Repo)
    const auto movedRepoPath = sandbox.dir / "project_renamed";
    std::filesystem::rename(validRepoPath, movedRepoPath);

    auto statusMissing = service.checkRepositoryStatus(repoId);
    REQUIRE(statusMissing.has_value());
    REQUIRE(statusMissing->pathStatus == RepositoryPathStatus::Missing);

    // 8. Riconnessione percorso (mantenendo lo STESSO RepositoryId!)
    auto reconnectRes = service.reconnectRepositoryPath(repoId, movedRepoPath);
    REQUIRE(reconnectRes.has_value());
    REQUIRE(reconnectRes->id == repoId);
    REQUIRE(reconnectRes->pathStatus == RepositoryPathStatus::Valid);
    REQUIRE(reconnectRes->localPath == std::filesystem::weakly_canonical(movedRepoPath));

    // 9. Unregister repository
    REQUIRE(service.unregisterRepository(repoId).value_or(false));
    REQUIRE(service.getAllRepositories()->empty());
}

TEST_CASE("GroupService gestisce membership e deduplicazione deterministica per Quick Open", "[unit][services][group]") {
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

    // 1. Validazione nome vuoto
    auto emptyRes = groupService.createGroup("");
    REQUIRE_FALSE(emptyRes.has_value());
    REQUIRE(emptyRes.error().kind == GroupServiceError::Kind::InvalidName);

    // 2. Creazione gruppi con repository condiviso (r2 è sia in g1 sia in g2)
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

    // 4. Modifiche ai membri del gruppo (add, remove, reorder)
    auto reordered = groupService.reorderRepositories(g1->id, {r2.id, r1.id});
    REQUIRE(reordered.has_value());
    REQUIRE(reordered->repositoryIds[0] == r2.id);
    REQUIRE(reordered->repositoryIds[1] == r1.id);

    // 5. Risoluzione contesto Quick Open multi-gruppo con deduplicazione!
    auto quickOpenCtx = groupService.resolveQuickOpenContext({g1->id, g2->id});
    REQUIRE(quickOpenCtx.has_value());
    REQUIRE(quickOpenCtx->selectedGroupIds.size() == 2);
    REQUIRE(quickOpenCtx->totalRepositoryReferences == 4);

    // I repository unici devono essere 3 (r2 appare una sola volta, rispettando Pagina 03 wireframe)
    REQUIRE(quickOpenCtx->repositories.size() == 3);
    REQUIRE(quickOpenCtx->repositories[0].id == r2.id);
    REQUIRE(quickOpenCtx->repositories[1].id == r1.id);
    REQUIRE(quickOpenCtx->repositories[2].id == r3.id);
}

TEST_CASE("WorkspaceService gestisce composizione gruppi/repo, deduplicazione e sessioni", "[unit][services][workspace]") {
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

    // 1. Creazione Workspace con 1 gruppo (r1, r2) e 1 repo diretto sovrapposto (r2) + 1 nuovo (r3)
    auto wsRes = wsService.createWorkspace("Lavoro Alpha", {grp.id}, {r2.id, r3.id});
    REQUIRE(wsRes.has_value());
    REQUIRE(wsRes->name == "Lavoro Alpha");

    const auto wsId = wsRes->id;

    // 2. Risoluzione repository effettivi (Deduplicati: r1, r2 da gruppo + r3 diretto)
    auto effectiveRepos = wsService.resolveWorkspaceRepositories(wsId);
    REQUIRE(effectiveRepos.has_value());
    REQUIRE(effectiveRepos->size() == 3);
    REQUIRE((*effectiveRepos)[0].id == r1.id);
    REQUIRE((*effectiveRepos)[1].id == r2.id);
    REQUIRE((*effectiveRepos)[2].id == r3.id);

    // 3. Verifica sessione iniziale creata automaticamente
    auto session = wsService.loadOrCreateSession(wsId);
    REQUIRE(session.has_value());
    REQUIRE(session->workspaceId == wsId);
    REQUIRE(session->activeRepositoryId == r1.id);
    REQUIRE(session->openedRepositoryIds.size() == 1);
    REQUIRE(session->openedRepositoryIds[0] == r1.id);

    // 4. Modifica e salvataggio sessione
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

    // 5. Eliminazione workspace
    REQUIRE(wsService.deleteWorkspace(wsId).value_or(false));
    REQUIRE(wsService.getAllWorkspaces()->empty());
}
