#include <catch2/catch_test_macros.hpp>

#include "core/modules/groups/bo/group.hpp"
#include "core/modules/repositories/bo/repository.hpp"
#include "core/modules/workspaces/bo/workspace.hpp"
#include "core/modules/workspaces/bo/workspace_session.hpp"
#include "infrastructure/persistence/groups/sqlite_group_store.hpp"
#include "infrastructure/persistence/repositories/sqlite_repository_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_statement.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/persistence/workspaces/sqlite_workspace_store.hpp"

using namespace gf::infra::persistence::sqlite;
using namespace gf::infra::persistence::repositories;
using namespace gf::infra::persistence::groups;
using namespace gf::infra::persistence::workspaces;

using gf::groups::bo::Group;
using gf::groups::bo::GroupId;
using gf::repositories::bo::Repository;
using gf::repositories::bo::RepositoryId;
using gf::repositories::bo::RepositoryPathStatus;
using gf::workspaces::bo::PanelLayoutState;
using gf::workspaces::bo::Workspace;
using gf::workspaces::bo::WorkspaceId;
using gf::workspaces::bo::WorkspaceSession;

namespace {

SqliteConnection createMigratedInMemoryDb() {
    auto connRes = SqliteConnection::openInMemory();
    REQUIRE(connRes.has_value());
    auto migRes = SchemaMigrator::migrateToLatest(*connRes);
    REQUIRE(migRes.has_value());
    return std::move(*connRes);
}

} // namespace

TEST_CASE("SqliteConnection & SchemaMigrator inizializzano correttamente il DB e sono idempotenti", "[integration][sqlite][migrator]") {
    auto connRes = SqliteConnection::openInMemory();
    REQUIRE(connRes.has_value());
    auto& conn = *connRes;

    auto v0 = SchemaMigrator::getCurrentVersion(conn);
    REQUIRE(v0.has_value());
    REQUIRE(*v0 == 0);

    auto migRes = SchemaMigrator::migrateToLatest(conn);
    REQUIRE(migRes.has_value());

    auto v1 = SchemaMigrator::getCurrentVersion(conn);
    REQUIRE(v1.has_value());
    REQUIRE(*v1 >= 2);

    // Subsequent idempotent execution
    auto migAgain = SchemaMigrator::migrateToLatest(conn);
    REQUIRE(migAgain.has_value());
    auto vStill1 = SchemaMigrator::getCurrentVersion(conn);
    REQUIRE(vStill1.has_value());
    REQUIRE(*vStill1 == *v1);
}

TEST_CASE("SqliteTransaction garantisce commit e rollback automatico in caso di errore", "[integration][sqlite][tx]") {
    auto conn = createMigratedInMemoryDb();

    // 1. Explicit commit
    {
        auto tx = SqliteTransaction::begin(conn);
        REQUIRE(tx.has_value());
        auto r = conn.execute("INSERT INTO groups (id, name, created_at_ms, updated_at_ms) VALUES ('g1', 'Alpha', 1, 1);");
        REQUIRE(r.has_value());
        REQUIRE(tx->commit().has_value());
    }

    // Verify persistence
    {
        auto stmt = SqliteStatement::prepare(conn, "SELECT COUNT(*) FROM groups WHERE id = 'g1';");
        REQUIRE(stmt.has_value());
        REQUIRE(stmt->step().value_or(false));
        REQUIRE(stmt->getInt(0) == 1);
    }

    // 2. Automatic rollback (without commit)
    {
        auto tx = SqliteTransaction::begin(conn);
        REQUIRE(tx.has_value());
        auto r = conn.execute("INSERT INTO groups (id, name, created_at_ms, updated_at_ms) VALUES ('g2', 'Beta', 2, 2);");
        REQUIRE(r.has_value());
        // tx goes out of scope without commit() -> performs ROLLBACK
    }

    // Verify absence of g2
    {
        auto stmt = SqliteStatement::prepare(conn, "SELECT COUNT(*) FROM groups WHERE id = 'g2';");
        REQUIRE(stmt.has_value());
        REQUIRE(stmt->step().value_or(false));
        REQUIRE(stmt->getInt(0) == 0);
    }
}

