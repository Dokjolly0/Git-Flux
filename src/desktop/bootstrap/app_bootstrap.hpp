#pragma once

#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/repositories/sqlite_repository_store.hpp"
#include "infrastructure/persistence/groups/sqlite_group_store.hpp"
#include "infrastructure/persistence/workspaces/sqlite_workspace_store.hpp"
#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"
#include "desktop/viewmodels/home_hub_viewmodel.hpp"
#include "desktop/viewmodels/quick_open_viewmodel.hpp"

#include <filesystem>
#include <memory>

class QQmlApplicationEngine;

namespace gf::desktop {

class AppBootstrap {
public:
    explicit AppBootstrap(const std::filesystem::path& databasePath);
    ~AppBootstrap();

    AppBootstrap(const AppBootstrap&) = delete;
    AppBootstrap& operator=(const AppBootstrap&) = delete;
    AppBootstrap(AppBootstrap&&) = default;
    AppBootstrap& operator=(AppBootstrap&&) = default;

    void initialize();
    void registerWithQml(QQmlApplicationEngine& engine);

    [[nodiscard]] HomeHubViewModel& homeHubViewModel() noexcept;
    [[nodiscard]] QuickOpenViewModel& quickOpenViewModel() noexcept;
    [[nodiscard]] repositories::bl::RepositoryService& repositoryService() noexcept;
    [[nodiscard]] groups::bl::GroupService& groupService() noexcept;
    [[nodiscard]] workspaces::bl::WorkspaceService& workspaceService() noexcept;

    static std::filesystem::path defaultDatabasePath();

private:
    std::filesystem::path m_dbPath;
    std::unique_ptr<infra::persistence::sqlite::SqliteConnection> m_connection;
    std::unique_ptr<infra::persistence::repositories::SqliteRepositoryStore> m_repoStore;
    std::unique_ptr<infra::persistence::groups::SqliteGroupStore> m_groupStore;
    std::unique_ptr<infra::persistence::workspaces::SqliteWorkspaceStore> m_workspaceStore;

    std::unique_ptr<repositories::bl::RepositoryService> m_repoService;
    std::unique_ptr<groups::bl::GroupService> m_groupService;
    std::unique_ptr<workspaces::bl::WorkspaceService> m_workspaceService;

    std::unique_ptr<HomeHubViewModel> m_homeHubVm;
    std::unique_ptr<QuickOpenViewModel> m_quickOpenVm;
};

} // namespace gf::desktop
