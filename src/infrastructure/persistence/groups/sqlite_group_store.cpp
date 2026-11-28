#include "infrastructure/persistence/groups/sqlite_group_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_statement.hpp"
#include "infrastructure/persistence/sqlite/sqlite_store_helpers.hpp"

#include <sqlite3.h>

namespace gf::infra::persistence::groups
{
    using gf::groups::bo::Group;
    using gf::groups::bo::GroupId;
    using gf::groups::ports::GroupStoreError;
    using repositories::bo::RepositoryId;

    namespace
    {
        GroupStoreError makeStorageError(const sqlite::SqliteError& err)
        {
            return sqlite::makeStorageError<GroupStoreError>(err);
        }

        Group rowToGroup(const sqlite::SqliteStatement& stmt)
        {
            Group grp;
            grp.id = GroupId(stmt.getText(0));
            grp.name = stmt.getText(1);
            grp.color = stmt.getText(2);
            grp.createdAtUnixEpochMs = stmt.getInt64(3);
            grp.updatedAtUnixEpochMs = stmt.getInt64(4);
            return grp;
        }
    } // namespace

    SqliteGroupStore::SqliteGroupStore(sqlite::SqliteConnection& conn) noexcept
        : conn_(conn)
    {
    }

    std::expected<std::vector<RepositoryId>, GroupStoreError>
    SqliteGroupStore::loadRepositoryIds(const GroupId& groupId) const
    {
        return sqlite::loadOrderedRelationIds<RepositoryId, GroupStoreError>(
            conn_,
            "SELECT repository_id FROM group_repositories WHERE group_id = ? ORDER BY sort_order ASC;",
            groupId.value());
    }

    std::expected<void, GroupStoreError>
    SqliteGroupStore::save(const Group& group)
    {
        if (!group.isValid())
        {
            return std::unexpected(GroupStoreError{
                .kind = GroupStoreError::Kind::StorageError,
                .message = "Gruppo non valido (ID o nome vuoto)"
            });
        }

        auto tx = sqlite::SqliteTransaction::begin(conn_);
        if (!tx)
        {
            return std::unexpected(makeStorageError(tx.error()));
        }

        constexpr std::string_view kUpsertGroupSql = R"(
INSERT INTO groups (id, name, color, created_at_ms, updated_at_ms)
VALUES (?, ?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
    name = excluded.name,
    color = excluded.color,
    updated_at_ms = excluded.updated_at_ms;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kUpsertGroupSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, group.id.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindText(2, group.name); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindText(3, group.color); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindInt64(4, group.createdAtUnixEpochMs); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindInt64(5, group.updatedAtUnixEpochMs); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }

        if (const auto stepRes = stmtRes->step(); !stepRes)
        {
            if (stepRes.error().code == SQLITE_CONSTRAINT_UNIQUE || stepRes.error().code == SQLITE_CONSTRAINT)
            {
                return std::unexpected(GroupStoreError{
                    .kind = GroupStoreError::Kind::DuplicateName,
                    .message = "Nome gruppo già esistente"
                });
            }
            return std::unexpected(makeStorageError(stepRes.error()));
        }

        // Rebuild ordered repository associations
        if (const auto relRes = sqlite::replaceOrderedRelationIds<RepositoryId, GroupStoreError>(
                conn_,
                "DELETE FROM group_repositories WHERE group_id = ?;",
                "INSERT INTO group_repositories (group_id, repository_id, sort_order) VALUES (?, ?, ?);",
                group.id.value(),
                group.repositoryIds,
                "Repository non presente nel catalogo o errore foreign key");
            !relRes)
        {
            return std::unexpected(relRes.error());
        }

        if (const auto commitRes = tx->commit(); !commitRes)
        {
            return std::unexpected(makeStorageError(commitRes.error()));
        }

        return {};
    }

    std::expected<std::optional<Group>, GroupStoreError>
    SqliteGroupStore::getById(const GroupId& id)
    {
        constexpr std::string_view kSql = R"(
SELECT id, name, color, created_at_ms, updated_at_ms
FROM groups
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

        Group grp = rowToGroup(*stmtRes);

        auto reposRes = loadRepositoryIds(grp.id);
        if (!reposRes)
        {
            return std::unexpected(reposRes.error());
        }
        grp.repositoryIds = std::move(*reposRes);

        return grp;
    }

    std::expected<std::vector<Group>, GroupStoreError>
    SqliteGroupStore::getAll()
    {
        constexpr std::string_view kSql = R"(
SELECT id, name, color, created_at_ms, updated_at_ms
FROM groups
ORDER BY name COLLATE NOCASE ASC;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        std::vector<Group> groups;
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

            groups.push_back(rowToGroup(*stmtRes));
        }

        for (auto& g : groups)
        {
            auto reposRes = loadRepositoryIds(g.id);
            if (!reposRes)
            {
                return std::unexpected(reposRes.error());
            }
            g.repositoryIds = std::move(*reposRes);
        }

        return groups;
    }

    std::expected<bool, GroupStoreError>
    SqliteGroupStore::remove(const GroupId& id)
    {
        return sqlite::deleteRowById<GroupStoreError>(
            conn_, "DELETE FROM groups WHERE id = ?;", id.value(), makeStorageError);
    }

    std::expected<std::vector<Group>, GroupStoreError>
    SqliteGroupStore::getGroupsContainingRepository(const RepositoryId& repoId)
    {
        constexpr std::string_view kSql = R"(
SELECT DISTINCT g.id, g.name, g.color, g.created_at_ms, g.updated_at_ms
FROM groups g
JOIN group_repositories gr ON g.id = gr.group_id
WHERE gr.repository_id = ?
ORDER BY g.name COLLATE NOCASE ASC;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, repoId.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        std::vector<Group> groups;
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

            groups.push_back(rowToGroup(*stmtRes));
        }

        for (auto& g : groups)
        {
            auto reposRes = loadRepositoryIds(g.id);
            if (!reposRes)
            {
                return std::unexpected(reposRes.error());
            }
            g.repositoryIds = std::move(*reposRes);
        }

        return groups;
    }
} // namespace gf::infra::persistence::groups
