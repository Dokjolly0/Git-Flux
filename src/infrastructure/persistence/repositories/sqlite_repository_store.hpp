#pragma once

#include "core/modules/repositories/ports/repository_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"

namespace gf::infra::persistence::repositories
{
    class SqliteRepositoryStore final : public gf::repositories::ports::IRepositoryStore
    {
    public:
        explicit SqliteRepositoryStore(sqlite::SqliteConnection& conn) noexcept;
        ~SqliteRepositoryStore() override = default;

        [[nodiscard]] std::expected<void, gf::repositories::ports::RepositoryStoreError>
        save(const gf::repositories::bo::Repository& repository) override;

        [[nodiscard]] std::expected<std::optional<gf::repositories::bo::Repository>,
                                    gf::repositories::ports::RepositoryStoreError>
        getById(const gf::repositories::bo::RepositoryId& id) override;

        [[nodiscard]] std::expected<std::optional<gf::repositories::bo::Repository>,
                                    gf::repositories::ports::RepositoryStoreError>
        getByPath(const std::filesystem::path& path) override;

        [[nodiscard]] std::expected<std::vector<gf::repositories::bo::Repository>,
                                    gf::repositories::ports::RepositoryStoreError>
        getAll() override;

        [[nodiscard]] std::expected<bool, gf::repositories::ports::RepositoryStoreError>
        remove(const gf::repositories::bo::RepositoryId& id) override;

        [[nodiscard]] std::expected<void, gf::repositories::ports::RepositoryStoreError>
        updatePath(const gf::repositories::bo::RepositoryId& id, const std::filesystem::path& newPath) override;

    private:
        sqlite::SqliteConnection& conn_;
    };
} // namespace gf::infra::persistence::repositories