TEST_CASE("SqliteRepositoryStore gestisce CRUD, percorsi Unicode e unicita' percorso", "[integration][sqlite][repo_store]") {
    auto conn = createMigratedInMemoryDb();
    SqliteRepositoryStore repoStore(conn);

    Repository repo1{
        .id = RepositoryId("repo-front"),
        .displayName = "Frontend Web",
        .localPath = "/progetti/città/frontend",
        .pathStatus = RepositoryPathStatus::Valid,
        .registeredAtUnixEpochMs = 1000,
        .lastAccessedAtUnixEpochMs = 1050
    };

    REQUIRE(repoStore.save(repo1).has_value());

    // Lookup by ID
    auto fetched = repoStore.getById(RepositoryId("repo-front"));
    REQUIRE(fetched.has_value());
    REQUIRE(fetched->has_value());
    REQUIRE((*fetched)->id == repo1.id);
    REQUIRE((*fetched)->displayName == "Frontend Web");
    REQUIRE((*fetched)->localPath == "/progetti/città/frontend");

    // Lookup by Path
    auto fetchedByPath = repoStore.getByPath("/progetti/città/frontend");
    REQUIRE(fetchedByPath.has_value());
    REQUIRE(fetchedByPath->has_value());
    REQUIRE((*fetchedByPath)->id == repo1.id);

    // Attempting to insert a different ID with the SAME path (must fail with DuplicatePath)
    Repository dupPathRepo{
        .id = RepositoryId("repo-dup"),
        .displayName = "Altro nome",
        .localPath = "/progetti/città/frontend"
    };
    auto dupRes = repoStore.save(dupPathRepo);
    REQUIRE_FALSE(dupRes.has_value());
    REQUIRE(dupRes.error().kind == gf::repositories::ports::RepositoryStoreError::Kind::DuplicatePath);

    // Update path (reconnect path)
    auto updateRes = repoStore.updatePath(RepositoryId("repo-front"), "/nuovo/percorso/frontend");
    REQUIRE(updateRes.has_value());

    auto updatedRepo = repoStore.getById(RepositoryId("repo-front"));
    REQUIRE(updatedRepo.has_value());
    REQUIRE((*updatedRepo)->localPath == "/nuovo/percorso/frontend");

    // Deletion
    auto delRes = repoStore.remove(RepositoryId("repo-front"));
    REQUIRE(delRes.has_value());
    REQUIRE(*delRes);

    auto notFound = repoStore.getById(RepositoryId("repo-front"));
    REQUIRE(notFound.has_value());
    REQUIRE_FALSE(notFound->has_value());
}

TEST_CASE("SqliteGroupStore gestisce ordinamento, membership e cascade delete", "[integration][sqlite][group_store]") {
    auto conn = createMigratedInMemoryDb();
    SqliteRepositoryStore repoStore(conn);
    SqliteGroupStore groupStore(conn);

    // Create repositories to populate catalog
    Repository r1{.id = RepositoryId("r1"), .displayName = "Repo 1", .localPath = "/p1"};
    Repository r2{.id = RepositoryId("r2"), .displayName = "Repo 2", .localPath = "/p2"};
    Repository r3{.id = RepositoryId("r3"), .displayName = "Repo 3", .localPath = "/p3"};
    REQUIRE(repoStore.save(r1).has_value());
    REQUIRE(repoStore.save(r2).has_value());
    REQUIRE(repoStore.save(r3).has_value());

    Group group{
        .id = GroupId("grp-core"),
        .name = "Core Services",
        .repositoryIds = {RepositoryId("r3"), RepositoryId("r1")}, // Specific order: r3 then r1
        .createdAtUnixEpochMs = 100,
        .updatedAtUnixEpochMs = 100
    };

    REQUIRE(groupStore.save(group).has_value());

    // Verify ordering is preserved
    auto loaded = groupStore.getById(GroupId("grp-core"));
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->has_value());
    REQUIRE((*loaded)->repositoryIds.size() == 2);
    REQUIRE((*loaded)->repositoryIds[0] == RepositoryId("r3"));
    REQUIRE((*loaded)->repositoryIds[1] == RepositoryId("r1"));

    // Lookup groups containing r1
    auto containingR1 = groupStore.getGroupsContainingRepository(RepositoryId("r1"));
    REQUIRE(containingR1.has_value());
    REQUIRE(containingR1->size() == 1);
    REQUIRE((*containingR1)[0].id == GroupId("grp-core"));

    // If r3 is deleted from catalog, ON DELETE CASCADE foreign key must remove it from group
    REQUIRE(repoStore.remove(RepositoryId("r3")).has_value());

    auto loadedAfterRepoDel = groupStore.getById(GroupId("grp-core"));
    REQUIRE(loadedAfterRepoDel.has_value());
    REQUIRE(loadedAfterRepoDel->has_value());
    REQUIRE((*loadedAfterRepoDel)->repositoryIds.size() == 1);
    REQUIRE((*loadedAfterRepoDel)->repositoryIds[0] == RepositoryId("r1"));
}

