#include <catch2/catch_test_macros.hpp>

#include "desktop/viewmodels/repository_list_model.hpp"
#include "desktop/viewmodels/group_list_model.hpp"
#include "desktop/viewmodels/workspace_list_model.hpp"
#include "desktop/viewmodels/home_hub_viewmodel.hpp"
#include "desktop/viewmodels/quick_open_viewmodel.hpp"

#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/persistence/repositories/sqlite_repository_store.hpp"
#include "infrastructure/persistence/groups/sqlite_group_store.hpp"
#include "infrastructure/persistence/workspaces/sqlite_workspace_store.hpp"

#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"

#include <QGuiApplication>
#include <QSignalSpy>
#include <filesystem>
#include <memory>

using namespace gf::desktop;
using namespace gf::infra::persistence::sqlite;
using namespace gf::infra::persistence::repositories;
using namespace gf::infra::persistence::groups;
using namespace gf::infra::persistence::workspaces;
using namespace gf::repositories::bo;
using namespace gf::repositories::bl;
using namespace gf::groups::bo;
using namespace gf::groups::bl;
using namespace gf::workspaces::bo;
using namespace gf::workspaces::bl;

namespace {

static int s_argc = 1;
static char s_appName[] = "gf_viewmodels_tests";
static char* s_argv[] = {s_appName, nullptr};

void ensureQtApp() {
    if (!QCoreApplication::instance()) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        new QGuiApplication(s_argc, s_argv);
    }
}

struct TestContext {
    std::filesystem::path tempDir;
    std::filesystem::path repoAPath;
    std::filesystem::path repoBPath;
    std::filesystem::path repoCPath;

    SqliteConnection connection;
    std::unique_ptr<SqliteRepositoryStore> repoStore;
    std::unique_ptr<SqliteGroupStore> groupStore;
    std::unique_ptr<SqliteWorkspaceStore> workspaceStore;

    std::unique_ptr<RepositoryService> repoService;
    std::unique_ptr<GroupService> groupService;
    std::unique_ptr<WorkspaceService> workspaceService;

    TestContext() {
        ensureQtApp();

        auto connResult = SqliteConnection::openInMemory();
        REQUIRE(connResult.has_value());
        connection = std::move(*connResult);

        auto migResult = SchemaMigrator::migrateToLatest(connection);
        REQUIRE(migResult.has_value());

        repoStore = std::make_unique<SqliteRepositoryStore>(connection);
        groupStore = std::make_unique<SqliteGroupStore>(connection);
        workspaceStore = std::make_unique<SqliteWorkspaceStore>(connection);

        repoService = std::make_unique<RepositoryService>(*repoStore);
        groupService = std::make_unique<GroupService>(*groupStore, *repoStore);
        workspaceService = std::make_unique<WorkspaceService>(*workspaceStore, *groupStore, *repoStore);

        tempDir = std::filesystem::temp_directory_path() / "gf_vm_test_env";
        std::filesystem::remove_all(tempDir);
        std::filesystem::create_directories(tempDir);

        repoAPath = tempDir / "repo_alpha";
        repoBPath = tempDir / "repo_beta";
        repoCPath = tempDir / "repo_gamma";

        std::filesystem::create_directories(repoAPath / ".git");
        std::filesystem::create_directories(repoBPath / ".git");
        std::filesystem::create_directories(repoCPath / ".git");
    }

    ~TestContext() {
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    }
};

} // namespace

TEST_CASE("RepositoryListModel exposes and updates repository data", "[desktop][mvvm]") {
    ensureQtApp();
    RepositoryListModel model;
    REQUIRE(model.rowCount() == 0);

    Repository r1{
        .id = RepositoryId{"repo_1"},
        .displayName = "Project Alpha",
        .localPath = std::filesystem::path("/work/alpha"),
        .pathStatus = RepositoryPathStatus::Valid,
        .registeredAtUnixEpochMs = 1000,
        .lastAccessedAtUnixEpochMs = 2000
    };

    Repository r2{
        .id = RepositoryId{"repo_2"},
        .displayName = "Project Beta",
        .localPath = std::filesystem::path("/work/beta"),
        .pathStatus = RepositoryPathStatus::Missing,
        .registeredAtUnixEpochMs = 1000,
        .lastAccessedAtUnixEpochMs = 2000
    };

    model.setRepositories({r1, r2});
    REQUIRE(model.rowCount() == 2);

    const auto idx0 = model.index(0);
    CHECK(model.data(idx0, RepositoryListModel::IdRole).toString() == "repo_1");
    CHECK(model.data(idx0, RepositoryListModel::DisplayNameRole).toString() == "Project Alpha");
    CHECK(model.data(idx0, RepositoryListModel::LocalPathRole).toString() == "/work/alpha");
    CHECK(model.data(idx0, RepositoryListModel::StatusRole).toString() == "valid");
    CHECK(model.data(idx0, RepositoryListModel::IsMissingRole).toBool() == false);

    const auto idx1 = model.index(1);
    CHECK(model.data(idx1, RepositoryListModel::IdRole).toString() == "repo_2");
    CHECK(model.data(idx1, RepositoryListModel::IsMissingRole).toBool() == true);
    CHECK(model.data(idx1, RepositoryListModel::StatusRole).toString() == "missing");

    // Dynamic update
    QSignalSpy spy(&model, &QAbstractListModel::dataChanged);
    model.updateStatus("repo_2", "valid", false);
    CHECK(spy.count() == 1);
    CHECK(model.data(idx1, RepositoryListModel::IsMissingRole).toBool() == false);
    CHECK(model.data(idx1, RepositoryListModel::StatusRole).toString() == "valid");
}

