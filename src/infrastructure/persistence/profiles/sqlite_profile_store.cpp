#include "infrastructure/persistence/profiles/sqlite_profile_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_statement.hpp"
#include <chrono>

namespace gf::infra::persistence::profiles
{
    namespace
    {
        constexpr std::string_view kUpsertSql = R"(
INSERT INTO git_provider_profiles (
    id, provider_type, display_name, host, username,
    encrypted_token, token_salt, token_iv, token_tag,
    preferred_protocol, custom_api_url, created_at_ms, updated_at_ms
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
    provider_type = excluded.provider_type,
    display_name = excluded.display_name,
    host = excluded.host,
    username = excluded.username,
    encrypted_token = excluded.encrypted_token,
    token_salt = excluded.token_salt,
    token_iv = excluded.token_iv,
    token_tag = excluded.token_tag,
    preferred_protocol = excluded.preferred_protocol,
    custom_api_url = excluded.custom_api_url,
    updated_at_ms = excluded.updated_at_ms;
)";

        constexpr std::string_view kSelectByIdSql = R"(
SELECT id, provider_type, display_name, host, username,
       encrypted_token, token_salt, token_iv, token_tag,
       preferred_protocol, custom_api_url, created_at_ms, updated_at_ms
FROM git_provider_profiles
WHERE id = ?;
)";

        constexpr std::string_view kSelectAllSql = R"(
SELECT id, provider_type, display_name, host, username,
       encrypted_token, token_salt, token_iv, token_tag,
       preferred_protocol, custom_api_url, created_at_ms, updated_at_ms
FROM git_provider_profiles
ORDER BY created_at_ms ASC;
)";

        constexpr std::string_view kDeleteSql = R"(
DELETE FROM git_provider_profiles WHERE id = ?;
)";

        core::git::bo::GitProviderProfile mapProfileRow(sqlite::SqliteStatement& stmt)
        {
            core::git::bo::GitProviderProfile p;
            p.id = stmt.getText(0);
            p.providerType = core::git::bo::gitProviderTypeFromString(stmt.getText(1));
            p.displayName = stmt.getText(2);
            p.host = stmt.getText(3);
            p.username = stmt.getText(4);
            p.tokenSecret.ciphertextHex = stmt.getText(5);
            p.tokenSecret.saltHex = stmt.getText(6);
            p.tokenSecret.ivHex = stmt.getText(7);
            p.tokenSecret.tagHex = stmt.getText(8);
            p.hasToken = !p.tokenSecret.empty();
            p.preferredProtocol = stmt.getText(9);
            p.customApiUrl = stmt.getText(10);
            p.createdAtMs = stmt.getInt64(11);
            p.updatedAtMs = stmt.getInt64(12);
            return p;
        }
    } // namespace

    SqliteProfileStore::SqliteProfileStore(sqlite::SqliteConnection& conn) noexcept
        : m_conn(conn)
    {
    }

    std::expected<void, sqlite::SqliteError>
    SqliteProfileStore::saveProfile(const core::git::bo::GitProviderProfile& profile)
    {
        auto stmtRes = sqlite::SqliteStatement::prepare(m_conn, kUpsertSql);
        if (!stmtRes) return std::unexpected(stmtRes.error());

        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const auto createdAt = profile.createdAtMs > 0 ? profile.createdAtMs : now;

        if (auto b = stmtRes->bindText(1, profile.id); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindText(2, core::git::bo::gitProviderTypeToString(profile.providerType)); !b) return
            std::unexpected(b.error());
        if (auto b = stmtRes->bindText(3, profile.displayName); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindText(4, profile.host); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindText(5, profile.username); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindText(6, profile.tokenSecret.ciphertextHex); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindText(7, profile.tokenSecret.saltHex); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindText(8, profile.tokenSecret.ivHex); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindText(9, profile.tokenSecret.tagHex); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindText(10, profile.preferredProtocol.empty() ? "ssh" : profile.preferredProtocol); !b)
            return std::unexpected(b.error());
        if (auto b = stmtRes->bindText(11, profile.customApiUrl); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindInt64(12, createdAt); !b) return std::unexpected(b.error());
        if (auto b = stmtRes->bindInt64(13, now); !b) return std::unexpected(b.error());

        auto stepRes = stmtRes->step();
        if (!stepRes) return std::unexpected(stepRes.error());
        return {};
    }

    std::expected<std::optional<core::git::bo::GitProviderProfile>, sqlite::SqliteError>
    SqliteProfileStore::getProfile(const std::string& id)
    {
        auto stmtRes = sqlite::SqliteStatement::prepare(m_conn, kSelectByIdSql);
        if (!stmtRes) return std::unexpected(stmtRes.error());

        if (auto b = stmtRes->bindText(1, id); !b) return std::unexpected(b.error());

        auto stepRes = stmtRes->step();
        if (!stepRes) return std::unexpected(stepRes.error());
        if (!*stepRes) return std::nullopt;

        return mapProfileRow(*stmtRes);
    }

    std::expected<std::vector<core::git::bo::GitProviderProfile>, sqlite::SqliteError>
    SqliteProfileStore::getAllProfiles()
    {
        auto stmtRes = sqlite::SqliteStatement::prepare(m_conn, kSelectAllSql);
        if (!stmtRes) return std::unexpected(stmtRes.error());

        std::vector<core::git::bo::GitProviderProfile> profiles;
        while (true)
        {
            auto stepRes = stmtRes->step();
            if (!stepRes) return std::unexpected(stepRes.error());
            if (!*stepRes) break;
            profiles.push_back(mapProfileRow(*stmtRes));
        }
        return profiles;
    }

    std::expected<bool, sqlite::SqliteError>
    SqliteProfileStore::deleteProfile(const std::string& id)
    {
        auto stmtRes = sqlite::SqliteStatement::prepare(m_conn, kDeleteSql);
        if (!stmtRes) return std::unexpected(stmtRes.error());

        if (auto b = stmtRes->bindText(1, id); !b) return std::unexpected(b.error());
        auto stepRes = stmtRes->step();
        if (!stepRes) return std::unexpected(stepRes.error());
        return m_conn.changes() > 0;
    }
} // namespace gf::infra::persistence::profiles
