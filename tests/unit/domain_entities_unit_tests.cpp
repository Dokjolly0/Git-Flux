#include <catch2/catch_test_macros.hpp>

#include "core/modules/groups/bo/group.hpp"
#include "core/modules/groups/bo/group_id.hpp"
#include "core/modules/repositories/bo/repository.hpp"
#include "core/modules/repositories/bo/repository_id.hpp"
#include "core/modules/workspaces/bo/workspace.hpp"
#include "core/modules/workspaces/bo/workspace_id.hpp"
#include "core/modules/workspaces/bo/workspace_session.hpp"

#include <type_traits>

TEST_CASE (
"StrongIds hanno type safety assoluta e non sono intercambiabili"
,
"[unit][domain][ids]"
)
 {
    using gf::groups::bo::GroupId;
    using gf::repositories::bo::RepositoryId;
    using gf::workspaces::bo::WorkspaceId;

    STATIC_REQUIRE_FALSE(std::is_same_v<RepositoryId, GroupId>);
    STATIC_REQUIRE_FALSE(std::is_same_v<RepositoryId, WorkspaceId>);
    STATIC_REQUIRE_FALSE(std::is_same_v<GroupId, WorkspaceId>);

    RepositoryId repoId("id-common");
    GroupId groupId("id-common");
    WorkspaceId workspaceId("id-common");

    REQUIRE(repoId.value() == "id-common");
    REQUIRE(groupId.value() == "id-common");
    REQUIRE(workspaceId.value() == "id-common");
}

TEST_CASE (
"Repository entity verifica campi obbligatori e comparazioni"
,
"[unit][domain][repository]"
)
 {
    using gf::repositories::bo::Repository;
    using gf::repositories::bo::RepositoryId;
    using gf::repositories::bo::RepositoryPathStatus;

    Repository validRepo{
        .id = RepositoryId("repo-1"),
        .displayName = "Frontend Client",
        .localPath = "/projects/alpha/frontend",
        .pathStatus = RepositoryPathStatus::Valid,
        .registeredAtUnixEpochMs = 1700000000000,
        .lastAccessedAtUnixEpochMs = 1700000050000
    };
    REQUIRE(validRepo.isValid());
    REQUIRE(validRepo.pathStatus == RepositoryPathStatus::Valid);

    Repository missingId{
        .id = RepositoryId(""),
        .displayName = "Frontend Client",
        .localPath = "/projects/alpha/frontend"
    };
    REQUIRE_FALSE(missingId.isValid());

    Repository missingName{
        .id = RepositoryId("repo-1"),
        .displayName = "",
        .localPath = "/projects/alpha/frontend"
    };
    REQUIRE_FALSE(missingName.isValid());

    Repository missingPath{
        .id = RepositoryId("repo-1"),
        .displayName = "Frontend Client",
        .localPath = {}
    };
    REQUIRE_FALSE(missingPath.isValid());

    Repository copy = validRepo;
    REQUIRE(copy == validRepo);
}

TEST_CASE (
"Group entity gestisce repository ordinati e membership"
,
"[unit][domain][group]"
)
 {
    using gf::groups::bo::Group;
    using gf::groups::bo::GroupId;
    using gf::repositories::bo::RepositoryId;

    Group group{
        .id = GroupId("grp-alpha"),
        .name = "Cliente Alpha",
        .repositoryIds = {
            RepositoryId("repo-frontend"),
            RepositoryId("repo-backend"),
            RepositoryId("repo-infra")
        },
        .createdAtUnixEpochMs = 1000,
        .updatedAtUnixEpochMs = 1000
    };

    REQUIRE(group.isValid());
    REQUIRE(group.repositoryIds.size() == 3);
    REQUIRE(group.contains(RepositoryId("repo-backend")));
    REQUIRE_FALSE(group.contains(RepositoryId("repo-portfolio")));

    // Repository ordering is preserved
    REQUIRE(group.repositoryIds[0] == RepositoryId("repo-frontend"));
    REQUIRE(group.repositoryIds[1] == RepositoryId("repo-backend"));
    REQUIRE(group.repositoryIds[2] == RepositoryId("repo-infra"));

    Group invalidGroup{
        .id = GroupId(""),
        .name = "No ID",
        .repositoryIds = {},
        .createdAtUnixEpochMs = 0,
        .updatedAtUnixEpochMs = 0
    };
    REQUIRE_FALSE(invalidGroup.isValid());
}

TEST_CASE (
"Workspace entity gestisce gruppi inclusi e repository diretti"
,
"[unit][domain][workspace]"
)
 {
    using gf::groups::bo::GroupId;
    using gf::repositories::bo::RepositoryId;
    using gf::workspaces::bo::Workspace;
    using gf::workspaces::bo::WorkspaceId;

    Workspace ws{
        .id = WorkspaceId("ws-main"),
        .name = "Lavoro Alpha",
        .groupIds = {GroupId("grp-client-alpha"), GroupId("grp-frontend")},
        .directRepositoryIds = {RepositoryId("repo-portfolio")},
        .createdAtUnixEpochMs = 2000,
        .updatedAtUnixEpochMs = 2000
    };

    REQUIRE(ws.isValid());
    REQUIRE(ws.containsGroup(GroupId("grp-client-alpha")));
    REQUIRE_FALSE(ws.containsGroup(GroupId("grp-personal")));
    REQUIRE(ws.containsDirectRepository(RepositoryId("repo-portfolio")));
    REQUIRE_FALSE(ws.containsDirectRepository(RepositoryId("repo-backend")));
}

TEST_CASE (
"WorkspaceSession memorizza stato pannelli, tab aperte e repo attivo"
,
"[unit][domain][session]"
)
 {
    using gf::repositories::bo::RepositoryId;
    using gf::workspaces::bo::PanelLayoutState;
    using gf::workspaces::bo::WorkspaceId;
    using gf::workspaces::bo::WorkspaceSession;

    PanelLayoutState layoutState;
    layoutState.leftSidebarCollapsed = false;
    layoutState.rightSidebarCollapsed = true;
    layoutState.leftSidebarWidth = 280.0;
    layoutState.rightSidebarWidth = 350.0;
    layoutState.activeView = "changes";

    WorkspaceSession session{
        .workspaceId = WorkspaceId("ws-main"),
        .schemaVersion = 1,
        .openedRepositoryIds = {
            RepositoryId("repo-frontend"),
            RepositoryId("repo-backend")
        },
        .activeRepositoryId = RepositoryId("repo-frontend"),
        .layout = layoutState,
        .customStateJson = R"({"branchFilter":"main"})",
        .updatedAtUnixEpochMs = 3000
    };

    REQUIRE(session.isValid());
    REQUIRE(session.openedRepositoryIds.size() == 2);
    REQUIRE(session.activeRepositoryId == RepositoryId("repo-frontend"));
    REQUIRE(session.layout.activeView == "changes");
    REQUIRE(session.layout.rightSidebarCollapsed);
    REQUIRE_FALSE(session.layout.leftSidebarCollapsed);

    WorkspaceSession invalidSession;
    invalidSession.workspaceId = WorkspaceId("");
    invalidSession.schemaVersion = 1;
    REQUIRE_FALSE(invalidSession.isValid());

    WorkspaceSession invalidVersionSession;
    invalidVersionSession.workspaceId = WorkspaceId("ws-1");
    invalidVersionSession.schemaVersion = 0;
    REQUIRE_FALSE(invalidVersionSession.isValid());
}