TEST_CASE("GroupListModel exposes group items and repository associations", "[desktop][mvvm]") {
    ensureQtApp();
    GroupListModel model;
    REQUIRE(model.rowCount() == 0);

    Group g1{
        .id = GroupId{"grp_backend"},
        .name = "Backend Services",
        .repositoryIds = {RepositoryId{"repo_api"}, RepositoryId{"repo_auth"}}
    };

    model.setGroups({g1});
    REQUIRE(model.rowCount() == 1);

    const auto idx = model.index(0);
    CHECK(model.data(idx, GroupListModel::IdRole).toString() == "grp_backend");
    CHECK(model.data(idx, GroupListModel::NameRole).toString() == "Backend Services");
    CHECK(model.data(idx, GroupListModel::RepositoryCountRole).toInt() == 2);
    CHECK(model.data(idx, GroupListModel::RepositoryIdsRole).toStringList().size() == 2);

    const auto item = model.findById("grp_backend");
    REQUIRE(item.has_value());
    CHECK(item->name == "Backend Services");
}

TEST_CASE("WorkspaceListModel exposes composition summaries and metadata", "[desktop][mvvm]") {
    TestContext ctx;
    auto repoA = *ctx.repoService->registerRepository(RegisterRepositoryRequest{.localPath = ctx.repoAPath, .displayName = std::nullopt});
    auto repoB = *ctx.repoService->registerRepository(RegisterRepositoryRequest{.localPath = ctx.repoBPath, .displayName = std::nullopt});
    auto grp = *ctx.groupService->createGroup("Core Group", {repoA.id});

    auto ws = *ctx.workspaceService->createWorkspace("Production Workspace", {grp.id}, {repoB.id});

    WorkspaceListModel model;
    model.setWorkspaces({ws}, *ctx.workspaceService);
    REQUIRE(model.rowCount() == 1);

    const auto idx = model.index(0);
    CHECK(model.data(idx, WorkspaceListModel::NameRole).toString().toStdString() == "Production Workspace");
    CHECK(model.data(idx, WorkspaceListModel::GroupCountRole).toInt() == 1);
    CHECK(model.data(idx, WorkspaceListModel::DirectRepoCountRole).toInt() == 1);
    CHECK(model.data(idx, WorkspaceListModel::TotalRepoCountRole).toInt() == 2);
    CHECK(model.data(idx, WorkspaceListModel::SummaryRole).toString().contains("1 gruppo"));
    CHECK(model.data(idx, WorkspaceListModel::SummaryRole).toString().contains("1 repo"));
}

TEST_CASE("HomeHubViewModel orchestrates catalog operations and search filtering", "[desktop][mvvm]") {
    TestContext ctx;
    HomeHubViewModel vm(*ctx.repoService, *ctx.groupService, *ctx.workspaceService);

    CHECK(vm.repositoriesCount() == 0);
    CHECK(vm.groupsCount() == 0);
    CHECK(vm.workspacesCount() == 0);

    // 1. Add Repository
    QSignalSpy repoSpy(&vm, &HomeHubViewModel::repositoriesChanged);
    bool added = vm.addRepository(QString::fromStdString(ctx.repoAPath.string()), "Alpha Repository");
    REQUIRE(added);
    CHECK(vm.repositoriesCount() == 1);
    CHECK(repoSpy.count() >= 1);

    const auto reposResult = ctx.repoService->getAllRepositories();
    REQUIRE(reposResult.has_value());
    REQUIRE(reposResult->size() == 1);
    const QString repoId = QString::fromStdString((*reposResult)[0].id.value());

    // 2. Open Repository Signal
    QSignalSpy openRepoSpy(&vm, &HomeHubViewModel::repositoryOpened);
    vm.openRepository(repoId);
    REQUIRE(openRepoSpy.count() == 1);
    CHECK(openRepoSpy.takeFirst().at(0).toString() == repoId);

    // 3. Create Group
    QSignalSpy grpSpy(&vm, &HomeHubViewModel::groupsChanged);
    bool grpCreated = vm.createGroup("Alpha Services", {repoId});
    REQUIRE(grpCreated);
    CHECK(vm.groupsCount() == 1);
    CHECK(grpSpy.count() >= 1);

    const auto groupsResult = ctx.groupService->getAllGroups();
    REQUIRE(groupsResult.has_value());
    REQUIRE(groupsResult->size() == 1);
    const QString groupId = QString::fromStdString((*groupsResult)[0].id.value());

    // Duplicate Group Name Error
    QSignalSpy errorSpy(&vm, &HomeHubViewModel::errorOccurred);
    bool duplicateGrp = vm.createGroup("Alpha Services");
    CHECK_FALSE(duplicateGrp);
    CHECK(errorSpy.count() == 1);

    // 4. Create Workspace
    QSignalSpy wsSpy(&vm, &HomeHubViewModel::workspacesChanged);
    bool wsCreated = vm.createWorkspace("Daily Work", {groupId}, {});
    REQUIRE(wsCreated);
    CHECK(vm.workspacesCount() == 1);
    CHECK(wsSpy.count() >= 1);

    // 5. Search Filtering
    vm.setSearchFilter("NonExistentQuery");
    CHECK(vm.repositoriesCount() == 0);
    CHECK(vm.groupsCount() == 0);
    CHECK(vm.workspacesCount() == 0);

    vm.setSearchFilter("Alpha");
    CHECK(vm.repositoriesCount() == 1);
    CHECK(vm.groupsCount() == 1);
    CHECK(vm.workspacesCount() == 0);

    vm.setSearchFilter("");
    CHECK(vm.repositoriesCount() == 1);
    CHECK(vm.groupsCount() == 1);
    CHECK(vm.workspacesCount() == 1);

    // 6. Delete Cascade Operations
    const auto workspacesResult = ctx.workspaceService->getAllWorkspaces();
    REQUIRE(workspacesResult.has_value());
    REQUIRE(workspacesResult->size() == 1);
    CHECK(vm.removeWorkspace(QString::fromStdString((*workspacesResult)[0].id.value())));
    CHECK(vm.workspacesCount() == 0);

    CHECK(vm.removeGroup(groupId));
    CHECK(vm.groupsCount() == 0);

    CHECK(vm.removeRepository(repoId));
    CHECK(vm.repositoriesCount() == 0);
}

