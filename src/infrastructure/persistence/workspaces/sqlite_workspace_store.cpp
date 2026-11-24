#include "infrastructure/persistence/workspaces/sqlite_workspace_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_statement.hpp"

#include <sqlite3.h>
#include <sstream>

namespace gf::infra::persistence::workspaces
{
    using groups::bo::GroupId;
    using repositories::bo::RepositoryId;
    using gf::workspaces::bo::PanelLayoutState;
    using gf::workspaces::bo::Workspace;
    using gf::workspaces::bo::WorkspaceId;
    using gf::workspaces::bo::WorkspaceSession;
    using gf::workspaces::ports::WorkspaceStoreError;

    namespace
    {
        WorkspaceStoreError makeStorageError(const sqlite::SqliteError& err)
        {
            return WorkspaceStoreError{
                .kind = WorkspaceStoreError::Kind::StorageError,
                .message = err.message
            };
        }

        std::string serializeRepoIds(const std::vector<RepositoryId>& ids)
        {
            std::string result;
            for (std::size_t i = 0; i < ids.size(); ++i)
            {
                if (i > 0)
                {
                    result.push_back(';');
                }
                result.append(ids[i].value());
            }
            return result;
        }

        std::vector<RepositoryId> deserializeRepoIds(const std::string_view sv)
        {
            std::vector<RepositoryId> ids;
            if (sv.empty())
            {
                return ids;
            }
            std::size_t start = 0;
            while (start < sv.size())
            {
                const auto end = sv.find(';', start);
                if (end == std::string_view::npos)
                {
                    ids.emplace_back(sv.substr(start));
                    break;
                }
                ids.emplace_back(sv.substr(start, end - start));
                start = end + 1;
            }
            return ids;
        }
    } // namespace

    SqliteWorkspaceStore::SqliteWorkspaceStore(sqlite::SqliteConnection& conn) noexcept
        : conn_(conn)
    {
    }

    std::expected<std::vector<GroupId>, WorkspaceStoreError>
    SqliteWorkspaceStore::loadWorkspaceGroupIds(const WorkspaceId& wsId) const
    {
        constexpr std::string_view kSql = R"(
SELECT group_id
FROM workspace_groups
WHERE workspace_id = ?
ORDER BY sort_order ASC;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, wsId.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        std::vector<GroupId> ids;
        while (true)
        {
            auto stepRes = stmtRes->step();
            if (!stepRes)
            {
                return std::unexpected(makeStorageError(stepRes.error()));
            }
            if (!*stepRes)
            {
                break;
            }
            ids.emplace_back(stmtRes->getText(0));
        }
        return ids;
    }

    std::expected<std::vector<RepositoryId>, WorkspaceStoreError>
    SqliteWorkspaceStore::loadWorkspaceDirectRepoIds(const WorkspaceId& wsId) const
    {
        constexpr std::string_view kSql = R"(
SELECT repository_id
FROM workspace_repositories
WHERE workspace_id = ?
ORDER BY sort_order ASC;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, wsId.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        std::vector<RepositoryId> ids;
        while (true)
        {
            auto stepRes = stmtRes->step();
            if (!stepRes)
            {
                return std::unexpected(makeStorageError(stepRes.error()));
            }
            if (!*stepRes)
            {
                break;
            }
            ids.emplace_back(stmtRes->getText(0));
        }
        return ids;
    }

