#include <catch2/catch_test_macros.hpp>

#include "desktop/viewmodels/repository_list_model.hpp"
#include "desktop/viewmodels/group_list_model.hpp"
#include "desktop/viewmodels/workspace_list_model.hpp"
#include "desktop/viewmodels/home_hub_viewmodel.hpp"
#include "desktop/viewmodels/quick_open_viewmodel.hpp"
#include "desktop/viewmodels/working_tree_file_list_model.hpp"
#include "desktop/viewmodels/diff_line_list_model.hpp"
#include "desktop/viewmodels/working_tree_viewmodel.hpp"
#include "desktop/viewmodels/commit_box_viewmodel.hpp"
#include "desktop/viewmodels/diff_viewmodel.hpp"
#include "desktop/viewmodels/branch_list_model.hpp"
#include "desktop/viewmodels/commit_history_list_model.hpp"
#include "desktop/viewmodels/branch_manager_viewmodel.hpp"
#include "desktop/viewmodels/stash_list_model.hpp"
#include "desktop/viewmodels/stash_manager_viewmodel.hpp"
#include "desktop/viewmodels/conflict_hunk_list_model.hpp"
#include "desktop/viewmodels/conflict_resolver_viewmodel.hpp"
#include "desktop/viewmodels/workspace_session_viewmodel.hpp"
#include "desktop/viewmodels/batch_operations_viewmodel.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"
#include "core/modules/git/ports/fs_watcher_port.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/modules/git/bl/git_diff_service.hpp"
#include "core/modules/git/bl/git_branch_service.hpp"
#include "core/modules/git/bl/git_history_service.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "core/modules/git/bl/git_stash_service.hpp"
#include "core/modules/git/bl/git_rebase_service.hpp"
#include "core/modules/git/bl/git_conflict_service.hpp"
#include "core/modules/git/bl/git_repository_watcher_service.hpp"
#include "core/modules/git/bl/async_git_service.hpp"
#include "core/modules/git/bl/multi_repo_batch_coordinator.hpp"
#include "core/modules/tasks/bl/background_task_runner.hpp"

#include <set>

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

TEST_CASE("HomeHubViewModel handles explorer paths, .git suffix, and comprehensive error feedback", "[desktop][mvvm]") {
    TestContext ctx;
    HomeHubViewModel vm(*ctx.repoService, *ctx.groupService, *ctx.workspaceService);

    // 1. Adding with .git suffix resolves correctly and clears error
    QSignalSpy addedSpy(&vm, &HomeHubViewModel::repositoryAdded);
    const QString dotGitPath = QString::fromStdString((ctx.repoAPath / ".git").string());
    bool ok1 = vm.addRepository(dotGitPath);
    REQUIRE(ok1);
    CHECK(vm.repositoriesCount() == 1);
    CHECK(vm.lastError().isEmpty());
    CHECK(vm.lastSuccessMessage().contains("repo_alpha"));
    CHECK(addedSpy.count() == 1);

    // 2. Adding with file:// URL prefix (as produced by FolderDialog) resolves correctly
    const QString fileUrlPath = "file://" + QString::fromStdString(ctx.repoBPath.string()) + "/.git/";
    bool ok2 = vm.addRepository(fileUrlPath);
    REQUIRE(ok2);
    CHECK(vm.repositoriesCount() == 2);
    CHECK(vm.lastError().isEmpty());
    CHECK(vm.lastSuccessMessage().contains("repo_beta"));

    // 3. Adding an empty path produces error feedback
    QSignalSpy errorSpy(&vm, &HomeHubViewModel::errorOccurred);
    bool errEmpty = vm.addRepository("");
    CHECK_FALSE(errEmpty);
    CHECK(errorSpy.count() == 1);
    CHECK(!vm.lastError().isEmpty());

    // 4. Adding a non-existent path produces error feedback
    bool errGhost = vm.addRepository("/non_existent_folder_xyz/nowhere");
    CHECK_FALSE(errGhost);
    CHECK(errorSpy.count() == 2);
    CHECK(vm.lastError().contains("Percorso non trovato"));

    // 5. Adding a regular folder without .git produces clear feedback
    const auto regularFolder = ctx.tempDir / "not_a_repo";
    std::filesystem::create_directories(regularFolder);
    bool errNotGit = vm.addRepository(QString::fromStdString(regularFolder.string()));
    CHECK_FALSE(errNotGit);
    CHECK(errorSpy.count() == 3);
    CHECK(vm.lastError().contains("non contiene un repository Git valido"));

    // 6. Adding already registered repository produces duplicate feedback
    bool errDup = vm.addRepository(QString::fromStdString(ctx.repoAPath.string()));
    CHECK_FALSE(errDup);
    CHECK(errorSpy.count() == 4);
    CHECK(vm.lastError().contains("già registrato"));

    // 7. Clear feedback resets feedback properties
    vm.clearFeedback();
    CHECK(vm.lastError().isEmpty());
    CHECK(vm.lastSuccessMessage().isEmpty());
}

