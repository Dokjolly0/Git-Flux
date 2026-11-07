#include "desktop/bootstrap/app_bootstrap.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QDir>
#include <stdexcept>

namespace gf::desktop {

AppBootstrap::AppBootstrap(const std::filesystem::path& databasePath)
    : m_dbPath(databasePath) {}

AppBootstrap::~AppBootstrap() = default;

void AppBootstrap::initialize() {
    if (m_dbPath == ":memory:") {
        auto connResult = infra::persistence::sqlite::SqliteConnection::openInMemory();
        if (!connResult) {
            throw std::runtime_error("Impossibile aprire database SQLite in memoria: " + connResult.error().message);
        }
        m_connection = std::make_unique<infra::persistence::sqlite::SqliteConnection>(std::move(*connResult));
    } else {
        if (!m_dbPath.parent_path().empty()) {
            std::filesystem::create_directories(m_dbPath.parent_path());
        }
        auto connResult = infra::persistence::sqlite::SqliteConnection::open(m_dbPath);
        if (!connResult) {
            throw std::runtime_error("Impossibile aprire database SQLite su disco: " + connResult.error().message);
        }
        m_connection = std::make_unique<infra::persistence::sqlite::SqliteConnection>(std::move(*connResult));
    }

    auto migrationResult = infra::persistence::sqlite::SchemaMigrator::migrateToLatest(*m_connection);
    if (!migrationResult) {
        throw std::runtime_error("Impossibile eseguire le migrazioni del database: " + migrationResult.error().message);
    }

    m_repoStore = std::make_unique<infra::persistence::repositories::SqliteRepositoryStore>(*m_connection);
    m_groupStore = std::make_unique<infra::persistence::groups::SqliteGroupStore>(*m_connection);
    m_workspaceStore = std::make_unique<infra::persistence::workspaces::SqliteWorkspaceStore>(*m_connection);

    m_repoService = std::make_unique<repositories::bl::RepositoryService>(*m_repoStore);
    m_groupService = std::make_unique<groups::bl::GroupService>(*m_groupStore, *m_repoStore);
    m_workspaceService = std::make_unique<workspaces::bl::WorkspaceService>(*m_workspaceStore, *m_groupStore, *m_repoStore);

    m_homeHubVm = std::make_unique<HomeHubViewModel>(*m_repoService, *m_groupService, *m_workspaceService);
    m_quickOpenVm = std::make_unique<QuickOpenViewModel>(*m_groupService, *m_repoService);
}

void AppBootstrap::registerWithQml(QQmlApplicationEngine& engine) {
    qmlRegisterUncreatableType<RepositoryListModel>("GitFlux.Desktop", 1, 0, "RepositoryListModel",
        "Modello fornito da HomeHubViewModel");
    qmlRegisterUncreatableType<GroupListModel>("GitFlux.Desktop", 1, 0, "GroupListModel",
        "Modello fornito da HomeHubViewModel");
    qmlRegisterUncreatableType<WorkspaceListModel>("GitFlux.Desktop", 1, 0, "WorkspaceListModel",
        "Modello fornito da HomeHubViewModel");
    qmlRegisterUncreatableType<HomeHubViewModel>("GitFlux.Desktop", 1, 0, "HomeHubViewModel",
        "ViewModel gestito dal bootstrap");
    qmlRegisterUncreatableType<QuickOpenViewModel>("GitFlux.Desktop", 1, 0, "QuickOpenViewModel",
        "ViewModel gestito dal bootstrap");

    auto* rootCtx = engine.rootContext();
    rootCtx->setContextProperty(QStringLiteral("homeHubVm"), m_homeHubVm.get());
    rootCtx->setContextProperty(QStringLiteral("quickOpenVm"), m_quickOpenVm.get());
}

HomeHubViewModel& AppBootstrap::homeHubViewModel() noexcept {
    return *m_homeHubVm;
}

QuickOpenViewModel& AppBootstrap::quickOpenViewModel() noexcept {
    return *m_quickOpenVm;
}

repositories::bl::RepositoryService& AppBootstrap::repositoryService() noexcept {
    return *m_repoService;
}

groups::bl::GroupService& AppBootstrap::groupService() noexcept {
    return *m_groupService;
}

workspaces::bl::WorkspaceService& AppBootstrap::workspaceService() noexcept {
    return *m_workspaceService;
}

std::filesystem::path AppBootstrap::defaultDatabasePath() {
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!appData.isEmpty()) {
        const QDir dir(appData);
        return (std::filesystem::path(dir.absolutePath().toStdString()) / "gitflux.db");
    }
    return std::filesystem::path("gitflux.db");
}

} // namespace gf::desktop
