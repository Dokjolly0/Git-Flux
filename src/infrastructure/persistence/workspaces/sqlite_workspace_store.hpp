#pragma once

#include "core/modules/workspaces/ports/workspace_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"

namespace gf::infra::persistence::workspaces
{
    class SqliteWorkspaceStore final : public gf::workspaces::ports::IWorkspaceStore
    {
    public:
        explicit SqliteWorkspaceStore(sqlite::SqliteConnection& conn) noexcept;
        ~SqliteWorkspaceStore() override = default;

        [[nodiscard]] std::expected<void, gf::workspaces::ports::WorkspaceStoreError>
        save(const gf::workspaces::bo::Workspace& workspace) override;

        [[nodiscard]] std::expected<std::optional<gf::workspaces::bo::Workspace>,
                                    gf::workspaces::ports::WorkspaceStoreError>
        getById(const gf::workspaces::bo::WorkspaceId& id) override;

        [[nodiscard]] std::expected<std::vector<gf::workspaces::bo::Workspace>,
                                    gf::workspaces::ports::WorkspaceStoreError>
        getAll() override;

        [[nodiscard]] std::expected<bool, gf::workspaces::ports::WorkspaceStoreError>
        remove(const gf::workspaces::bo::WorkspaceId& id) override;

        [[nodiscard]] std::expected<void, gf::workspaces::ports::WorkspaceStoreError>
        saveSession(const gf::workspaces::bo::WorkspaceSession& session) override;

        [[nodiscard]] std::expected<std::optional<gf::workspaces::bo::WorkspaceSession>,
                                    gf::workspaces::ports::WorkspaceStoreError>
        getSession(const gf::workspaces::bo::WorkspaceId& id) override;

    private:
        [[nodiscard]] std::expected<std::vector<gf::groups::bo::GroupId>, gf::workspaces::ports::WorkspaceStoreError>
        loadWorkspaceGroupIds(const gf::workspaces::bo::WorkspaceId& wsId);

        [[nodiscard]] std::expected<std::vector<gf::repositories::bo::RepositoryId>,
                                    gf::workspaces::ports::WorkspaceStoreError>
        loadWorkspaceDirectRepoIds(const gf::workspaces::bo::WorkspaceId& wsId);

        sqlite::SqliteConnection& conn_;
    };
} // namespace gf::infra::persistence::workspaces
