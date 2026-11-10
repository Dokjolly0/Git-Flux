#include "desktop/bootstrap/app_bootstrap.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/git/git_discovery.hpp"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QDir>
#include <QFontDatabase>
#include <stdexcept>

namespace gf::desktop {

namespace {

QString resolveMonospaceFontFamily() {
    const QStringList candidates = {
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("Cascadia Code"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Fira Code"),
        QStringLiteral("Consolas"),
        QStringLiteral("SF Mono"),
        QStringLiteral("Menlo"),
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Liberation Mono")
    };
    for (const QString& family : candidates) {
        if (QFontDatabase::hasFamily(family)) {
            return family;
        }
    }
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

} // namespace
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
    m_cloneService = std::make_unique<git::bl::GitCloneService>(*m_gitOps, *m_repoService);
    m_stashService = std::make_unique<git::bl::GitStashService>(*m_gitOps);
    m_rebaseService = std::make_unique<git::bl::GitRebaseService>(*m_gitOps);
    m_conflictService = std::make_unique<git::bl::GitConflictService>(*m_gitOps);

    m_watcherPort = std::make_unique<infra::watcher::NativePollingFsWatcher>();
    m_watcherPort->start();

    m_watcherService = std::make_unique<git::bl::GitRepositoryWatcherService>(*m_watcherPort);
    m_watcherService->startAutoFlush();

    m_taskRunner = std::make_unique<tasks::bl::BackgroundTaskRunner>(4);
    m_asyncGitService = std::make_unique<git::bl::AsyncGitService>(*m_taskRunner, *m_remoteMergeService, *m_workingTreeService);
    m_batchCoordinator = std::make_unique<git::bl::MultiRepoBatchCoordinator>(
        *m_repoService, *m_groupService, *m_workspaceService,
        *m_remoteMergeService, *m_workingTreeService, *m_taskRunner
    );

    m_homeHubVm = std::make_unique<HomeHubViewModel>(*m_repoService, *m_groupService, *m_workspaceService, m_cloneService.get());
    m_quickOpenVm = std::make_unique<QuickOpenViewModel>(*m_groupService, *m_repoService);
    m_workspaceSessionVm = std::make_unique<WorkspaceSessionViewModel>(
        *m_repoService,
        *m_workspaceService,
        *m_workingTreeService,
        *m_stagingService,
        *m_diffService,
        *m_branchService,
        *m_historyService,
        *m_remoteMergeService,
        *m_stashService,
        *m_rebaseService,
        *m_conflictService,
        m_watcherService.get()
    );
    m_batchOperationsVm = std::make_unique<BatchOperationsViewModel>(*m_batchCoordinator);

    m_profileStore = std::make_unique<infra::persistence::profiles::SqliteProfileStore>(*m_connection);
    m_cryptoVault = std::make_unique<infra::security::CryptoVault>();
    m_credentialDetector = std::make_unique<infra::git::credentials::SystemCredentialDetector>();
    m_nam = std::make_unique<QNetworkAccessManager>();
    m_gitApiClient = std::make_unique<services::GitProviderApiClient>(m_nam.get());
    m_gitProfilesVm = std::make_unique<viewmodels::GitProfilesViewModel>(
        *m_profileStore, *m_cryptoVault, *m_credentialDetector, m_gitApiClient.get()
    );
    m_gitProfilesVm->initialize();
}

void AppBootstrap::registerWithQml(QQmlApplicationEngine& engine) {
    qmlRegisterUncreatableType<RepositoryListModel>("GitFlux.Desktop", 1, 0, "RepositoryListModel",
        "Model provided by HomeHubViewModel");
    qmlRegisterUncreatableType<GroupListModel>("GitFlux.Desktop", 1, 0, "GroupListModel",
        "Model provided by HomeHubViewModel");
    qmlRegisterUncreatableType<WorkspaceListModel>("GitFlux.Desktop", 1, 0, "WorkspaceListModel",
        "Model provided by HomeHubViewModel");
    qmlRegisterUncreatableType<WorkingTreeFileListModel>("GitFlux.Desktop", 1, 0, "WorkingTreeFileListModel",
        "Model provided by WorkingTreeViewModel");
    qmlRegisterUncreatableType<DiffLineListModel>("GitFlux.Desktop", 1, 0, "DiffLineListModel",
        "Model provided by DiffViewModel");
    qmlRegisterUncreatableType<BranchListModel>("GitFlux.Desktop", 1, 0, "BranchListModel",
        "Model provided by BranchManagerViewModel");
    qmlRegisterUncreatableType<CommitHistoryListModel>("GitFlux.Desktop", 1, 0, "CommitHistoryListModel",
        "Model provided by BranchManagerViewModel");

    qmlRegisterUncreatableType<HomeHubViewModel>("GitFlux.Desktop", 1, 0, "HomeHubViewModel",
        "ViewModel managed by bootstrap");
    qmlRegisterUncreatableType<QuickOpenViewModel>("GitFlux.Desktop", 1, 0, "QuickOpenViewModel",
        "ViewModel managed by bootstrap");
    qmlRegisterUncreatableType<WorkingTreeViewModel>("GitFlux.Desktop", 1, 0, "WorkingTreeViewModel",
        "ViewModel managed by WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<CommitBoxViewModel>("GitFlux.Desktop", 1, 0, "CommitBoxViewModel",
        "ViewModel managed by WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<DiffViewModel>("GitFlux.Desktop", 1, 0, "DiffViewModel",
        "ViewModel managed by WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<BranchManagerViewModel>("GitFlux.Desktop", 1, 0, "BranchManagerViewModel",
        "ViewModel managed by WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<StashListModel>("GitFlux.Desktop", 1, 0, "StashListModel",
        "Model provided by StashManagerViewModel");
    qmlRegisterUncreatableType<ConflictHunkListModel>("GitFlux.Desktop", 1, 0, "ConflictHunkListModel",
        "Model provided by ConflictResolverViewModel");
    qmlRegisterUncreatableType<StashManagerViewModel>("GitFlux.Desktop", 1, 0, "StashManagerViewModel",
        "ViewModel managed by WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<ConflictResolverViewModel>("GitFlux.Desktop", 1, 0, "ConflictResolverViewModel",
        "ViewModel managed by WorkspaceSessionViewModel");
    qmlRegisterUncreatableType<WorkspaceSessionViewModel>("GitFlux.Desktop", 1, 0, "WorkspaceSessionViewModel",
        "ViewModel managed by bootstrap");
    qmlRegisterUncreatableType<BatchOperationsViewModel>("GitFlux.Desktop", 1, 0, "BatchOperationsViewModel",
        "ViewModel managed by bootstrap");
    qmlRegisterUncreatableType<viewmodels::RemoteRepoListModel>("GitFlux.Desktop", 1, 0, "RemoteRepoListModel",
        "Model provided by GitProfilesViewModel");
    qmlRegisterUncreatableType<viewmodels::GitProfilesViewModel>("GitFlux.Desktop", 1, 0, "GitProfilesViewModel",
        "ViewModel managed by bootstrap");

    auto* rootCtx = engine.rootContext();
    rootCtx->setContextProperty(QStringLiteral("homeHubVm"), m_homeHubVm.get());
    rootCtx->setContextProperty(QStringLiteral("quickOpenVm"), m_quickOpenVm.get());
    rootCtx->setContextProperty(QStringLiteral("workspaceSessionVm"), m_workspaceSessionVm.get());
    rootCtx->setContextProperty(QStringLiteral("batchOperationsVm"), m_batchOperationsVm.get());
    rootCtx->setContextProperty(QStringLiteral("gitProfilesVm"), m_gitProfilesVm.get());
    rootCtx->setContextProperty(QStringLiteral("gfMonoFont"), resolveMonospaceFontFamily());
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

BatchOperationsViewModel& AppBootstrap::batchOperationsViewModel() noexcept {
    return *m_batchOperationsVm;
}

viewmodels::GitProfilesViewModel& AppBootstrap::gitProfilesViewModel() noexcept {
    return *m_gitProfilesVm;
}

git::bl::GitRepositoryWatcherService& AppBootstrap::watcherService() noexcept {
    return *m_watcherService;
}

git::bl::MultiRepoBatchCoordinator& AppBootstrap::batchCoordinator() noexcept {
    return *m_batchCoordinator;
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
        auto targetPath = std::filesystem::path(dir.absolutePath().toStdString()) / "gitflux.db";
        std::error_code ec;
        if (std::filesystem::exists(targetPath, ec) && std::filesystem::file_size(targetPath, ec) > 0) {
            return targetPath;
        }

        // Automatic discovery and migration of existing database across folder name variations
        const QString home = QDir::homePath();
        const std::vector<std::filesystem::path> candidates = {
            std::filesystem::path(home.toStdString()) / ".local/share/GitFlux/GitFlux/gitflux.db",
            std::filesystem::path(home.toStdString()) / ".local/share/GitFlux/git_flux/gitflux.db",
            std::filesystem::path(home.toStdString()) / ".local/share/GitFlux/gitflux.db"
        };
        for (const auto& candidate : candidates) {
            if (candidate != targetPath && std::filesystem::exists(candidate, ec) && std::filesystem::file_size(candidate, ec) > 0) {
                std::filesystem::create_directories(targetPath.parent_path(), ec);
                std::filesystem::copy_file(candidate, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec) {
                    return targetPath;
                }
                return candidate;
            }
        }
        return targetPath;
    }
    return std::filesystem::path("gitflux.db");
}

} // namespace gf::desktop
