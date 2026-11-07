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
#include "desktop/viewmodels/workspace_session_viewmodel.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/modules/git/bl/git_diff_service.hpp"

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

class MockFullGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    gf::git::bo::RepoWorkingTreeStatus statusToReturn;
    gf::git::bo::FileDiff diffToReturn;
    bool stageCalled = false;
    bool unstageCalled = false;
    bool discardCalled = false;
    bool commitCalled = false;

    std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
    getWorkingTreeStatus(const std::filesystem::path&) override {
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
        // 1 hunk header + 2 righe = 3
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

        // Al termine del commit il messaggio viene resettato
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

    WorkspaceSessionViewModel sessionVm(
        *ctx.repoService,
        *ctx.workspaceService,
        statusService,
        stagingService,
        diffService
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
}