TEST_CASE("QuickOpenViewModel provides multi-group selection and deterministic deduplication", "[desktop][mvvm]") {
    TestContext ctx;
    auto repoA = *ctx.repoService->registerRepository(RegisterRepositoryRequest{.localPath = ctx.repoAPath, .displayName = "Repo A"});
    auto repoB = *ctx.repoService->registerRepository(RegisterRepositoryRequest{.localPath = ctx.repoBPath, .displayName = "Repo B"});
    auto repoC = *ctx.repoService->registerRepository(RegisterRepositoryRequest{.localPath = ctx.repoCPath, .displayName = "Repo C"});

    // Grp 1: [A, B]
    // Grp 2: [B, C] -> B is shared
    auto grp1 = *ctx.groupService->createGroup("Frontend Group", {repoA.id, repoB.id});
    auto grp2 = *ctx.groupService->createGroup("Backend Group", {repoB.id, repoC.id});

    QuickOpenViewModel vm(*ctx.groupService, *ctx.repoService);

    CHECK(vm.selectedGroupsCount() == 0);
    CHECK(vm.hasSelection() == false);
    CHECK(vm.resolvedRepositoriesCount() == 0);
    CHECK(vm.deduplicatedCount() == 0);

    const QString g1Id = QString::fromStdString(grp1.id.value());
    const QString g2Id = QString::fromStdString(grp2.id.value());

    // Select Group 1
    vm.toggleGroup(g1Id);
    CHECK(vm.selectedGroupsCount() == 1);
    CHECK(vm.hasSelection() == true);
    CHECK(vm.resolvedRepositoriesCount() == 2);
    CHECK(vm.deduplicatedCount() == 0);

    // Select Group 2 as well -> B is shared!
    vm.toggleGroup(g2Id);
    CHECK(vm.selectedGroupsCount() == 2);
    CHECK(vm.resolvedRepositoriesCount() == 3); // A, B, C
    CHECK(vm.deduplicatedCount() == 1); // 1 duplicate united

    // Verify resolved repo details
    const auto resolved = vm.resolvedRepositories();
    REQUIRE(resolved.size() == 3);
    CHECK(resolved[0].toMap().value("displayName").toString().toStdString() == "Repo A");
    CHECK(resolved[1].toMap().value("displayName").toString().toStdString() == "Repo B");
    CHECK(resolved[2].toMap().value("displayName").toString().toStdString() == "Repo C");

    // Open Quick Session
    QSignalSpy sessionSpy(&vm, &QuickOpenViewModel::quickSessionStarted);
    bool sessionStarted = vm.openQuickSession();
    REQUIRE(sessionStarted);
    REQUIRE(sessionSpy.count() == 1);
    const auto passedIds = sessionSpy.takeFirst().at(0).toStringList();
    CHECK(passedIds.size() == 3);

    // Clear selection
    vm.clearSelection();
    CHECK(vm.selectedGroupsCount() == 0);
    CHECK(vm.resolvedRepositoriesCount() == 0);
    CHECK(vm.deduplicatedCount() == 0);

    // Guard on empty open session
    QSignalSpy errorSpy(&vm, &QuickOpenViewModel::errorOccurred);
    CHECK_FALSE(vm.openQuickSession());
    CHECK(errorSpy.count() == 1);
}
