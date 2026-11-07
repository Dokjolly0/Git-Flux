#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_statement.hpp"

#include <chrono>

namespace gf::infra::persistence::sqlite {

namespace {

constexpr std::string_view kCreateMigrationTableSql = R"(
CREATE TABLE IF NOT EXISTS schema_migrations (
    version INTEGER PRIMARY KEY,
    applied_at_ms INTEGER NOT NULL
);
)";

constexpr std::string_view kMigrationV1Sql = R"(
-- 1. Repositories
CREATE TABLE IF NOT EXISTS repositories (
    id TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    local_path TEXT NOT NULL UNIQUE,
    path_status INTEGER NOT NULL DEFAULT 0,
    registered_at_ms INTEGER NOT NULL,
    last_accessed_at_ms INTEGER NOT NULL
);

-- 2. Groups
CREATE TABLE IF NOT EXISTS groups (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    created_at_ms INTEGER NOT NULL,
    updated_at_ms INTEGER NOT NULL
);

-- 3. Group Repositories (N:N join con ordine)
CREATE TABLE IF NOT EXISTS group_repositories (
    group_id TEXT NOT NULL,
    repository_id TEXT NOT NULL,
    sort_order INTEGER NOT NULL,
    PRIMARY KEY (group_id, repository_id),
    FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE,
    FOREIGN KEY (repository_id) REFERENCES repositories(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_group_repo_sort ON group_repositories(group_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_group_repo_repo ON group_repositories(repository_id);

-- 4. Workspaces
CREATE TABLE IF NOT EXISTS workspaces (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    created_at_ms INTEGER NOT NULL,
    updated_at_ms INTEGER NOT NULL
);

-- 5. Workspace Groups (N:N join con ordine)
CREATE TABLE IF NOT EXISTS workspace_groups (
    workspace_id TEXT NOT NULL,
    group_id TEXT NOT NULL,
    sort_order INTEGER NOT NULL,
    PRIMARY KEY (workspace_id, group_id),
    FOREIGN KEY (workspace_id) REFERENCES workspaces(id) ON DELETE CASCADE,
    FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_workspace_group_sort ON workspace_groups(workspace_id, sort_order);

-- 6. Workspace Repositories (repo diretti con ordine)
CREATE TABLE IF NOT EXISTS workspace_repositories (
    workspace_id TEXT NOT NULL,
    repository_id TEXT NOT NULL,
    sort_order INTEGER NOT NULL,
    PRIMARY KEY (workspace_id, repository_id),
    FOREIGN KEY (workspace_id) REFERENCES workspaces(id) ON DELETE CASCADE,
    FOREIGN KEY (repository_id) REFERENCES repositories(id) ON DELETE CASCADE
);
CREATE INDEX IF NOT EXISTS idx_workspace_repo_sort ON workspace_repositories(workspace_id, sort_order);

-- 7. Workspace Sessions (1:1 con Workspace)
CREATE TABLE IF NOT EXISTS workspace_sessions (
    workspace_id TEXT PRIMARY KEY,
    schema_version INTEGER NOT NULL DEFAULT 1,
    opened_repository_ids TEXT NOT NULL,
    active_repository_id TEXT,
    left_sidebar_collapsed INTEGER NOT NULL DEFAULT 0,
    right_sidebar_collapsed INTEGER NOT NULL DEFAULT 0,
    left_sidebar_width REAL NOT NULL DEFAULT 260.0,
    right_sidebar_width REAL NOT NULL DEFAULT 360.0,
    active_view TEXT NOT NULL DEFAULT 'graph',
    custom_state_json TEXT NOT NULL DEFAULT '{}',
    updated_at_ms INTEGER NOT NULL,
    FOREIGN KEY (workspace_id) REFERENCES workspaces(id) ON DELETE CASCADE
);
)";

} // namespace

std::expected<int, SqliteError>
SchemaMigrator::getCurrentVersion(SqliteConnection& conn) {
    auto initRes = conn.execute(kCreateMigrationTableSql);
    if (!initRes) {
        return std::unexpected(initRes.error());
    }

    auto stmtRes = SqliteStatement::prepare(
        conn, "SELECT COALESCE(MAX(version), 0) FROM schema_migrations;");
    if (!stmtRes) {
        return std::unexpected(stmtRes.error());
    }

    auto stepRes = stmtRes->step();
    if (!stepRes) {
        return std::unexpected(stepRes.error());
    }
    if (*stepRes) {
        return stmtRes->getInt(0);
    }
    return 0;
}

std::expected<void, SqliteError>
SchemaMigrator::migrateToLatest(SqliteConnection& conn) {
    auto curVer = getCurrentVersion(conn);
    if (!curVer) {
        return std::unexpected(curVer.error());
    }

    if (*curVer < 1) {
        auto tx = SqliteTransaction::begin(conn);
        if (!tx) {
            return std::unexpected(tx.error());
        }

        auto execRes = conn.execute(kMigrationV1Sql);
        if (!execRes) {
            return std::unexpected(execRes.error());
        }

        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();

        auto recordStmt = SqliteStatement::prepare(
            conn, "INSERT INTO schema_migrations (version, applied_at_ms) VALUES (1, ?);");
        if (!recordStmt) {
            return std::unexpected(recordStmt.error());
        }
        auto bindRes = recordStmt->bindInt64(1, nowMs);
        if (!bindRes) {
            return std::unexpected(bindRes.error());
        }
        auto stepRes = recordStmt->step();
        if (!stepRes) {
            return std::unexpected(stepRes.error());
        }

        auto commitRes = tx->commit();
        if (!commitRes) {
            return std::unexpected(commitRes.error());
        }
    }

    return {};
}

} // namespace gf::infra::persistence::sqlite