TEST_CASE("SqliteWorkspaceStore gestisce gruppi, repo diretti, sessioni e cascade", "[integration][sqlite][ws_store]") {
    auto conn = createMigratedInMemoryDb();
    SqliteRepositoryStore repoStore(conn);
    SqliteGroupStore groupStore(conn);
    SqliteWorkspaceStore wsStore(conn);

    Repository r1{.id = RepositoryId("r1"), .displayName = "Repo 1", .localPath = "/p1"};
    Repository r2{.id = RepositoryId("r2"), .displayName = "Repo 2", .localPath = "/p2"};
    REQUIRE(repoStore.save(r1).has_value());
    REQUIRE(repoStore.save(r2).has_value());

    Group g1{.id = GroupId("g1"), .name = "Group 1", .repositoryIds = {RepositoryId("r1")}};
    REQUIRE(groupStore.save(g1).has_value());

    Workspace ws{
        .id = WorkspaceId("ws-alpha"),
        .name = "Workspace Alpha",
        .groupIds = {GroupId("g1")},
        .directRepositoryIds = {RepositoryId("r2")},
        .createdAtUnixEpochMs = 500,
        .updatedAtUnixEpochMs = 500
    };

    REQUIRE(wsStore.save(ws).has_value());

    auto loadedWs = wsStore.getById(WorkspaceId("ws-alpha"));
    REQUIRE(loadedWs.has_value());
    REQUIRE(loadedWs->has_value());
    REQUIRE((*loadedWs)->groupIds.size() == 1);
    REQUIRE((*loadedWs)->groupIds[0] == GroupId("g1"));
    REQUIRE((*loadedWs)->directRepositoryIds.size() == 1);
    REQUIRE((*loadedWs)->directRepositoryIds[0] == RepositoryId("r2"));

    // Save and retrieve session
    PanelLayoutState layout;
    layout.leftSidebarCollapsed = false;
    layout.rightSidebarCollapsed = true;
    layout.leftSidebarWidth = 270.0;
    layout.rightSidebarWidth = 380.0;
    layout.activeView = "history";

    WorkspaceSession session{
        .workspaceId = WorkspaceId("ws-alpha"),
        .schemaVersion = 1,
        .openedRepositoryIds = {RepositoryId("r1"), RepositoryId("r2")},
        .activeRepositoryId = RepositoryId("r2"),
        .layout = layout,
        .customStateJson = R"({"activeBranch":"feature"})",
        .updatedAtUnixEpochMs = 600
    };

    REQUIRE(wsStore.saveSession(session).has_value());

    auto loadedSession = wsStore.getSession(WorkspaceId("ws-alpha"));
    REQUIRE(loadedSession.has_value());
    REQUIRE(loadedSession->has_value());
    REQUIRE((*loadedSession)->workspaceId == WorkspaceId("ws-alpha"));
    REQUIRE((*loadedSession)->openedRepositoryIds.size() == 2);
    REQUIRE((*loadedSession)->openedRepositoryIds[0] == RepositoryId("r1"));
    REQUIRE((*loadedSession)->openedRepositoryIds[1] == RepositoryId("r2"));
    REQUIRE((*loadedSession)->activeRepositoryId == RepositoryId("r2"));
    REQUIRE((*loadedSession)->layout.activeView == "history");
    REQUIRE((*loadedSession)->layout.rightSidebarCollapsed);
    REQUIRE((*loadedSession)->layout.leftSidebarWidth == 270.0);
    REQUIRE((*loadedSession)->customStateJson == R"({"activeBranch":"feature"})");

    // Delete workspace with cascade on session
    REQUIRE(wsStore.remove(WorkspaceId("ws-alpha")).has_value());

    auto deletedWs = wsStore.getById(WorkspaceId("ws-alpha"));
    REQUIRE(deletedWs.has_value());
    REQUIRE_FALSE(deletedWs->has_value());

    // Session must also be deleted in cascade
    auto deletedSession = wsStore.getSession(WorkspaceId("ws-alpha"));
    REQUIRE(deletedSession.has_value());
    REQUIRE_FALSE(deletedSession->has_value());
}