    std::expected<void, WorkspaceStoreError>
    SqliteWorkspaceStore::save(const Workspace& workspace)
    {
        if (!workspace.isValid())
        {
            return std::unexpected(WorkspaceStoreError{
                .kind = WorkspaceStoreError::Kind::StorageError,
                .message = "Workspace non valido (ID o nome vuoto)"
            });
        }

        auto tx = sqlite::SqliteTransaction::begin(conn_);
        if (!tx)
        {
            return std::unexpected(makeStorageError(tx.error()));
        }

        constexpr std::string_view kUpsertWsSql = R"(
INSERT INTO workspaces (id, name, created_at_ms, updated_at_ms)
VALUES (?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
    name = excluded.name,
    updated_at_ms = excluded.updated_at_ms;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kUpsertWsSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, workspace.id.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindText(2, workspace.name); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindInt64(3, workspace.createdAtUnixEpochMs); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindInt64(4, workspace.updatedAtUnixEpochMs); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }

        if (const auto stepRes = stmtRes->step(); !stepRes)
        {
            if (stepRes.error().code == SQLITE_CONSTRAINT_UNIQUE || stepRes.error().code == SQLITE_CONSTRAINT)
            {
                return std::unexpected(WorkspaceStoreError{
                    .kind = WorkspaceStoreError::Kind::DuplicateName,
                    .message = "Nome workspace già esistente"
                });
            }
            return std::unexpected(makeStorageError(stepRes.error()));
        }

        // 1. workspace_groups
        auto delGroupsStmt = sqlite::SqliteStatement::prepare(
            conn_, "DELETE FROM workspace_groups WHERE workspace_id = ?;");
        if (!delGroupsStmt)
        {
            return std::unexpected(makeStorageError(delGroupsStmt.error()));
        }
        if (const auto bindRes = delGroupsStmt->bindText(1, workspace.id.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto delStepRes = delGroupsStmt->step(); !delStepRes)
        {
            return std::unexpected(makeStorageError(delStepRes.error()));
        }

        if (!workspace.groupIds.empty())
        {
            auto insGrpStmt = sqlite::SqliteStatement::prepare(
                conn_, "INSERT INTO workspace_groups (workspace_id, group_id, sort_order) VALUES (?, ?, ?);");
            if (!insGrpStmt)
            {
                return std::unexpected(makeStorageError(insGrpStmt.error()));
            }

            for (int i = 0; i < static_cast<int>(workspace.groupIds.size()); ++i)
            {
                insGrpStmt->reset();
                insGrpStmt->clearBindings();
                if (const auto bindRes = insGrpStmt->bindText(1, workspace.id.value()); !bindRes)
                {
                    return std::unexpected(makeStorageError(bindRes.error()));
                }
                if (const auto bindRes = insGrpStmt->bindText(2, workspace.groupIds[static_cast<std::size_t>(i)].value()); !bindRes)
                {
                    return std::unexpected(makeStorageError(bindRes.error()));
                }
                if (const auto bindRes = insGrpStmt->bindInt(3, i); !bindRes)
                {
                    return std::unexpected(makeStorageError(bindRes.error()));
                }
                if (const auto insStep = insGrpStmt->step(); !insStep)
                {
                    return std::unexpected(WorkspaceStoreError{
                        .kind = WorkspaceStoreError::Kind::StorageError,
                        .message = "Gruppo referenziato inesistente o errore foreign key"
                    });
                }
            }
        }

        // 2. workspace_repositories (direct)
        auto delReposStmt = sqlite::SqliteStatement::prepare(
            conn_, "DELETE FROM workspace_repositories WHERE workspace_id = ?;");
        if (!delReposStmt)
        {
            return std::unexpected(makeStorageError(delReposStmt.error()));
        }
        if (const auto bindRes = delReposStmt->bindText(1, workspace.id.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto delStepRes = delReposStmt->step(); !delStepRes)
        {
            return std::unexpected(makeStorageError(delStepRes.error()));
        }

        if (!workspace.directRepositoryIds.empty())
        {
            auto insRepoStmt = sqlite::SqliteStatement::prepare(
                conn_,
                "INSERT INTO workspace_repositories (workspace_id, repository_id, sort_order) VALUES (?, ?, ?);");
            if (!insRepoStmt)
            {
                return std::unexpected(makeStorageError(insRepoStmt.error()));
            }

            for (int i = 0; i < static_cast<int>(workspace.directRepositoryIds.size()); ++i)
            {
                insRepoStmt->reset();
                insRepoStmt->clearBindings();
                if (const auto bindRes = insRepoStmt->bindText(1, workspace.id.value()); !bindRes)
                {
                    return std::unexpected(makeStorageError(bindRes.error()));
                }
                if (const auto bindRes = insRepoStmt->bindText(2, workspace.directRepositoryIds[static_cast<std::size_t>(i)].value()); !bindRes)
                {
                    return std::unexpected(makeStorageError(bindRes.error()));
                }
                if (const auto bindRes = insRepoStmt->bindInt(3, i); !bindRes)
                {
                    return std::unexpected(makeStorageError(bindRes.error()));
                }
                if (const auto insStep = insRepoStmt->step(); !insStep)
                {
                    return std::unexpected(WorkspaceStoreError{
                        .kind = WorkspaceStoreError::Kind::StorageError,
                        .message = "Repository referenziato inesistente o errore foreign key"
                    });
                }
            }
        }

        if (const auto commitRes = tx->commit(); !commitRes)
        {
            return std::unexpected(makeStorageError(commitRes.error()));
        }

        return {};
    }

    std::expected<std::optional<Workspace>, WorkspaceStoreError>
    SqliteWorkspaceStore::getById(const WorkspaceId& id)
    {
        constexpr std::string_view kSql = R"(
SELECT id, name, created_at_ms, updated_at_ms
FROM workspaces
WHERE id = ?;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, id.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        auto stepRes = stmtRes->step();
        if (!stepRes)
        {
            return std::unexpected(makeStorageError(stepRes.error()));
        }

        if (!*stepRes)
        {
            return std::nullopt;
        }

        Workspace ws;
        ws.id = WorkspaceId(stmtRes->getText(0));
        ws.name = stmtRes->getText(1);
        ws.createdAtUnixEpochMs = stmtRes->getInt64(2);
        ws.updatedAtUnixEpochMs = stmtRes->getInt64(3);

        auto groupsRes = loadWorkspaceGroupIds(ws.id);
        if (!groupsRes)
        {
            return std::unexpected(groupsRes.error());
        }
        ws.groupIds = std::move(*groupsRes);

        auto reposRes = loadWorkspaceDirectRepoIds(ws.id);
        if (!reposRes)
        {
            return std::unexpected(reposRes.error());
        }
        ws.directRepositoryIds = std::move(*reposRes);

        return ws;
    }

    std::expected<std::vector<Workspace>, WorkspaceStoreError>
    SqliteWorkspaceStore::getAll()
    {
        constexpr std::string_view kSql = R"(
SELECT id, name, created_at_ms, updated_at_ms
FROM workspaces
ORDER BY name COLLATE NOCASE ASC;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        std::vector<Workspace> list;
        while (true)
        {
            auto stepRes = stmtRes->step();
            if (!stepRes)
            {
                return std::unexpected(makeStorageError(stepRes.error()));
            }
            if (!*stepRes)
            {
                break;
            }

            Workspace ws;
            ws.id = WorkspaceId(stmtRes->getText(0));
            ws.name = stmtRes->getText(1);
            ws.createdAtUnixEpochMs = stmtRes->getInt64(2);
            ws.updatedAtUnixEpochMs = stmtRes->getInt64(3);
            list.push_back(std::move(ws));
        }

        for (auto& ws : list)
        {
            auto groupsRes = loadWorkspaceGroupIds(ws.id);
            if (!groupsRes)
            {
                return std::unexpected(groupsRes.error());
            }
            ws.groupIds = std::move(*groupsRes);

            auto reposRes = loadWorkspaceDirectRepoIds(ws.id);
            if (!reposRes)
            {
                return std::unexpected(reposRes.error());
            }
            ws.directRepositoryIds = std::move(*reposRes);
        }

        return list;
    }

    std::expected<bool, WorkspaceStoreError>
    SqliteWorkspaceStore::remove(const WorkspaceId& id)
    {
        constexpr std::string_view kSql = "DELETE FROM workspaces WHERE id = ?;";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, id.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto stepRes = stmtRes->step(); !stepRes)
        {
            return std::unexpected(makeStorageError(stepRes.error()));
        }

        return conn_.changes() > 0;
    }

    std::expected<void, WorkspaceStoreError>
    SqliteWorkspaceStore::saveSession(const WorkspaceSession& session)
    {
        if (!session.isValid())
        {
            return std::unexpected(WorkspaceStoreError{
                .kind = WorkspaceStoreError::Kind::StorageError,
                .message = "WorkspaceSession non valida (ID vuoto o schema <= 0)"
            });
        }

        constexpr std::string_view kSql = R"(
INSERT INTO workspace_sessions (
    workspace_id, schema_version, opened_repository_ids, active_repository_id,
    left_sidebar_collapsed, right_sidebar_collapsed, left_sidebar_width, right_sidebar_width,
    active_view, custom_state_json, updated_at_ms
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(workspace_id) DO UPDATE SET
    schema_version = excluded.schema_version,
    opened_repository_ids = excluded.opened_repository_ids,
    active_repository_id = excluded.active_repository_id,
    left_sidebar_collapsed = excluded.left_sidebar_collapsed,
    right_sidebar_collapsed = excluded.right_sidebar_collapsed,
    left_sidebar_width = excluded.left_sidebar_width,
    right_sidebar_width = excluded.right_sidebar_width,
    active_view = excluded.active_view,
    custom_state_json = excluded.custom_state_json,
    updated_at_ms = excluded.updated_at_ms;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, session.workspaceId.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindInt(2, session.schemaVersion); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindText(3, serializeRepoIds(session.openedRepositoryIds)); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (session.activeRepositoryId)
        {
            if (const auto bindRes = stmtRes->bindText(4, session.activeRepositoryId->value()); !bindRes)
            {
                return std::unexpected(makeStorageError(bindRes.error()));
            }
        }
        else
        {
            if (const auto bindRes = stmtRes->bindNull(4); !bindRes)
            {
                return std::unexpected(makeStorageError(bindRes.error()));
            }
        }
        if (const auto bindRes = stmtRes->bindInt(5, session.layout.leftSidebarCollapsed ? 1 : 0); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindInt(6, session.layout.rightSidebarCollapsed ? 1 : 0); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindDouble(7, session.layout.leftSidebarWidth); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindDouble(8, session.layout.rightSidebarWidth); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindText(9, session.layout.activeView); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindText(10, session.customStateJson.empty() ? "{}" : session.customStateJson); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindInt64(11, session.updatedAtUnixEpochMs); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }

        if (const auto stepRes = stmtRes->step(); !stepRes)
        {
            return std::unexpected(WorkspaceStoreError{
                .kind = WorkspaceStoreError::Kind::StorageError,
                .message = "Impossibile salvare sessione (workspace inesistente o errore foreign key)"
            });
        }

        return {};
    }

    std::expected<std::optional<WorkspaceSession>, WorkspaceStoreError>
    SqliteWorkspaceStore::getSession(const WorkspaceId& id)
    {
        constexpr std::string_view kSql = R"(
SELECT
    workspace_id, schema_version, opened_repository_ids, active_repository_id,
    left_sidebar_collapsed, right_sidebar_collapsed, left_sidebar_width, right_sidebar_width,
    active_view, custom_state_json, updated_at_ms
FROM workspace_sessions
WHERE workspace_id = ?;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, id.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        auto stepRes = stmtRes->step();
        if (!stepRes)
        {
            return std::unexpected(makeStorageError(stepRes.error()));
        }

        if (!*stepRes)
        {
            return std::nullopt;
        }

        WorkspaceSession s;
        s.workspaceId = WorkspaceId(stmtRes->getText(0));
        s.schemaVersion = stmtRes->getInt(1);
        s.openedRepositoryIds = deserializeRepoIds(stmtRes->getText(2));
        if (!stmtRes->isNull(3))
        {
            s.activeRepositoryId = RepositoryId(stmtRes->getText(3));
        }
        s.layout.leftSidebarCollapsed = stmtRes->getInt(4) != 0;
        s.layout.rightSidebarCollapsed = stmtRes->getInt(5) != 0;
        s.layout.leftSidebarWidth = stmtRes->getDouble(6);
        s.layout.rightSidebarWidth = stmtRes->getDouble(7);
        s.layout.activeView = stmtRes->getText(8);
        s.customStateJson = stmtRes->getText(9);
        s.updatedAtUnixEpochMs = stmtRes->getInt64(10);

        return s;
    }
} // namespace gf::infra::persistence::workspaces
