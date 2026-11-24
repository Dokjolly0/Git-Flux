#include "infrastructure/persistence/groups/sqlite_group_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_statement.hpp"

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
            return GroupStoreError{
                .kind = GroupStoreError::Kind::StorageError,
                .message = err.message
            };
        }
    } // namespace

    SqliteGroupStore::SqliteGroupStore(sqlite::SqliteConnection& conn) noexcept
        : conn_(conn)
    {
    }

    std::expected<std::vector<RepositoryId>, GroupStoreError>
    SqliteGroupStore::loadRepositoryIds(const GroupId& groupId) const
    {
        constexpr std::string_view kSql = R"(
SELECT repository_id
FROM group_repositories
WHERE group_id = ?
ORDER BY sort_order ASC;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(makeStorageError(stmtRes.error()));
        }

        if (const auto bindRes = stmtRes->bindText(1, groupId.value()); !bindRes)
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
INSERT INTO groups (id, name, created_at_ms, updated_at_ms)
VALUES (?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
    name = excluded.name,
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
        if (const auto bindRes = stmtRes->bindInt64(3, group.createdAtUnixEpochMs); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto bindRes = stmtRes->bindInt64(4, group.updatedAtUnixEpochMs); !bindRes)
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
        auto delStmtRes = sqlite::SqliteStatement::prepare(
            conn_, "DELETE FROM group_repositories WHERE group_id = ?;");
        if (!delStmtRes)
        {
            return std::unexpected(makeStorageError(delStmtRes.error()));
        }
        if (const auto bindRes = delStmtRes->bindText(1, group.id.value()); !bindRes)
        {
            return std::unexpected(makeStorageError(bindRes.error()));
        }
        if (const auto delStepRes = delStmtRes->step(); !delStepRes)
        {
            return std::unexpected(makeStorageError(delStepRes.error()));
        }

        if (!group.repositoryIds.empty())
        {
            auto insStmtRes = sqlite::SqliteStatement::prepare(
                conn_, "INSERT INTO group_repositories (group_id, repository_id, sort_order) VALUES (?, ?, ?);");
            if (!insStmtRes)
            {
                return std::unexpected(makeStorageError(insStmtRes.error()));
            }

            for (int i = 0; i < static_cast<int>(group.repositoryIds.size()); ++i)
            {
                insStmtRes->reset();
                insStmtRes->clearBindings();
                if (const auto bindRes = insStmtRes->bindText(1, group.id.value()); !bindRes)
                {
                    return std::unexpected(makeStorageError(bindRes.error()));
                }
                if (const auto bindRes = insStmtRes->bindText(2, group.repositoryIds[static_cast<std::size_t>(i)].value()); !bindRes)
                {
                    return std::unexpected(makeStorageError(bindRes.error()));
                }
                if (const auto bindRes = insStmtRes->bindInt(3, i); !bindRes)
                {
                    return std::unexpected(makeStorageError(bindRes.error()));
                }
                if (const auto insStep = insStmtRes->step(); !insStep)
                {
                    return std::unexpected(GroupStoreError{
                        .kind = GroupStoreError::Kind::StorageError,
                        .message = "Repository non presente nel catalogo o errore foreign key"
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

    std::expected<std::optional<Group>, GroupStoreError>
    SqliteGroupStore::getById(const GroupId& id)
    {
        constexpr std::string_view kSql = R"(
SELECT id, name, created_at_ms, updated_at_ms
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

        Group grp;
        grp.id = GroupId(stmtRes->getText(0));
        grp.name = stmtRes->getText(1);
        grp.createdAtUnixEpochMs = stmtRes->getInt64(2);
        grp.updatedAtUnixEpochMs = stmtRes->getInt64(3);

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
SELECT id, name, created_at_ms, updated_at_ms
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

            Group g;
            g.id = GroupId(stmtRes->getText(0));
            g.name = stmtRes->getText(1);
            g.createdAtUnixEpochMs = stmtRes->getInt64(2);
            g.updatedAtUnixEpochMs = stmtRes->getInt64(3);
            groups.push_back(std::move(g));
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
        constexpr std::string_view kSql = "DELETE FROM groups WHERE id = ?;";

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

    std::expected<std::vector<Group>, GroupStoreError>
    SqliteGroupStore::getGroupsContainingRepository(const RepositoryId& repoId)
    {
        constexpr std::string_view kSql = R"(
SELECT DISTINCT g.id, g.name, g.created_at_ms, g.updated_at_ms
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

            Group g;
            g.id = GroupId(stmtRes->getText(0));
            g.name = stmtRes->getText(1);
            g.createdAtUnixEpochMs = stmtRes->getInt64(2);
            g.updatedAtUnixEpochMs = stmtRes->getInt64(3);
            groups.push_back(std::move(g));
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