TEST_CASE("HomeHubViewModel interactive operations: renaming, group repo association, and group opening", "[desktop][mvvm]") {
    TestContext ctx;
    HomeHubViewModel vm(*ctx.repoService, *ctx.groupService, *ctx.workspaceService);

    // 1. Add two repositories
    REQUIRE(vm.addRepository(QString::fromStdString(ctx.repoAPath.string()), "Project Alpha"));
    REQUIRE(vm.addRepository(QString::fromStdString(ctx.repoBPath.string()), "Project Beta"));

    const auto repos = ctx.repoService->getAllRepositories();
    REQUIRE(repos.has_value());
    REQUIRE(repos->size() == 2);
    const QString repoAId = QString::fromStdString((*repos)[0].id.value());
    const QString repoBId = QString::fromStdString((*repos)[1].id.value());

    // 2. Rename repository
    bool renamed = vm.renameRepository(repoAId, "Alpha Engine");
    CHECK(renamed);
    auto repoAUpdated = ctx.repoService->getRepository((*repos)[0].id);
    REQUIRE(repoAUpdated.has_value());
    CHECK((*repoAUpdated)->displayName == "Alpha Engine");

    // 3. Create Group with Project Alpha
    bool grpCreated = vm.createGroup("Backend Team", {repoAId});
    REQUIRE(grpCreated);
    const auto groups = ctx.groupService->getAllGroups();
    REQUIRE(groups.has_value());
    REQUIRE(groups->size() == 1);
    const QString grpId = QString::fromStdString((*groups)[0].id.value());

    // 4. Rename Group
    bool grpRenamed = vm.renameGroup(grpId, "Core Backend");
    CHECK(grpRenamed);
    auto grpUpdated = ctx.groupService->getGroup((*groups)[0].id);
    REQUIRE(grpUpdated.has_value());
    CHECK((*grpUpdated)->name == "Core Backend");

    // 5. Add second repository to group
    bool addedToGrp = vm.addRepositoryToGroup(grpId, repoBId);
    CHECK(addedToGrp);
    auto grpWithTwo = ctx.groupService->getGroup((*groups)[0].id);
    REQUIRE(grpWithTwo.has_value());
    CHECK((*grpWithTwo)->repositoryIds.size() == 2);

    // Verify GroupListModel reflects the updated repositories and names
    CHECK(vm.groupsCount() == 1);
    QModelIndex grpIdx = vm.groupModel()->index(0, 0);
    QVariant repoNamesVar = vm.groupModel()->data(grpIdx, GroupListModel::RepositoryNamesRole);
    QStringList repoNamesList = repoNamesVar.toStringList();
    CHECK(repoNamesList.size() == 2);

    // 6. Open Group emits groupOpened with all repo IDs
    QSignalSpy openGrpSpy(&vm, &HomeHubViewModel::groupOpened);
    vm.openGroup(grpId);
    REQUIRE(openGrpSpy.count() == 1);
    QList<QVariant> args = openGrpSpy.takeFirst();
    CHECK(args.at(0).toString() == grpId);
    CHECK(args.at(1).toString() == "Core Backend");
    QStringList openedRepoIds = args.at(2).toStringList();
    CHECK(openedRepoIds.size() == 2);
    CHECK(openedRepoIds.contains(repoAId));
    CHECK(openedRepoIds.contains(repoBId));

    // 7. Remove repository from group
    bool removedFromGrp = vm.removeRepositoryFromGroup(grpId, repoAId);
    CHECK(removedFromGrp);
    auto grpWithOne = ctx.groupService->getGroup((*groups)[0].id);
    REQUIRE(grpWithOne.has_value());
    CHECK((*grpWithOne)->repositoryIds.size() == 1);
    CHECK((*grpWithOne)->repositoryIds[0].value() == repoBId.toStdString());

    // 8. Copy to clipboard
    vm.copyToClipboard("/test/path");
    CHECK(vm.lastSuccessMessage().contains("Copiato negli appunti"));
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

class MockFullGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    gf::git::bo::RepoWorkingTreeStatus statusToReturn;
    gf::git::bo::FileDiff diffToReturn;
    bool getStatusCalled = false;
    bool stageCalled = false;
    bool unstageCalled = false;
    bool discardCalled = false;
    bool commitCalled = false;

    std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
    getWorkingTreeStatus(const std::filesystem::path&) override {
        getStatusCalled = true;
        return statusToReturn;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stageFiles(const std::filesystem::path&, const std::vector<std::string>&) override {
        stageCalled = true;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stageAll(const std::filesystem::path&) override {
        stageCalled = true;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    unstageFiles(const std::filesystem::path&, const std::vector<std::string>&) override {
        unstageCalled = true;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    unstageAll(const std::filesystem::path&) override {
        unstageCalled = true;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    discardFiles(const std::filesystem::path&, const std::vector<std::string>&) override {
        discardCalled = true;
        return {};
    }

    std::expected<gf::git::bo::CommitResult, gf::git::ports::GitOpsError>
    createCommit(const std::filesystem::path&, const gf::git::bo::CommitOptions&) override {
        commitCalled = true;
        return gf::git::bo::CommitResult{
            .commitSha = "1234567890123456789012345678901234567890",
            .shortSha = "12345678",
            .summary = "Commit test",
            .branch = "main"
        };
    }

    std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
    getFileDiff(const std::filesystem::path&, const std::string&, gf::git::bo::DiffScope) override {
        return diffToReturn;
    }

    std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError>
    getAllDiffs(const std::filesystem::path&, gf::git::bo::DiffScope) override {
        return std::vector<gf::git::bo::FileDiff>{diffToReturn};
    }

    std::expected<std::vector<gf::git::bo::GitBranch>, gf::git::ports::GitOpsError>
    listBranches(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitBranch>{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    createBranch(const std::filesystem::path&, const gf::git::bo::CreateBranchOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    checkoutBranch(const std::filesystem::path&, const gf::git::bo::CheckoutOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    deleteBranch(const std::filesystem::path&, const std::string&, bool) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    renameBranch(const std::filesystem::path&, const std::string&, const std::string&) override {
        return {};
    }

    std::expected<std::vector<gf::git::bo::GitCommitLogItem>, gf::git::ports::GitOpsError>
    getCommitLog(const std::filesystem::path&, const gf::git::bo::CommitLogOptions&) override {
        return std::vector<gf::git::bo::GitCommitLogItem>{};
    }

    std::expected<std::vector<gf::git::bo::GitTag>, gf::git::ports::GitOpsError>
    listTags(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitTag>{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    createTag(const std::filesystem::path&, const gf::git::bo::CreateTagOptions&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    deleteTag(const std::filesystem::path&, const std::string&) override {
        return {};
    }

    std::expected<std::vector<gf::git::bo::GitRemote>, gf::git::ports::GitOpsError>
    listRemotes(const std::filesystem::path&) override {
        return std::vector<gf::git::bo::GitRemote>{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    addRemote(const std::filesystem::path&, const std::string&, const std::string&) override {
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    removeRemote(const std::filesystem::path&, const std::string&) override {
        return {};
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    fetchRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override {
        return gf::git::bo::RemoteOperationResult{.success = true};
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    pullRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override {
        return gf::git::bo::RemoteOperationResult{.success = true};
    }

    std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    pushRemote(const std::filesystem::path&, const gf::git::bo::RemoteOperationOptions&) override {
        return gf::git::bo::RemoteOperationResult{.success = true};
    }

    std::expected<gf::git::bo::MergeResult, gf::git::ports::GitOpsError>
    mergeBranch(const std::filesystem::path&, const gf::git::bo::MergeOptions&) override {
        return gf::git::bo::MergeResult{.status = gf::git::bo::MergeStatus::Merged};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    abortMerge(const std::filesystem::path&) override {
        return {};
    }

    std::vector<gf::git::bo::GitStashItem> stashesToReturn;
    bool stashPushCalled = false;
    bool stashPopCalled = false;
    bool stashApplyCalled = false;
    bool stashDropCalled = false;
    bool stashClearCalled = false;

    bool rebaseInProgressToReturn = false;
    bool startRebaseCalled = false;
    bool continueRebaseCalled = false;
    bool skipRebaseCalled = false;
    bool abortRebaseCalled = false;

    gf::git::bo::ConflictFile conflictFileToReturn;
    bool applyConflictResolutionCalled = false;

    std::expected<std::vector<gf::git::bo::GitStashItem>, gf::git::ports::GitOpsError>
    listStashes(const std::filesystem::path&) override {
        return stashesToReturn;
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashPush(const std::filesystem::path&, const gf::git::bo::StashPushOptions&) override {
        stashPushCalled = true;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashPop(const std::filesystem::path&, const gf::git::bo::StashApplyOptions&) override {
        stashPopCalled = true;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashApply(const std::filesystem::path&, const gf::git::bo::StashApplyOptions&) override {
        stashApplyCalled = true;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashDrop(const std::filesystem::path&, std::size_t) override {
        stashDropCalled = true;
        return {};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    stashClear(const std::filesystem::path&) override {
        stashClearCalled = true;
        return {};
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    startRebase(const std::filesystem::path&, const gf::git::bo::RebaseOptions&) override {
        startRebaseCalled = true;
        return gf::git::bo::RebaseResult{.status = gf::git::bo::RebaseStatus::Applied, .message = "Rebase started"};
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    continueRebase(const std::filesystem::path&) override {
        continueRebaseCalled = true;
        return gf::git::bo::RebaseResult{.status = gf::git::bo::RebaseStatus::Finished, .message = "Rebase finished"};
    }

    std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    skipRebase(const std::filesystem::path&) override {
        skipRebaseCalled = true;
        return gf::git::bo::RebaseResult{.status = gf::git::bo::RebaseStatus::Skipped, .message = "Rebase skipped"};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    abortRebase(const std::filesystem::path&) override {
        abortRebaseCalled = true;
        return {};
    }

    std::expected<bool, gf::git::ports::GitOpsError>
    isRebaseInProgress(const std::filesystem::path&) override {
        return rebaseInProgressToReturn;
    }

    std::expected<gf::git::bo::CherryPickResult, gf::git::ports::GitOpsError>
    cherryPick(const std::filesystem::path&, const gf::git::bo::CherryPickOptions&) override {
        return gf::git::bo::CherryPickResult{.success = true};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    abortCherryPick(const std::filesystem::path&) override {
        return {};
    }

    std::expected<gf::git::bo::ConflictFile, gf::git::ports::GitOpsError>
    getConflictFile(const std::filesystem::path&, const std::string&) override {
        return conflictFileToReturn;
    }

    std::expected<std::string, gf::git::ports::GitOpsError>
    getFileStageContent(const std::filesystem::path&, const std::string&, int) override {
        return std::string{};
    }

    std::expected<void, gf::git::ports::GitOpsError>
    applyConflictResolution(const std::filesystem::path&, const gf::git::bo::ApplyConflictResolutionOptions&) override {
        applyConflictResolutionCalled = true;
        return {};
    }
};

TEST_CASE("WorkingTreeFileListModel and DiffLineListModel MVVM models", "[viewmodel][models]") {
    ensureQtApp();

    SECTION("WorkingTreeFileListModel handles items and roles correctly") {
        WorkingTreeFileListModel model;
        CHECK(model.rowCount() == 0);

        std::vector<gf::git::bo::WorkingTreeFileItem> items = {
            gf::git::bo::WorkingTreeFileItem{
                .path = "src/app.cpp",
                .originalPath = {},
                .kind = gf::git::bo::FileChangeKind::Modified,
                .statusCode = 'M'
            },
            gf::git::bo::WorkingTreeFileItem{
                .path = "new_doc.md",
                .originalPath = {},
                .kind = gf::git::bo::FileChangeKind::Added,
                .statusCode = 'A'
            }
        };

        model.setItems(std::move(items));
        REQUIRE(model.rowCount() == 2);

        QModelIndex idx0 = model.index(0, 0);
        CHECK(model.data(idx0, WorkingTreeFileListModel::PathRole).toString().toStdString() == "src/app.cpp");
        CHECK(model.data(idx0, WorkingTreeFileListModel::KindRole).toString().toStdString() == "Modified");
        CHECK(model.data(idx0, WorkingTreeFileListModel::StatusCodeRole).toString().toStdString() == "M");

        QModelIndex idx1 = model.index(1, 0);
        CHECK(model.data(idx1, WorkingTreeFileListModel::KindRole).toString().toStdString() == "Added");
        CHECK(model.data(idx1, WorkingTreeFileListModel::BadgeColorRole).toString().toStdString() == "#34D399");

        model.clear();
        CHECK(model.rowCount() == 0);
    }

    SECTION("DiffLineListModel creates flat representation with line numbers") {
        DiffLineListModel diffModel;
        CHECK(diffModel.rowCount() == 0);

        gf::git::bo::FileDiff diff;
        diff.oldPath = "file.txt";
        diff.newPath = "file.txt";
        gf::git::bo::DiffHunk hunk;
        hunk.header = "@@ -1,2 +1,3 @@";
        hunk.lines.push_back(gf::git::bo::DiffLine{
            .kind = gf::git::bo::DiffLineKind::Context,
            .oldLineNumber = 1,
            .newLineNumber = 1,
            .content = "line 1"
        });
        hunk.lines.push_back(gf::git::bo::DiffLine{
            .kind = gf::git::bo::DiffLineKind::Addition,
            .oldLineNumber = std::nullopt,
            .newLineNumber = 2,
            .content = "line 2 added"
        });
        diff.hunks.push_back(std::move(hunk));

        diffModel.setDiff(diff);
        // 1 hunk header + 2 lines = 3
        REQUIRE(diffModel.rowCount() == 3);

        QModelIndex hIdx = diffModel.index(0, 0);
        CHECK(diffModel.data(hIdx, DiffLineListModel::LineKindRole).toString().toStdString() == "header");

        QModelIndex lIdx = diffModel.index(2, 0);
        CHECK(diffModel.data(lIdx, DiffLineListModel::LineKindRole).toString().toStdString() == "addition");
        CHECK(diffModel.data(lIdx, DiffLineListModel::NewLineNumberRole).toString().toStdString() == "2");
        CHECK(diffModel.data(lIdx, DiffLineListModel::OldLineNumberRole).toString().isEmpty());

        diffModel.clear();
        CHECK(diffModel.rowCount() == 0);
    }
}

TEST_CASE("WorkingTreeViewModel and CommitBoxViewModel reactive behavior", "[viewmodel][session]") {
    ensureQtApp();

    MockFullGitOps mockOps;
    gf::git::bl::GitWorkingTreeService statusService(mockOps);
    gf::git::bl::GitStagingService stagingService(mockOps);

    auto validTempDir = std::filesystem::temp_directory_path();

    SECTION("WorkingTreeViewModel refreshes and manages changes") {
        WorkingTreeViewModel wtVm(statusService, stagingService);

        mockOps.statusToReturn.branch.headName = "feature/phase2";
        mockOps.statusToReturn.branch.ahead = 2;
        mockOps.statusToReturn.stagedFiles.push_back(gf::git::bo::WorkingTreeFileItem{
            .path = "a.txt",
            .originalPath = {},
            .kind = gf::git::bo::FileChangeKind::Added,
            .statusCode = 'A'
        });

        QSignalSpy statusSpy(&wtVm, &WorkingTreeViewModel::statusChanged);
        bool refreshed = wtVm.refresh(QString::fromStdString(validTempDir.string()));
        REQUIRE(refreshed);
        CHECK(statusSpy.count() == 1);
        CHECK(wtVm.branchName().toStdString() == "feature/phase2");
        CHECK(wtVm.aheadCount() == 2);
        CHECK(wtVm.stagedCount() == 1);
        CHECK(wtVm.stagedModel()->rowCount() == 1);
        CHECK_FALSE(wtVm.isClean());

        // Stage file trigger
        CHECK(wtVm.stageFile(QString::fromStdString(validTempDir.string()), "b.txt"));
        CHECK(mockOps.stageCalled);

        // Unstage file trigger
        CHECK(wtVm.unstageFile(QString::fromStdString(validTempDir.string()), "a.txt"));
        CHECK(mockOps.unstageCalled);
    }

    SECTION("CommitBoxViewModel validates message and triggers commit") {
        CommitBoxViewModel commitVm(stagingService);

        CHECK_FALSE(commitVm.canCommit());

        QSignalSpy canCommitSpy(&commitVm, &CommitBoxViewModel::canCommitChanged);
        commitVm.setCommitMessage("feat(core): initial implementation");
        CHECK(commitVm.canCommit());
        CHECK(canCommitSpy.count() >= 1);

        QSignalSpy successSpy(&commitVm, &CommitBoxViewModel::commitSucceeded);
        bool committed = commitVm.executeCommit(QString::fromStdString(validTempDir.string()));
        REQUIRE(committed);
        CHECK(mockOps.commitCalled);
        CHECK(successSpy.count() == 1);

        // Upon commit completion, message is reset
        CHECK(commitVm.commitMessage().isEmpty());
        CHECK_FALSE(commitVm.canCommit());
    }
}

TEST_CASE("WorkspaceSessionViewModel multi-tab orchestration", "[viewmodel][session][tabs]") {
    ensureQtApp();

    TestContext ctx;
    auto r1Res = ctx.repoService->registerRepository({
        .localPath = ctx.repoAPath,
        .displayName = "Repo 1"
    });
    REQUIRE(r1Res.has_value());

    auto r2Res = ctx.repoService->registerRepository({
        .localPath = ctx.repoBPath,
        .displayName = "Repo 2"
    });
    REQUIRE(r2Res.has_value());

    MockFullGitOps mockOps;
    gf::git::bl::GitWorkingTreeService statusService(mockOps);
    gf::git::bl::GitStagingService stagingService(mockOps);
    gf::git::bl::GitDiffService diffService(mockOps);
    gf::git::bl::GitBranchService branchService(mockOps);
    gf::git::bl::GitHistoryService historyService(mockOps);
    gf::git::bl::GitRemoteMergeService remoteMergeService(mockOps);
    gf::git::bl::GitStashService stashService(mockOps);
    gf::git::bl::GitRebaseService rebaseService(mockOps);
    gf::git::bl::GitConflictService conflictService(mockOps);

    WorkspaceSessionViewModel sessionVm(
        *ctx.repoService,
        *ctx.workspaceService,
        statusService,
        stagingService,
        diffService,
        branchService,
        historyService,
        remoteMergeService,
        stashService,
        rebaseService,
        conflictService
    );

    CHECK_FALSE(sessionVm.hasOpenTabs());
    CHECK(sessionVm.activeTabIndex() == -1);

    QSignalSpy tabsSpy(&sessionVm, &WorkspaceSessionViewModel::tabsChanged);
    QSignalSpy activeSpy(&sessionVm, &WorkspaceSessionViewModel::activeTabChanged);

    // Open Repo 1
    sessionVm.openRepository(QString::fromStdString(r1Res->id.value()), "Repo 1", QString::fromStdString(ctx.repoAPath.string()));
    CHECK(sessionVm.hasOpenTabs());
    CHECK(sessionVm.tabs().size() == 1);
    CHECK(sessionVm.activeTabIndex() == 0);
    CHECK(sessionVm.activeRepoName().toStdString() == "Repo 1");
    CHECK(sessionVm.activeRepoPath().toStdString() == ctx.repoAPath.string());

    // Open Repo 2
    sessionVm.openRepository(QString::fromStdString(r2Res->id.value()), "Repo 2", QString::fromStdString(ctx.repoBPath.string()));
    CHECK(sessionVm.tabs().size() == 2);
    CHECK(sessionVm.activeTabIndex() == 1);
    CHECK(sessionVm.activeRepoName().toStdString() == "Repo 2");

    // Re-opening Repo 1 switches to existing tab
    sessionVm.openRepository(QString::fromStdString(r1Res->id.value()), "Repo 1", QString::fromStdString(ctx.repoAPath.string()));
    CHECK(sessionVm.tabs().size() == 2);
    CHECK(sessionVm.activeTabIndex() == 0);

    // Close active tab
    sessionVm.closeTab(0);
    CHECK(sessionVm.tabs().size() == 1);
    CHECK(sessionVm.activeTabIndex() == 0);
    CHECK(sessionVm.activeRepoName().toStdString() == "Repo 2");

    // Close last tab
    sessionVm.closeTab(0);
    CHECK_FALSE(sessionVm.hasOpenTabs());
    CHECK(sessionVm.activeTabIndex() == -1);

    // Test openGroup orchestration: opens all repos in group, sets currentGroupName
    QSignalSpy groupNameSpy(&sessionVm, &WorkspaceSessionViewModel::currentGroupNameChanged);
    sessionVm.openGroup("Core Microservices", {
        QString::fromStdString(r1Res->id.value()),
        QString::fromStdString(r2Res->id.value())
    });

    CHECK(sessionVm.currentGroupName() == "Core Microservices");
    CHECK(groupNameSpy.count() >= 1);
    CHECK(sessionVm.tabs().size() == 2);
    CHECK(sessionVm.hasOpenTabs());

    // Closing all tabs clears group name
    sessionVm.closeAllTabs();
    CHECK_FALSE(sessionVm.hasOpenTabs());
    CHECK(sessionVm.currentGroupName().isEmpty());
}

TEST_CASE("BranchListModel and CommitHistoryListModel MVVM representations", "[viewmodel][models][branch][history]") {
    ensureQtApp();

    SECTION("BranchListModel handles branch roles and reset") {
        BranchListModel model;
        CHECK(model.rowCount() == 0);

        std::vector<gf::git::bo::GitBranch> branches = {
            gf::git::bo::GitBranch{
                .name = "main",
                .shortName = "main",
                .fullName = "refs/heads/main",
                .targetCommitSha = "aaaa1111",
                .upstream = "origin/main",
                .ahead = 2,
                .behind = 1,
                .isCurrent = true,
                .isDetached = false,
                .type = gf::git::bo::BranchType::Local
            },
            gf::git::bo::GitBranch{
                .name = "origin/feature",
                .shortName = "feature",
                .fullName = "refs/remotes/origin/feature",
                .targetCommitSha = "bbbb2222",
                .upstream = {},
                .ahead = 0,
                .behind = 0,
                .isCurrent = false,
                .isDetached = false,
                .type = gf::git::bo::BranchType::Remote
            }
        };

        model.setBranches(branches);
        REQUIRE(model.rowCount() == 2);

        auto idx0 = model.index(0);
        CHECK(model.data(idx0, BranchListModel::NameRole).toString() == "main");
        CHECK(model.data(idx0, BranchListModel::ShortNameRole).toString() == "main");
        CHECK(model.data(idx0, BranchListModel::IsCurrentRole).toBool() == true);
        CHECK(model.data(idx0, BranchListModel::IsRemoteRole).toBool() == false);
        CHECK(model.data(idx0, BranchListModel::UpstreamRole).toString() == "origin/main");
        CHECK(model.data(idx0, BranchListModel::AheadCountRole).toLongLong() == 2);
        CHECK(model.data(idx0, BranchListModel::BehindCountRole).toLongLong() == 1);
        CHECK(model.data(idx0, BranchListModel::TargetShaRole).toString() == "aaaa1111");

        auto idx1 = model.index(1);
        CHECK(model.data(idx1, BranchListModel::IsRemoteRole).toBool() == true);
        CHECK(model.data(idx1, BranchListModel::IsCurrentRole).toBool() == false);
    }

    SECTION("CommitHistoryListModel handles commit roles and reset") {
        CommitHistoryListModel model;
        CHECK(model.rowCount() == 0);

        std::vector<gf::git::bo::GitCommitLogItem> commits = {
            gf::git::bo::GitCommitLogItem{
                .sha = "1234567890abcdef",
                .shortSha = "1234567",
                .parents = {"parent1", "parent2"},
                .authorName = "Author A",
                .authorEmail = "a@test.com",
                .authorDateIso = "2026-11-07T12:00:00Z",
                .subject = "Commit Subject",
                .body = "Commit Body",
                .refDecorations = {"HEAD -> main", "tag: v1.0"}
            }
        };

        model.setCommits(commits);
        REQUIRE(model.rowCount() == 1);

        auto idx0 = model.index(0);
        CHECK(model.data(idx0, CommitHistoryListModel::ShaRole).toString() == "1234567890abcdef");
        CHECK(model.data(idx0, CommitHistoryListModel::ShortShaRole).toString() == "1234567");
        CHECK(model.data(idx0, CommitHistoryListModel::SubjectRole).toString() == "Commit Subject");
        CHECK(model.data(idx0, CommitHistoryListModel::BodyRole).toString() == "Commit Body");
        CHECK(model.data(idx0, CommitHistoryListModel::AuthorNameRole).toString() == "Author A");
        CHECK(model.data(idx0, CommitHistoryListModel::AuthorEmailRole).toString() == "a@test.com");
        CHECK(model.data(idx0, CommitHistoryListModel::AuthorDateRole).toString() == "2026-11-07T12:00:00Z");

        auto decs = model.data(idx0, CommitHistoryListModel::DecorationsRole).toStringList();
        REQUIRE(decs.size() == 2);
        CHECK(decs[0] == "HEAD -> main");
        CHECK(decs[1] == "tag: v1.0");

        auto parents = model.data(idx0, CommitHistoryListModel::ParentsRole).toStringList();
        REQUIRE(parents.size() == 2);
        CHECK(parents[0] == "parent1");
        CHECK(parents[1] == "parent2");
    }
}

TEST_CASE("BranchManagerViewModel operations and signal propagation", "[viewmodel][branch][manager]") {
    ensureQtApp();

    MockFullGitOps mockOps;
    gf::git::bl::GitBranchService branchService(mockOps);
    gf::git::bl::GitHistoryService historyService(mockOps);
    gf::git::bl::GitRemoteMergeService remoteMergeService(mockOps);

    BranchManagerViewModel branchVm(branchService, historyService, remoteMergeService);
    auto tempDir = std::filesystem::temp_directory_path();

    branchVm.setRepositoryPath(QString::fromStdString(tempDir.string()));

    QSignalSpy switchedSpy(&branchVm, &BranchManagerViewModel::branchSwitched);
    QSignalSpy completedSpy(&branchVm, &BranchManagerViewModel::operationCompleted);

    // Checkout
    bool checkedOut = branchVm.checkoutBranch("main");
    CHECK(checkedOut);
    CHECK(switchedSpy.count() == 1);
    CHECK(completedSpy.count() == 1);

    // Create branch
    bool created = branchVm.createBranch("feature/test", true);
    CHECK(created);
    CHECK(switchedSpy.count() == 2);
    CHECK(completedSpy.count() == 2);

    // Delete branch
    bool deleted = branchVm.deleteBranch("feature/test", false);
    CHECK(deleted);
    CHECK(completedSpy.count() == 3);

    // Rename branch
    bool renamed = branchVm.renameBranch("old-name", "new-name");
    CHECK(renamed);
    CHECK(completedSpy.count() == 4);

    // Merge branch
    bool merged = branchVm.mergeBranch("feature/test", false);
    CHECK(merged);
    CHECK(completedSpy.count() == 5);
}

TEST_CASE("StashListModel and StashManagerViewModel operations", "[viewmodel][stash]") {
    ensureQtApp();

    SECTION("StashListModel manages items and exposes correct roles") {
        StashListModel model;
        CHECK(model.rowCount() == 0);

        std::vector<gf::git::bo::GitStashItem> stashes = {
            gf::git::bo::GitStashItem{
                .index = 0,
                .selector = "stash@{0}",
                .commitSha = "abc123456789",
                .dateIso = "2026-11-07T12:00:00Z",
                .message = "WIP work on feature",
                .branchName = "main"
            },
            gf::git::bo::GitStashItem{
                .index = 1,
                .selector = "stash@{1}",
                .commitSha = "def987654321",
                .dateIso = "2026-11-06T10:00:00Z",
                .message = "On dev: quick fix",
                .branchName = "dev"
            }
        };

        model.setStashes(stashes);
        REQUIRE(model.rowCount() == 2);

        auto idx0 = model.index(0);
        CHECK(model.data(idx0, StashListModel::IndexRole).toLongLong() == 0);
        CHECK(model.data(idx0, StashListModel::SelectorRole).toString() == "stash@{0}");
        CHECK(model.data(idx0, StashListModel::CommitShaRole).toString() == "abc123456789");
        CHECK(model.data(idx0, StashListModel::DateIsoRole).toString() == "2026-11-07T12:00:00Z");
        CHECK(model.data(idx0, StashListModel::MessageRole).toString() == "WIP work on feature");
        CHECK(model.data(idx0, StashListModel::BranchNameRole).toString() == "main");

        auto idx1 = model.index(1);
        CHECK(model.data(idx1, StashListModel::IndexRole).toLongLong() == 1);
        CHECK(model.data(idx1, StashListModel::SelectorRole).toString() == "stash@{1}");

        model.clear();
        CHECK(model.rowCount() == 0);
    }

    SECTION("StashManagerViewModel interacts with GitStashService and signals") {
        MockFullGitOps mockOps;
        mockOps.stashesToReturn = {
            gf::git::bo::GitStashItem{
                .index = 0,
                .selector = "stash@{0}",
                .commitSha = "1111",
                .dateIso = "2026-11-07T12:00:00Z",
                .message = "Stash #1",
                .branchName = "main"
            }
        };

        gf::git::bl::GitStashService stashService(mockOps);
        StashManagerViewModel vm(stashService);

        auto tempDir = std::filesystem::temp_directory_path();
        vm.setRepoPath(QString::fromStdString(tempDir.string()));
        CHECK(vm.repoPath().toStdString() == tempDir.string());

        QSignalSpy opSpy(&vm, &StashManagerViewModel::operationSucceeded);

        // Refresh populates stashesModel
        vm.refresh();
        CHECK(vm.stashesModel()->rowCount() == 1);

        // Push
        bool pushed = vm.push("New stash message", true, false);
        CHECK(pushed);
        CHECK(mockOps.stashPushCalled);
        CHECK(opSpy.count() == 1);
        CHECK(opSpy.takeFirst().at(0).toString() == "push");

        // Apply
        bool applied = vm.apply(0, false);
        CHECK(applied);
        CHECK(mockOps.stashApplyCalled);
        CHECK(opSpy.count() == 1);
        CHECK(opSpy.takeFirst().at(0).toString() == "apply");

        // Pop
        bool popped = vm.pop(0, false);
        CHECK(popped);
        CHECK(mockOps.stashPopCalled);
        CHECK(opSpy.count() == 1);
        CHECK(opSpy.takeFirst().at(0).toString() == "pop");

        // Drop
        bool dropped = vm.drop(0);
        CHECK(dropped);
        CHECK(mockOps.stashDropCalled);
        CHECK(opSpy.count() == 1);
        CHECK(opSpy.takeFirst().at(0).toString() == "drop");

        // Clear
        bool cleared = vm.clear();
        CHECK(cleared);
        CHECK(mockOps.stashClearCalled);
        CHECK(opSpy.count() == 1);
        CHECK(opSpy.takeFirst().at(0).toString() == "clear");
    }
}

TEST_CASE("ConflictHunkListModel and ConflictResolverViewModel 3-way conflict resolution", "[viewmodel][conflict]") {
    ensureQtApp();

    SECTION("ConflictHunkListModel tracks resolution state") {
        ConflictHunkListModel model;
        CHECK(model.rowCount() == 0);
        CHECK(model.allResolved());

        std::vector<gf::git::bo::ConflictHunk> hunks = {
            gf::git::bo::ConflictHunk{
                .hunkIndex = 0,
                .startLine = 10,
                .baseContent = "int x = 0;\n",
                .oursContent = "int x = 1;\n",
                .theirsContent = "int x = 2;\n",
                .chosenSide = gf::git::bo::ConflictSide::Ours,
                .customContent = {},
                .isResolved = false
            },
            gf::git::bo::ConflictHunk{
                .hunkIndex = 1,
                .startLine = 25,
                .baseContent = "return true;\n",
                .oursContent = "return ok;\n",
                .theirsContent = "return valid;\n",
                .chosenSide = gf::git::bo::ConflictSide::Theirs,
                .customContent = {},
                .isResolved = false
            }
        };

        model.setHunks(hunks);
        REQUIRE(model.rowCount() == 2);
        CHECK(model.resolvedCount() == 0);
        CHECK_FALSE(model.allResolved());

        // Resolve first hunk with Ours
        model.resolveHunk(0, gf::git::bo::ConflictSide::Ours);
        CHECK(model.resolvedCount() == 1);
        CHECK_FALSE(model.allResolved());

        auto idx0 = model.index(0);
        CHECK(model.data(idx0, ConflictHunkListModel::IsResolvedRole).toBool() == true);
        CHECK(model.data(idx0, ConflictHunkListModel::ChosenSideRole).toInt() == static_cast<int>(gf::git::bo::ConflictSide::Ours));
        CHECK(model.data(idx0, ConflictHunkListModel::ResolvedPreviewRole).toString() == "int x = 1;\n");

        // Resolve second hunk with Custom text
        model.resolveHunk(1, gf::git::bo::ConflictSide::Custom, "return ok && valid;\n");
        CHECK(model.resolvedCount() == 2);
        CHECK(model.allResolved());

        auto idx1 = model.index(1);
        CHECK(model.data(idx1, ConflictHunkListModel::IsResolvedRole).toBool() == true);
        CHECK(model.data(idx1, ConflictHunkListModel::ChosenSideRole).toInt() == static_cast<int>(gf::git::bo::ConflictSide::Custom));
        CHECK(model.data(idx1, ConflictHunkListModel::CustomContentRole).toString() == "return ok && valid;\n");
        CHECK(model.data(idx1, ConflictHunkListModel::ResolvedPreviewRole).toString() == "return ok && valid;\n");

        model.clear();
        CHECK(model.rowCount() == 0);
    }

    SECTION("ConflictResolverViewModel loads file, manages resolution and applies") {
        MockFullGitOps mockOps;
        mockOps.conflictFileToReturn = gf::git::bo::ConflictFile{
            .filePath = "src/main.cpp",
            .isBinary = false,
            .hunks = {
                gf::git::bo::ConflictHunk{
                    .hunkIndex = 0,
                    .startLine = 5,
                    .baseContent = "base",
                    .oursContent = "ours",
                    .theirsContent = "theirs",
                    .chosenSide = gf::git::bo::ConflictSide::Ours,
                    .customContent = {},
                    .isResolved = false
                }
            },
            .fullContentWithMarkers = "<<<<<<< HEAD\nours\n=======\ntheirs\n>>>>>>> feature\n"
        };

        gf::git::bl::GitConflictService conflictService(mockOps);
        ConflictResolverViewModel vm(conflictService);

        auto tempDir = std::filesystem::temp_directory_path();
        vm.setRepoPath(QString::fromStdString(tempDir.string()));

        QSignalSpy resAppliedSpy(&vm, &ConflictResolverViewModel::resolutionApplied);

        // Load conflict file
        bool loaded = vm.loadFile("src/main.cpp");
        CHECK(loaded);
        CHECK(vm.currentFilePath() == "src/main.cpp");
        CHECK(vm.totalHunks() == 1);
        CHECK(vm.resolvedHunks() == 0);
        CHECK_FALSE(vm.allResolved());

        // Resolve hunk 0 with Theirs (side = 2)
        vm.resolveHunk(0, 2);
        CHECK(vm.resolvedHunks() == 1);
        CHECK(vm.allResolved());

        // Apply resolution
        bool applied = vm.applyResolution(true);
        CHECK(applied);
        CHECK(mockOps.applyConflictResolutionCalled);
        CHECK(resAppliedSpy.count() == 1);
        CHECK(resAppliedSpy.takeFirst().at(0).toString() == "src/main.cpp");
    }
}

TEST_CASE("WorkspaceSessionViewModel Rebase operations and state tracking", "[viewmodel][session][rebase]") {
    ensureQtApp();

    TestContext ctx;
    auto r1Res = ctx.repoService->registerRepository({
        .localPath = ctx.repoAPath,
        .displayName = "Repo Rebase"
    });
    REQUIRE(r1Res.has_value());

    MockFullGitOps mockOps;
    gf::git::bl::GitWorkingTreeService statusService(mockOps);
    gf::git::bl::GitStagingService stagingService(mockOps);
    gf::git::bl::GitDiffService diffService(mockOps);
    gf::git::bl::GitBranchService branchService(mockOps);
    gf::git::bl::GitHistoryService historyService(mockOps);
    gf::git::bl::GitRemoteMergeService remoteMergeService(mockOps);
    gf::git::bl::GitStashService stashService(mockOps);
    gf::git::bl::GitRebaseService rebaseService(mockOps);
    gf::git::bl::GitConflictService conflictService(mockOps);

    WorkspaceSessionViewModel sessionVm(
        *ctx.repoService,
        *ctx.workspaceService,
        statusService,
        stagingService,
        diffService,
        branchService,
        historyService,
        remoteMergeService,
        stashService,
        rebaseService,
        conflictService
    );

    sessionVm.openRepository(QString::fromStdString(r1Res->id.value()), "Repo Rebase", QString::fromStdString(ctx.repoAPath.string()));
    CHECK(sessionVm.hasOpenTabs());

    CHECK_FALSE(sessionVm.isRebaseInProgress());

    // Start rebase
    QSignalSpy rebaseSucceededSpy(&sessionVm, &WorkspaceSessionViewModel::rebaseSucceeded);
    bool rebaseStarted = sessionVm.startRebase("main");
    CHECK(rebaseStarted);
    CHECK(mockOps.startRebaseCalled);
    CHECK(rebaseSucceededSpy.count() == 1);

    // Continue rebase
    bool cont = sessionVm.continueRebase();
    CHECK(cont);
    CHECK(mockOps.continueRebaseCalled);

    // Skip rebase
    bool skipped = sessionVm.skipRebase();
    CHECK(skipped);
    CHECK(mockOps.skipRebaseCalled);

    // Abort rebase
    bool aborted = sessionVm.abortRebase();
    CHECK(aborted);
    CHECK(mockOps.abortRebaseCalled);

    // Sub-viewmodels accessors
    CHECK(sessionVm.stashManager() != nullptr);
    CHECK(sessionVm.conflictResolver() != nullptr);
    CHECK(sessionVm.workingTree()->conflictedModel() != nullptr);
}

class MockFsWatcherPort final : public gf::git::ports::IFsWatcherPort {
public:
    std::set<std::filesystem::path> watchedPaths;
    gf::git::ports::FsRawEventCallback eventCb;

    std::expected<void, gf::git::ports::FsWatcherError> watchRepository(const std::filesystem::path& repoPath) override {
        watchedPaths.insert(repoPath);
        return {};
    }

    std::expected<void, gf::git::ports::FsWatcherError> unwatchRepository(const std::filesystem::path& repoPath) override {
        watchedPaths.erase(repoPath);
        return {};
    }

    bool isWatching(const std::filesystem::path& repoPath) const override {
        return watchedPaths.contains(repoPath);
    }

    void setEventCallback(gf::git::ports::FsRawEventCallback cb) override {
        eventCb = std::move(cb);
    }
};

TEST_CASE("WorkspaceSessionViewModel - Watcher Integration & Auto-Refresh", "[desktop][viewmodels][watcher]") {
    TestContext ctx;
    auto r1Res = ctx.repoService->registerRepository({
        .localPath = ctx.repoAPath,
        .displayName = "Repo Auto-Refresh"
    });
    REQUIRE(r1Res.has_value());

    MockFullGitOps mockOps;
    gf::git::bl::GitWorkingTreeService statusService(mockOps);
    gf::git::bl::GitStagingService stagingService(mockOps);
    gf::git::bl::GitDiffService diffService(mockOps);
    gf::git::bl::GitBranchService branchService(mockOps);
    gf::git::bl::GitHistoryService historyService(mockOps);
    gf::git::bl::GitRemoteMergeService remoteMergeService(mockOps);
    gf::git::bl::GitStashService stashService(mockOps);
    gf::git::bl::GitRebaseService rebaseService(mockOps);
    gf::git::bl::GitConflictService conflictService(mockOps);

    MockFsWatcherPort watcherPort;
    gf::git::bl::GitRepositoryWatcherService watcherService(watcherPort);

    WorkspaceSessionViewModel sessionVm(
        *ctx.repoService,
        *ctx.workspaceService,
        statusService,
        stagingService,
        diffService,
        branchService,
        historyService,
        remoteMergeService,
        stashService,
        rebaseService,
        conflictService,
        &watcherService
    );

    // Opening repository starts watching
    sessionVm.openRepository(QString::fromStdString(r1Res->id.value()), "Repo Auto-Refresh", QString::fromStdString(ctx.repoAPath.string()));
    CHECK(sessionVm.hasOpenTabs());
    CHECK(watcherPort.isWatching(ctx.repoAPath));

    // Simulate an event arriving from file system
    gf::git::bo::FsRawEvent event{
        .repositoryPath = ctx.repoAPath,
        .changedPath = ctx.repoAPath / ".git" / "HEAD",
        .changeType = gf::git::bo::FsChangeType::Modified
    };
    watcherService.onRawEvent(event);
    watcherService.flushAll();

    // Process queued Qt events (QMetaObject::invokeMethod)
    QCoreApplication::processEvents();

    CHECK(mockOps.getStatusCalled);

    // Closing repository tab stops watching
    sessionVm.closeTab(0);
    CHECK_FALSE(watcherPort.isWatching(ctx.repoAPath));
}

TEST_CASE("BatchOperationsViewModel - Batch Execution & Status Reporting", "[desktop][viewmodels][batch]") {
    TestContext ctx;
    auto r1Res = ctx.repoService->registerRepository({
        .localPath = ctx.repoAPath,
        .displayName = "Repo Batch 1"
    });
    REQUIRE(r1Res.has_value());

    auto r2Res = ctx.repoService->registerRepository({
        .localPath = ctx.repoBPath,
        .displayName = "Repo Batch 2"
    });
    REQUIRE(r2Res.has_value());

    auto gRes = ctx.groupService->createGroup("Batch Group", {r1Res->id, r2Res->id});
    REQUIRE(gRes.has_value());

    MockFullGitOps mockOps;
    gf::git::bl::GitWorkingTreeService statusService(mockOps);
    gf::git::bl::GitStagingService stagingService(mockOps);
    gf::git::bl::GitDiffService diffService(mockOps);
    gf::git::bl::GitBranchService branchService(mockOps);
    gf::git::bl::GitHistoryService historyService(mockOps);
    gf::git::bl::GitRemoteMergeService remoteMergeService(mockOps);
    gf::git::bl::GitStashService stashService(mockOps);
    gf::git::bl::GitRebaseService rebaseService(mockOps);
    gf::git::bl::GitConflictService conflictService(mockOps);

    gf::tasks::bl::BackgroundTaskRunner runner(2);
    gf::git::bl::MultiRepoBatchCoordinator coordinator(
        *ctx.repoService,
        *ctx.groupService,
        *ctx.workspaceService,
        remoteMergeService,
        statusService,
        runner
    );

    BatchOperationsViewModel batchVm(coordinator);

    CHECK_FALSE(batchVm.isRunning());
    CHECK(batchVm.progressPercentage() == 0);
    CHECK(batchVm.succeededCount() == 0);
    CHECK(batchVm.failedCount() == 0);

    QSignalSpy completedSpy(&batchVm, &BatchOperationsViewModel::operationCompleted);
    QSignalSpy isRunningSpy(&batchVm, &BatchOperationsViewModel::isRunningChanged);

    // 1. Fetch Group
    batchVm.fetchGroup(QString::fromStdString(gRes->id.value()), "origin");
    CHECK(batchVm.isRunning());
    CHECK(batchVm.currentOperationName() == "Fetch Gruppo");

    // Wait for completion (with timeout)
    for (int i = 0; i < 50 && batchVm.isRunning(); ++i) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    CHECK_FALSE(batchVm.isRunning());
    CHECK(batchVm.progressPercentage() == 100);
    CHECK(batchVm.succeededCount() == 2);
    CHECK(batchVm.failedCount() == 0);
    CHECK(batchVm.lastReportItems().size() == 2);
    CHECK(completedSpy.count() >= 1);

    // 2. Status Repositories
    QStringList statusList;
    statusList << QString::fromStdString(r1Res->id.value())
               << QString::fromStdString(r2Res->id.value());
    batchVm.checkStatusRepositories(statusList);

    for (int i = 0; i < 50 && batchVm.isRunning(); ++i) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    CHECK_FALSE(batchVm.isRunning());
    CHECK(batchVm.succeededCount() == 2);

    // 3. Cancel batch operation test
    QStringList fetchList;
    fetchList << QString::fromStdString(r1Res->id.value())
              << QString::fromStdString(r2Res->id.value());
    batchVm.fetchRepositories(fetchList, "origin");

    if (batchVm.isRunning()) {
        bool canceled = batchVm.cancelCurrentOperation();
        CHECK(canceled);
    }
}

TEST_CASE("DiffViewModel - View mode switching and property invariants", "[desktop][viewmodels]") {
    MockFullGitOps mockOps;
    gf::git::bl::GitDiffService diffService(mockOps);

    DiffViewModel diffVm(diffService);

    CHECK(diffVm.viewMode() == "split");
    CHECK(diffVm.isEmpty());
    CHECK_FALSE(diffVm.isBinary());
    CHECK(diffVm.splitLines().isEmpty());
    CHECK(diffVm.hunks().isEmpty());
    CHECK(diffVm.blameLines().isEmpty());

    QSignalSpy modeSpy(&diffVm, &DiffViewModel::viewModeChanged);

    // Switch to inline
    diffVm.setViewMode("inline");
    CHECK(diffVm.viewMode() == "inline");
    CHECK(modeSpy.count() == 1);

    // Redundant switch should not re-emit
    diffVm.setViewMode("inline");
    CHECK(modeSpy.count() == 1);

    // Switch to hunk
    diffVm.setViewMode("hunk");
    CHECK(diffVm.viewMode() == "hunk");
    CHECK(modeSpy.count() == 2);

    // Switch to blame
    diffVm.setViewMode("blame");
    CHECK(diffVm.viewMode() == "blame");
    CHECK(modeSpy.count() == 3);

    // Switch back to split
    diffVm.setViewMode("split");
    CHECK(diffVm.viewMode() == "split");
    CHECK(modeSpy.count() == 4);
}

