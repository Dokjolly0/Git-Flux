#pragma once

#include "core/modules/git/bo/git_provider_profile.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include <optional>
#include <vector>
#include <string>

namespace gf::infra::persistence::profiles
{
    class SqliteProfileStore
    {
    public:
        explicit SqliteProfileStore(sqlite::SqliteConnection& conn) noexcept;

        [[nodiscard]] std::expected<void, sqlite::SqliteError>
        saveProfile(const core::git::bo::GitProviderProfile& profile);

        [[nodiscard]] std::expected<std::optional<core::git::bo::GitProviderProfile>, sqlite::SqliteError>
        getProfile(const std::string& id);

        [[nodiscard]] std::expected<std::vector<core::git::bo::GitProviderProfile>, sqlite::SqliteError>
        getAllProfiles();

        [[nodiscard]] std::expected<bool, sqlite::SqliteError>
        deleteProfile(const std::string& id);

    private:
        sqlite::SqliteConnection& m_conn;
    };
} // namespace gf::infra::persistence::profiles
