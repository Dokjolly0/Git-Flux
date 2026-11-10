#include "infrastructure/persistence/sqlite/connection/sqlite_statement.hpp"

#include <sqlite3.h>

#include <utility>

namespace gf::infra::persistence::sqlite
{
    SqliteStatement::SqliteStatement(sqlite3_stmt* stmt) noexcept
        : stmt_(stmt)
    {
    }

    SqliteStatement::~SqliteStatement()
    {
        if (stmt_)
        {
            sqlite3_finalize(stmt_);
            stmt_ = nullptr;
        }
    }

    SqliteStatement::SqliteStatement(SqliteStatement&& other) noexcept
        : stmt_(std::exchange(other.stmt_, nullptr))
    {
    }

    SqliteStatement& SqliteStatement::operator=(SqliteStatement&& other) noexcept
    {
        if (this != &other)
        {
            if (stmt_)
            {
                sqlite3_finalize(stmt_);
            }
            stmt_ = std::exchange(other.stmt_, nullptr);
        }
        return *this;
    }

    std::expected<SqliteStatement, SqliteError>
    SqliteStatement::prepare(SqliteConnection& conn, std::string_view sql)
    {
        if (!conn.isOpen())
        {
            return std::unexpected(SqliteError{
                .code = SQLITE_MISUSE,
                .message = "Impossibile preparare statement: connessione chiusa"
            });
        }

        sqlite3_stmt* stmt = nullptr;
        const int rc = sqlite3_prepare_v2(
            conn.rawHandle(),
            sql.data(),
            static_cast<int>(sql.size()),
            &stmt,
            nullptr);

        if (rc != SQLITE_OK)
        {
            std::string err = sqlite3_errmsg(conn.rawHandle());
            if (stmt)
            {
                sqlite3_finalize(stmt);
            }
            return std::unexpected(SqliteError{
                .code = rc,
                .message = std::move(err)
            });
        }

        return SqliteStatement(stmt);
    }

    std::expected<void, SqliteError>
    SqliteStatement::bindText(int index, std::string_view text)
    {
        const int rc = sqlite3_bind_text(
            stmt_,
            index,
            text.data(),
            static_cast<int>(text.size()),
            SQLITE_TRANSIENT);
        if (rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindText"
            });
        }
        return {};
    }

    std::expected<void, SqliteError>
    SqliteStatement::bindInt64(int index, std::int64_t value)
    {
        const int rc = sqlite3_bind_int64(stmt_, index, static_cast<sqlite3_int64>(value));
        if (rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindInt64"
            });
        }
        return {};
    }

    std::expected<void, SqliteError>
    SqliteStatement::bindInt(int index, int value)
    {
        const int rc = sqlite3_bind_int(stmt_, index, value);
        if (rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindInt"
            });
        }
        return {};
    }

    std::expected<void, SqliteError>
    SqliteStatement::bindDouble(int index, double value)
    {
        const int rc = sqlite3_bind_double(stmt_, index, value);
        if (rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindDouble"
            });
        }
        return {};
    }

    std::expected<void, SqliteError>
    SqliteStatement::bindNull(int index)
    {
        const int rc = sqlite3_bind_null(stmt_, index);
        if (rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindNull"
            });
        }
        return {};
    }

    std::expected<bool, SqliteError>
    SqliteStatement::step()
    {
        const int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW)
        {
            return true;
        }
        if (rc == SQLITE_DONE)
        {
            return false;
        }
        return std::unexpected(SqliteError{
            .code = rc,
            .message = "Fallito sqlite3_step"
        });
    }

    bool SqliteStatement::isNull(int colIndex) const noexcept
    {
        return sqlite3_column_type(stmt_, colIndex) == SQLITE_NULL;
    }

    std::string SqliteStatement::getText(int colIndex) const
    {
        const auto textPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, colIndex));
        const int bytes = sqlite3_column_bytes(stmt_, colIndex);
        if (!textPtr || bytes <= 0)
        {
            return {};
        }
        return std::string(textPtr, static_cast<std::size_t>(bytes));
    }

    std::int64_t SqliteStatement::getInt64(int colIndex) const noexcept
    {
        return static_cast<std::int64_t>(sqlite3_column_int64(stmt_, colIndex));
    }

    int SqliteStatement::getInt(int colIndex) const noexcept
    {
        return sqlite3_column_int(stmt_, colIndex);
    }

    double SqliteStatement::getDouble(int colIndex) const noexcept
    {
        return sqlite3_column_double(stmt_, colIndex);
    }

    void SqliteStatement::reset() noexcept
    {
        if (stmt_)
        {
            sqlite3_reset(stmt_);
        }
    }

    void SqliteStatement::clearBindings() noexcept
    {
        if (stmt_)
        {
            sqlite3_clear_bindings(stmt_);
        }
    }
} // namespace gf::infra::persistence::sqlite
