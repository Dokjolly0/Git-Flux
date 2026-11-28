#pragma once

#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

struct sqlite3_stmt;

namespace gf::infra::persistence::sqlite
{
    class SqliteStatement
    {
    public:
        SqliteStatement() noexcept = default;
        ~SqliteStatement();

        SqliteStatement(const SqliteStatement&) = delete;
        SqliteStatement& operator=(const SqliteStatement&) = delete;

        SqliteStatement(SqliteStatement&& other) noexcept;
        SqliteStatement& operator=(SqliteStatement&& other) noexcept;

        [[nodiscard]] static std::expected<SqliteStatement, SqliteError>
        prepare(const SqliteConnection& conn, std::string_view sql);

        [[nodiscard]] std::expected<void, SqliteError> bindText(int index, std::string_view text) const;
        [[nodiscard]] std::expected<void, SqliteError> bindInt64(int index, std::int64_t value) const;
        [[nodiscard]] std::expected<void, SqliteError> bindInt(int index, int value) const;
        [[nodiscard]] std::expected<void, SqliteError> bindDouble(int index, double value) const;
        [[nodiscard]] std::expected<void, SqliteError> bindNull(int index) const;

        [[nodiscard]] std::expected<bool, SqliteError> step() const;

        [[nodiscard]] bool isNull(int colIndex) const noexcept;
        [[nodiscard]] std::string getText(int colIndex) const;
        [[nodiscard]] std::int64_t getInt64(int colIndex) const noexcept;
        [[nodiscard]] int getInt(int colIndex) const noexcept;
        [[nodiscard]] double getDouble(int colIndex) const noexcept;

        void reset() const noexcept;
        void clearBindings() const noexcept;

    private:
        explicit SqliteStatement(sqlite3_stmt* stmt) noexcept;

        sqlite3_stmt* stmt_ = nullptr;
    };
} // namespace gf::infra::persistence::sqlite
