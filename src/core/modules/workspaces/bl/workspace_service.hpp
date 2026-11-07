#pragma once

#include "core/modules/groups/ports/group_store.hpp"
#include "core/modules/repositories/bo/repository.hpp"
#include "core/modules/repositories/ports/repository_store.hpp"
#include "core/modules/workspaces/bo/workspace.hpp"
#include "core/modules/workspaces/bo/workspace_session.hpp"
#include "core/modules/workspaces/ports/workspace_store.hpp"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace gf::workspaces::bl {

struct WorkspaceServiceError {
    enum class Kind {
        NotFound,
        DuplicateName,
        GroupNotFound,
        RepositoryNotFound,
        InvalidName,
        StorageError
    };
    Kind kind = Kind::StorageError;
    std::string message;
};

class WorkspaceService {
public:
    WorkspaceService(ports::IWorkspaceStore& wsStore,
                     groups::ports::IGroupStore& groupStore,
                     repositories::ports::IRepositoryStore& repoStore) noexcept;

    [[nodiscard]] std::expected<bo::Workspace, WorkspaceServiceError>
    createWorkspace(std::string name,
                    std::vector<groups::bo::GroupId> groupIds = {},
                    std::vector<repositories::bo::RepositoryId> directRepoIds = {});

    [[nodiscard]] std::expected<bo::Workspace, WorkspaceServiceError>
    updateWorkspaceComposition(const bo::WorkspaceId& id,
                               std::vector<groups::bo::GroupId> groupIds,
                               std::vector<repositories::bo::RepositoryId> directRepoIds);

    [[nodiscard]] std::expected<bool, WorkspaceServiceError>
    deleteWorkspace(const bo::WorkspaceId& id);

    [[nodiscard]] std::expected<std::optional<bo::Workspace>, WorkspaceServiceError>
    getWorkspace(const bo::WorkspaceId& id);

    [[nodiscard]] std::expected<std::vector<bo::Workspace>, WorkspaceServiceError>
    getAllWorkspaces();

    [[nodiscard]] std::expected<std::vector<repositories::bo::Repository>, WorkspaceServiceError>
    resolveWorkspaceRepositories(const bo::WorkspaceId& id);

    [[nodiscard]] std::expected<bo::WorkspaceSession, WorkspaceServiceError>
    loadOrCreateSession(const bo::WorkspaceId& id);

    [[nodiscard]] std::expected<void, WorkspaceServiceError>
    saveSession(const bo::WorkspaceSession& session);

private:
    ports::IWorkspaceStore& wsStore_;
    groups::ports::IGroupStore& groupStore_;
    repositories::ports::IRepositoryStore& repoStore_;
};

} // namespace gf::workspaces::bl
