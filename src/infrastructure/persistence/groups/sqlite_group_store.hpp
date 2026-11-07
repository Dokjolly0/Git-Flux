#pragma once

#include "core/modules/groups/ports/group_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"

namespace gf::infra::persistence::groups {

class SqliteGroupStore final : public gf::groups::ports::IGroupStore {
public:
    explicit SqliteGroupStore(sqlite::SqliteConnection& conn) noexcept;
    ~SqliteGroupStore() override = default;

    [[nodiscard]] std::expected<void, gf::groups::ports::GroupStoreError>
    save(const gf::groups::bo::Group& group) override;

    [[nodiscard]] std::expected<std::optional<gf::groups::bo::Group>, gf::groups::ports::GroupStoreError>
    getById(const gf::groups::bo::GroupId& id) override;

    [[nodiscard]] std::expected<std::vector<gf::groups::bo::Group>, gf::groups::ports::GroupStoreError>
    getAll() override;

    [[nodiscard]] std::expected<bool, gf::groups::ports::GroupStoreError>
    remove(const gf::groups::bo::GroupId& id) override;

    [[nodiscard]] std::expected<std::vector<gf::groups::bo::Group>, gf::groups::ports::GroupStoreError>
    getGroupsContainingRepository(const gf::repositories::bo::RepositoryId& repoId) override;

private:
    [[nodiscard]] std::expected<std::vector<gf::repositories::bo::RepositoryId>, gf::groups::ports::GroupStoreError>
    loadRepositoryIds(const gf::groups::bo::GroupId& groupId);

    sqlite::SqliteConnection& conn_;
};

} // namespace gf::infra::persistence::groups
