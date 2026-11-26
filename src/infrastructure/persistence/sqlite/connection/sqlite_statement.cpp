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
    SqliteStatement::prepare(const SqliteConnection& conn, const std::string_view sql)
    {
        if (!conn.isOpen())
        {
            return std::unexpected(SqliteError{
                .code = SQLITE_MISUSE,
                .message = "Impossibile preparare statement: connessione chiusa"
            });
        }

        sqlite3_stmt* stmt = nullptr;
        if (const int rc = sqlite3_prepare_v2(
                conn.rawHandle(),
                sql.data(),
                static_cast<int>(sql.size()),
                &stmt,
                nullptr);
            rc != SQLITE_OK)
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
    SqliteStatement::bindText(const int index, const std::string_view text) const
    {
        if (const int rc = sqlite3_bind_text(
                stmt_,
                index,
                text.data(),
                static_cast<int>(text.size()),
                SQLITE_TRANSIENT);
            rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindText"
            });
        }
        return {};
    }

    std::expected<void, SqliteError>
    SqliteStatement::bindInt64(const int index, const std::int64_t value) const
    {
        if (const int rc = sqlite3_bind_int64(stmt_, index, value); rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindInt64"
            });
        }
        return {};
    }

    std::expected<void, SqliteError>
    SqliteStatement::bindInt(const int index, const int value) const
    {
        if (const int rc = sqlite3_bind_int(stmt_, index, value); rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindInt"
            });
        }
        return {};
    }

    std::expected<void, SqliteError>
    SqliteStatement::bindDouble(const int index, const double value) const
    {
        if (const int rc = sqlite3_bind_double(stmt_, index, value); rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindDouble"
            });
        }
        return {};
    }

    std::expected<void, SqliteError>
    SqliteStatement::bindNull(const int index) const
    {
        if (const int rc = sqlite3_bind_null(stmt_, index); rc != SQLITE_OK)
        {
            return std::unexpected(SqliteError{
                .code = rc,
                .message = "Fallito bindNull"
            });
        }
        return {};
    }

    std::expected<bool, SqliteError>
    SqliteStatement::step() const
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

    bool SqliteStatement::isNull(const int colIndex) const noexcept
    {
        return sqlite3_column_type(stmt_, colIndex) == SQLITE_NULL;
    }

    std::string SqliteStatement::getText(const int colIndex) const
    {
        const auto textPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, colIndex));
        const int bytes = sqlite3_column_bytes(stmt_, colIndex);
        if (!textPtr || bytes <= 0)
        {
            return {};
        }
        return std::string(textPtr, static_cast<std::size_t>(bytes));
    }

    std::int64_t SqliteStatement::getInt64(const int colIndex) const noexcept
    {
        return sqlite3_column_int64(stmt_, colIndex);
    }

    int SqliteStatement::getInt(const int colIndex) const noexcept
    {
        return sqlite3_column_int(stmt_, colIndex);
    }

    double SqliteStatement::getDouble(const int colIndex) const noexcept
    {
        return sqlite3_column_double(stmt_, colIndex);
    }

    void SqliteStatement::reset() const noexcept
    {
        if (stmt_)
        {
            sqlite3_reset(stmt_);
        }
    }

    void SqliteStatement::clearBindings() const noexcept
    {
        if (stmt_)
        {
            sqlite3_clear_bindings(stmt_);
        }
    }
} // namespace gf::infra::persistence::sqlite
