#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

struct sqlite3;

namespace gf::infra::persistence::sqlite
{
    struct SqliteError
    {
        int code = 0;
        std::string message;
    };

    class SqliteConnection
    {
    public:
        SqliteConnection() noexcept = default;
        ~SqliteConnection();

        SqliteConnection(const SqliteConnection&) = delete;
        SqliteConnection& operator=(const SqliteConnection&) = delete;

        SqliteConnection(SqliteConnection&& other) noexcept;
        SqliteConnection& operator=(SqliteConnection&& other) noexcept;

        [[nodiscard]] static std::expected<SqliteConnection, SqliteError>
        open(const std::filesystem::path& dbPath);

        [[nodiscard]] static std::expected<SqliteConnection, SqliteError>
        openInMemory();

        [[nodiscard]] bool isOpen() const noexcept;
        void close() noexcept;

        [[nodiscard]] sqlite3* rawHandle() const noexcept { return db_; }

        [[nodiscard]] std::expected<void, SqliteError>
        execute(std::string_view sql);

        [[nodiscard]] std::int64_t lastInsertRowId() const noexcept;
        [[nodiscard]] int changes() const noexcept;

    private:
        explicit SqliteConnection(sqlite3* db) noexcept;

        sqlite3* db_ = nullptr;
    };

    class SqliteTransaction
    {
    public:
        [[nodiscard]] static std::expected<SqliteTransaction, SqliteError>
        begin(SqliteConnection& conn, bool immediate = true);

        ~SqliteTransaction();

        SqliteTransaction(const SqliteTransaction&) = delete;
        SqliteTransaction& operator=(const SqliteTransaction&) = delete;

        SqliteTransaction(SqliteTransaction&& other) noexcept;
        SqliteTransaction& operator=(SqliteTransaction&& other) noexcept;

        [[nodiscard]] std::expected<void, SqliteError> commit();
        void rollback() noexcept;

        [[nodiscard]] bool isCommitted() const noexcept { return committed_; }

    private:
        explicit SqliteTransaction(SqliteConnection* conn) noexcept;

        SqliteConnection* conn_ = nullptr;
        bool committed_ = false;
    };
} // namespace gf::infra::persistence::sqlite
