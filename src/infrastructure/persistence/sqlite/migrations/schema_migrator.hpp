#pragma once

#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"

#include <expected>

namespace gf::infra::persistence::sqlite {

class SchemaMigrator {
public:
    static constexpr int kTargetSchemaVersion = 1;

    [[nodiscard]] static std::expected<int, SqliteError>
    getCurrentVersion(SqliteConnection& conn);

    [[nodiscard]] static std::expected<void, SqliteError>
    migrateToLatest(SqliteConnection& conn);
};

} // namespace gf::infra::persistence::sqlite
