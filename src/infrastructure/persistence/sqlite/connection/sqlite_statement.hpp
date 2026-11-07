#pragma once

#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

struct sqlite3_stmt;

namespace gf::infra::persistence::sqlite {

class SqliteStatement {
public:
    SqliteStatement() noexcept = default;
    ~SqliteStatement();

    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;

    SqliteStatement(SqliteStatement&& other) noexcept;
    SqliteStatement& operator=(SqliteStatement&& other) noexcept;

    [[nodiscard]] static std::expected<SqliteStatement, SqliteError>
    prepare(SqliteConnection& conn, std::string_view sql);

    [[nodiscard]] bool isValid() const noexcept { return stmt_ != nullptr; }

    [[nodiscard]] std::expected<void, SqliteError> bindText(int index, std::string_view text);
    [[nodiscard]] std::expected<void, SqliteError> bindInt64(int index, std::int64_t value);
    [[nodiscard]] std::expected<void, SqliteError> bindInt(int index, int value);
    [[nodiscard]] std::expected<void, SqliteError> bindDouble(int index, double value);
    [[nodiscard]] std::expected<void, SqliteError> bindNull(int index);

    [[nodiscard]] std::expected<bool, SqliteError> step();

    [[nodiscard]] bool isNull(int colIndex) const noexcept;
    [[nodiscard]] std::string getText(int colIndex) const;
    [[nodiscard]] std::int64_t getInt64(int colIndex) const noexcept;
    [[nodiscard]] int getInt(int colIndex) const noexcept;
    [[nodiscard]] double getDouble(int colIndex) const noexcept;

    void reset() noexcept;
    void clearBindings() noexcept;

private:
    explicit SqliteStatement(sqlite3_stmt* stmt) noexcept;

    sqlite3_stmt* stmt_ = nullptr;
};

} // namespace gf::infra::persistence::sqlite
