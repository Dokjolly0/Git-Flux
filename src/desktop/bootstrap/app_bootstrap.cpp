#include "desktop/bootstrap/app_bootstrap.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/git/git_discovery.hpp"

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

    auto gitDiscovery = infra::git::GitDiscovery::discover();
    std::filesystem::path gitExe = gitDiscovery
        ? (gitDiscovery->directBinaryPath.empty() ? gitDiscovery->discoveredInPath : gitDiscovery->directBinaryPath)
        : "git";

    m_gitExecutor = std::make_unique<infra::git::NativeGitCommandExecutor>(gitExe);
    m_gitOps = std::make_unique<infra::git::NativeRepositoryGitOps>(*m_gitExecutor);

    m_workingTreeService = std::make_unique<git::bl::GitWorkingTreeService>(*m_gitOps);
    m_stagingService = std::make_unique<git::bl::GitStagingService>(*m_gitOps);
    m_diffService = std::make_unique<git::bl::GitDiffService>(*m_gitOps);
    m_branchService = std::make_unique<git::bl::GitBranchService>(*m_gitOps);
    m_historyService = std::make_unique<git::bl::GitHistoryService>(*m_gitOps);
    m_remoteMergeService = std::make_unique<git::bl::GitRemoteMergeService>(*m_gitOps);

    m_homeHubVm = std::make_unique<HomeHubViewModel>(*m_repoService, *m_groupService, *m_workspaceService);
    m_quickOpenVm = std::make_unique<QuickOpenViewModel>(*m_groupService, *m_repoService);
    m_workspaceSessionVm = std::make_unique<WorkspaceSessionViewModel>(
        *m_repoService,
        *m_workspaceService,
        *m_workingTreeService,
        *m_stagingService,
        *m_diffService,
        *m_branchService,
        *m_historyService,
        *m_remoteMergeService
    );
}

void AppBootstrap::registerWithQml(QQmlApplicationEngine& engine) {
    qmlRegisterUncreatableType<RepositoryListModel>("GitFlux.Desktop", 1, 0, "RepositoryListModel",
        "Modello fornito da HomeHubViewModel");
    qmlRegisterUncreatableType<GroupListModel>("GitFlux.Desktop", 1, 0, "GroupListModel",
        "Modello fornito da HomeHubViewModel");
    qmlRegisterUncreatableType<WorkspaceListModel>("GitFlux.Desktop", 1, 0, "WorkspaceListModel",
        "Modello fornito da HomeHubViewModel");
    qmlRegisterUncreatableType<WorkingTreeFileListModel>("GitFlux.Desktop", 1, 0, "WorkingTreeFileListModel",
        "Modello fornito da WorkingTreeViewModel");
    qmlRegisterUncreatableType<DiffLineListModel>("GitFlux.Desktop", 1, 0, "DiffLineListModel",
        "Modello fornito da DiffViewModel");
    qmlRegisterUncreatableType<BranchListModel>("GitFlux.Desktop", 1, 0, "BranchListModel",
        "Modello fornito da BranchManagerViewModel");
    qmlRegisterUncreatableType<CommitHistoryListModel>("GitFlux.Desktop", 1, 0, "CommitHistoryListModel",
        "Modello fornito da BranchManagerViewModel");

    qmlRegisterUncreatableType<HomeHubViewModel>("GitFlux.Desktop", 1, 0, "HomeHubViewModel",
        "ViewModel gestito dal bootstrap");
    qmlRegisterUncreatableType<QuickOpenViewModel>("GitFlux.Desktop", 1, 0, "QuickOpenViewModel",
        "ViewModel gestito dal bootstrap");
    qmlRegisterUncreatableType<WorkingTreeViewModel>("GitFlux.Desktop", 1, 0, "WorkingTreeViewModel",
        "ViewModel gestito da WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<CommitBoxViewModel>("GitFlux.Desktop", 1, 0, "CommitBoxViewModel",
        "ViewModel gestito da WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<DiffViewModel>("GitFlux.Desktop", 1, 0, "DiffViewModel",
        "ViewModel gestito da WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<BranchManagerViewModel>("GitFlux.Desktop", 1, 0, "BranchManagerViewModel",
        "ViewModel gestito da WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<WorkspaceSessionViewModel>("GitFlux.Desktop", 1, 0, "WorkspaceSessionViewModel",
        "ViewModel gestito dal bootstrap");

    auto* rootCtx = engine.rootContext();
    rootCtx->setContextProperty(QStringLiteral("homeHubVm"), m_homeHubVm.get());
    rootCtx->setContextProperty(QStringLiteral("quickOpenVm"), m_quickOpenVm.get());
    rootCtx->setContextProperty(QStringLiteral("workspaceSessionVm"), m_workspaceSessionVm.get());
}

HomeHubViewModel& AppBootstrap::homeHubViewModel() noexcept {
    return *m_homeHubVm;
}

QuickOpenViewModel& AppBootstrap::quickOpenViewModel() noexcept {
    return *m_quickOpenVm;
}

WorkspaceSessionViewModel& AppBootstrap::workspaceSessionViewModel() noexcept {
    return *m_workspaceSessionVm;
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
