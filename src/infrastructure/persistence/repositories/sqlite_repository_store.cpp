#include "infrastructure/persistence/repositories/sqlite_repository_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_statement.hpp"
#include "core/shared/utf8_path.hpp"

#include <sqlite3.h>

namespace gf::infra::persistence::repositories
{
    using gf::repositories::bo::Repository;
    using gf::repositories::bo::RepositoryId;
    using gf::repositories::bo::RepositoryPathStatus;
    using gf::repositories::ports::RepositoryStoreError;

    namespace
    {
        Repository rowToRepository(sqlite::SqliteStatement& stmt)
        {
            Repository repo;
            repo.id = RepositoryId(stmt.getText(0));
            repo.displayName = stmt.getText(1);
            repo.localPath = gf::core::utf8ToPath(stmt.getText(2));
            repo.pathStatus = static_cast<RepositoryPathStatus>(stmt.getInt(3));
            repo.registeredAtUnixEpochMs = stmt.getInt64(4);
            repo.lastAccessedAtUnixEpochMs = stmt.getInt64(5);
            return repo;
        }
    } // namespace

    SqliteRepositoryStore::SqliteRepositoryStore(sqlite::SqliteConnection& conn) noexcept
        : conn_(conn)
    {
    }

    std::expected<void, RepositoryStoreError>
    SqliteRepositoryStore::save(const Repository& repo)
    {
        if (!repo.isValid())
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = "Repository non valida (ID, nome o percorso vuoti)"
            });
        }

        constexpr std::string_view kSql = R"(
INSERT INTO repositories (id, display_name, local_path, path_status, registered_at_ms, last_accessed_at_ms)
VALUES (?, ?, ?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
    display_name = excluded.display_name,
    local_path = excluded.local_path,
    path_status = excluded.path_status,
    last_accessed_at_ms = excluded.last_accessed_at_ms;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stmtRes.error().message
            });
        }

        (void)stmtRes->bindText(1, repo.id.value());
        (void)stmtRes->bindText(2, repo.displayName);
        (void)stmtRes->bindText(3, gf::core::pathToUtf8(repo.localPath));
        (void)stmtRes->bindInt(4, static_cast<int>(repo.pathStatus));
        (void)stmtRes->bindInt64(5, repo.registeredAtUnixEpochMs);
        (void)stmtRes->bindInt64(6, repo.lastAccessedAtUnixEpochMs);

        auto stepRes = stmtRes->step();
        if (!stepRes)
        {
            if (stepRes.error().code == SQLITE_CONSTRAINT_UNIQUE || stepRes.error().code == SQLITE_CONSTRAINT)
            {
                return std::unexpected(RepositoryStoreError{
                    .kind = RepositoryStoreError::Kind::DuplicatePath,
                    .message = "Percorso già associato a un altro repository"
                });
            }
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stepRes.error().message
            });
        }

        return {};
    }

    std::expected<std::optional<Repository>, RepositoryStoreError>
    SqliteRepositoryStore::getById(const RepositoryId& id)
    {
        constexpr std::string_view kSql = R"(
SELECT id, display_name, local_path, path_status, registered_at_ms, last_accessed_at_ms
FROM repositories
WHERE id = ?;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stmtRes.error().message
            });
        }

        (void)stmtRes->bindText(1, id.value());
        auto stepRes = stmtRes->step();
        if (!stepRes)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stepRes.error().message
            });
        }

        if (*stepRes)
        {
            return rowToRepository(*stmtRes);
        }
        return std::nullopt;
    }

    std::expected<std::optional<Repository>, RepositoryStoreError>
    SqliteRepositoryStore::getByPath(const std::filesystem::path& path)
    {
        constexpr std::string_view kSql = R"(
SELECT id, display_name, local_path, path_status, registered_at_ms, last_accessed_at_ms
FROM repositories
WHERE local_path = ?;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stmtRes.error().message
            });
        }

        (void)stmtRes->bindText(1, gf::core::pathToUtf8(path));
        auto stepRes = stmtRes->step();
        if (!stepRes)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stepRes.error().message
            });
        }

        if (*stepRes)
        {
            return rowToRepository(*stmtRes);
        }
        return std::nullopt;
    }

    std::expected<std::vector<Repository>, RepositoryStoreError>
    SqliteRepositoryStore::getAll()
    {
        constexpr std::string_view kSql = R"(
SELECT id, display_name, local_path, path_status, registered_at_ms, last_accessed_at_ms
FROM repositories
ORDER BY display_name COLLATE NOCASE ASC;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stmtRes.error().message
            });
        }

        std::vector<Repository> results;
        while (true)
        {
            auto stepRes = stmtRes->step();
            if (!stepRes)
            {
                return std::unexpected(RepositoryStoreError{
                    .kind = RepositoryStoreError::Kind::StorageError,
                    .message = stepRes.error().message
                });
            }
            if (!*stepRes)
            {
                break;
            }
            results.push_back(rowToRepository(*stmtRes));
        }

        return results;
    }

    std::expected<bool, RepositoryStoreError>
    SqliteRepositoryStore::remove(const RepositoryId& id)
    {
        constexpr std::string_view kSql = "DELETE FROM repositories WHERE id = ?;";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stmtRes.error().message
            });
        }

        (void)stmtRes->bindText(1, id.value());
        auto stepRes = stmtRes->step();
        if (!stepRes)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stepRes.error().message
            });
        }

        return conn_.changes() > 0;
    }

    std::expected<void, RepositoryStoreError>
    SqliteRepositoryStore::updatePath(const RepositoryId& id, const std::filesystem::path& newPath)
    {
        constexpr std::string_view kSql = R"(
UPDATE repositories
SET local_path = ?, path_status = ?
WHERE id = ?;
)";

        auto stmtRes = sqlite::SqliteStatement::prepare(conn_, kSql);
        if (!stmtRes)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stmtRes.error().message
            });
        }

        (void)stmtRes->bindText(1, gf::core::pathToUtf8(newPath));
        (void)stmtRes->bindInt(2, static_cast<int>(RepositoryPathStatus::Valid));
        (void)stmtRes->bindText(3, id.value());

        auto stepRes = stmtRes->step();
        if (!stepRes)
        {
            if (stepRes.error().code == SQLITE_CONSTRAINT_UNIQUE || stepRes.error().code == SQLITE_CONSTRAINT)
            {
                return std::unexpected(RepositoryStoreError{
                    .kind = RepositoryStoreError::Kind::DuplicatePath,
                    .message = "Il nuovo percorso è già utilizzato da un altro repository"
                });
            }
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::StorageError,
                .message = stepRes.error().message
            });
        }

        if (conn_.changes() == 0)
        {
            return std::unexpected(RepositoryStoreError{
                .kind = RepositoryStoreError::Kind::NotFound,
                .message = "Repository non trovato per aggiornamento percorso"
            });
        }

        return {};
    }
} // namespace gf::infra::persistence::repositories
