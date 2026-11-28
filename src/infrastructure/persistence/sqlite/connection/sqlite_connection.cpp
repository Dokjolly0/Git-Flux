#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "core/shared/utf8_path.hpp"

#include <sqlite3.h>

#include <utility>

namespace gf::infra::persistence::sqlite
{
    SqliteConnection::SqliteConnection(sqlite3* db) noexcept
        : db_(db)
    {
    }

    SqliteConnection::~SqliteConnection()
    {
        close();
    }

    SqliteConnection::SqliteConnection(SqliteConnection&& other) noexcept
        : db_(std::exchange(other.db_, nullptr))
    {
    }

    SqliteConnection& SqliteConnection::operator=(SqliteConnection&& other) noexcept
    {
        if (this != &other)
        {
            close();
            db_ = std::exchange(other.db_, nullptr);
        }
        return *this;
    }

    std::expected<SqliteConnection, SqliteError>
    SqliteConnection::open(const std::filesystem::path& dbPath)
    {
        sqlite3* db = nullptr;
        const std::string pathUtf8 = core::pathToUtf8(dbPath);

        constexpr int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
        if (const int rc = sqlite3_open_v2(pathUtf8.c_str(), &db, flags, nullptr); rc != SQLITE_OK)
        {
            std::string err = db ? sqlite3_errmsg(db) : "Apertura database SQLite fallita";
            if (db)
            {
                sqlite3_close_v2(db);
            }
            return std::unexpected(SqliteError{
                .code = rc,
                .message = std::move(err)
            });
        }

        SqliteConnection conn(db);

        // Configure busy timeout to 5000ms
        sqlite3_busy_timeout(db, 5000);

        // Enable foreign key constraints (required in SQLite)
        if (const auto fkRes = conn.execute("PRAGMA foreign_keys = ON;"); !fkRes)
        {
            return std::unexpected(fkRes.error());
        }

        // For on-disk databases, enable WAL mode and NORMAL synchronous for maximum performance and durability
        if (pathUtf8 != ":memory:")
        {
            if (const auto walRes = conn.execute("PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;"); !walRes)
            {
                return std::unexpected(walRes.error());
            }
        }

        return conn;
    }

    std::expected<SqliteConnection, SqliteError>
    SqliteConnection::openInMemory()
    {
        return open(":memory:");
    }

    bool SqliteConnection::isOpen() const noexcept
    {
        return db_ != nullptr;
    }

    void SqliteConnection::close() noexcept
    {
        if (db_)
        {
            sqlite3_close_v2(db_);
            db_ = nullptr;
        }
    }

    std::expected<void, SqliteError>
    SqliteConnection::execute(const std::string_view sql) const
    {
        if (!db_)
        {
            return std::unexpected(SqliteError{
                .code = SQLITE_MISUSE,
                .message = "Connessione al database non aperta"
            });
        }

        char* errMsg = nullptr;
        const std::string sqlStr(sql);
        if (const int rc = sqlite3_exec(db_, sqlStr.c_str(), nullptr, nullptr, &errMsg); rc != SQLITE_OK)
        {
            std::string msg = errMsg ? errMsg : sqlite3_errmsg(db_);
            if (errMsg)
            {
                sqlite3_free(errMsg);
            }
            return std::unexpected(SqliteError{
                .code = rc,
                .message = std::move(msg)
            });
        }

        return {};
    }

    int SqliteConnection::changes() const noexcept
    {
        return db_ ? sqlite3_changes(db_) : 0;
    }

    // -----------------------------------------------------------------------------
    // SqliteTransaction
    // -----------------------------------------------------------------------------

    SqliteTransaction::SqliteTransaction(SqliteConnection* conn) noexcept
        : conn_(conn)
    {
    }

    std::expected<SqliteTransaction, SqliteError>
    SqliteTransaction::begin(SqliteConnection& conn, const bool immediate)
    {
        if (const auto res = conn.execute(immediate ? "BEGIN IMMEDIATE TRANSACTION;" : "BEGIN TRANSACTION;"); !res)
        {
            return std::unexpected(res.error());
        }
        return SqliteTransaction(&conn);
    }

    SqliteTransaction::~SqliteTransaction()
    {
        rollback();
    }

    SqliteTransaction::SqliteTransaction(SqliteTransaction&& other) noexcept
        : conn_(std::exchange(other.conn_, nullptr))
          , committed_(std::exchange(other.committed_, false))
    {
    }

    SqliteTransaction& SqliteTransaction::operator=(SqliteTransaction&& other) noexcept
    {
        if (this != &other)
        {
            rollback();
            conn_ = std::exchange(other.conn_, nullptr);
            committed_ = std::exchange(other.committed_, false);
        }
        return *this;
    }

    std::expected<void, SqliteError> SqliteTransaction::commit()
    {
        if (!conn_ || committed_)
        {
            return {};
        }
        auto res = conn_->execute("COMMIT;");
        if (res)
        {
            committed_ = true;
        }
        return res;
    }

    void SqliteTransaction::rollback() noexcept
    {
        if (conn_ && !committed_)
        {
            if (conn_->isOpen())
            {
                sqlite3_exec(conn_->rawHandle(), "ROLLBACK;", nullptr, nullptr, nullptr);
            }
            committed_ = true;
        }
    }
} // namespace gf::infra::persistence::sqlite
