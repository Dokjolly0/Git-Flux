#include "core/modules/workspaces/bl/workspace_service.hpp"
#include "core/shared/id_generator.hpp"

#include <algorithm>
#include <chrono>
#include <unordered_set>

namespace gf::workspaces::bl
{
    using bo::PanelLayoutState;
    using bo::Workspace;
    using bo::WorkspaceId;
    using bo::WorkspaceSession;
    using groups::bo::GroupId;
    using repositories::bo::Repository;
    using repositories::bo::RepositoryId;

    namespace
    {
        [[nodiscard]] std::int64_t currentUnixMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                .count();
        }
    } // namespace

    WorkspaceService::WorkspaceService(ports::IWorkspaceStore& wsStore,
                                       groups::ports::IGroupStore& groupStore,
                                       repositories::ports::IRepositoryStore& repoStore) noexcept
        : wsStore_(wsStore)
          , groupStore_(groupStore)
          , repoStore_(repoStore)
    {
    }

    std::expected<Workspace, WorkspaceServiceError>
    WorkspaceService::createWorkspace(std::string name,
                                      std::vector<GroupId> groupIds,
                                      std::vector<RepositoryId> directRepoIds) const
    {
        if (name.empty())
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::InvalidName,
                .message = "Il nome del workspace non può essere vuoto"
            });
        }

        // Validate included groups
        for (const auto& gId : groupIds)
        {
            if (const auto g = groupStore_.getById(gId); !g || !g->has_value())
            {
                return std::unexpected(WorkspaceServiceError{
                    .kind = WorkspaceServiceError::Kind::GroupNotFound,
                    .message = "Gruppo non trovato: " + gId.value()
                });
            }
        }

        // Validate direct repositories
        for (const auto& rId : directRepoIds)
        {
            if (const auto r = repoStore_.getById(rId); !r || !r->has_value())
            {
                return std::unexpected(WorkspaceServiceError{
                    .kind = WorkspaceServiceError::Kind::RepositoryNotFound,
                    .message = "Repository diretto non trovato nel catalogo: " + rId.value()
                });
            }
        }

        const auto now = currentUnixMs();
        Workspace ws{
            .id = WorkspaceId(core::generateUniqueId("ws")),
            .name = std::move(name),
            .groupIds = std::move(groupIds),
            .directRepositoryIds = std::move(directRepoIds),
            .createdAtUnixEpochMs = now,
            .updatedAtUnixEpochMs = now
        };

        if (const auto saveRes = wsStore_.save(ws); !saveRes)
        {
            if (saveRes.error().kind == ports::WorkspaceStoreError::Kind::DuplicateName)
            {
                return std::unexpected(WorkspaceServiceError{
                    .kind = WorkspaceServiceError::Kind::DuplicateName,
                    .message = "Esiste già un workspace con questo nome"
                });
            }
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }

        // Resolve actual repositories and initialize the initial session
        std::vector<RepositoryId> openTabs;
        std::optional<RepositoryId> activeRepo;
        if (const auto reposRes = resolveWorkspaceRepositories(ws.id); reposRes && !reposRes->empty())
        {
            openTabs.push_back(reposRes->front().id);
            activeRepo = reposRes->front().id;
        }

        const WorkspaceSession initialSession{
            .workspaceId = ws.id,
            .schemaVersion = 1,
            .openedRepositoryIds = std::move(openTabs),
            .activeRepositoryId = activeRepo,
            .layout = PanelLayoutState{},
            .customStateJson = "{}",
            .updatedAtUnixEpochMs = now
        };
        if (const auto sessionSaveRes = wsStore_.saveSession(initialSession); !sessionSaveRes)
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::StorageError,
                .message = sessionSaveRes.error().message
            });
        }

        return ws;
    }

    std::expected<Workspace, WorkspaceServiceError>
    WorkspaceService::updateWorkspaceComposition(const WorkspaceId& id,
                                                 std::vector<GroupId> groupIds,
                                                 std::vector<RepositoryId> directRepoIds) const
    {
        auto wsRes = wsStore_.getById(id);
        if (!wsRes || !wsRes->has_value())
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::NotFound,
                .message = "Workspace non trovato"
            });
        }

        // Validate groups
        for (const auto& gId : groupIds)
        {
            if (const auto g = groupStore_.getById(gId); !g || !g->has_value())
            {
                return std::unexpected(WorkspaceServiceError{
                    .kind = WorkspaceServiceError::Kind::GroupNotFound,
                    .message = "Gruppo non trovato: " + gId.value()
                });
            }
        }

        // Validate direct repositories
        for (const auto& rId : directRepoIds)
        {
            if (const auto r = repoStore_.getById(rId); !r || !r->has_value())
            {
                return std::unexpected(WorkspaceServiceError{
                    .kind = WorkspaceServiceError::Kind::RepositoryNotFound,
                    .message = "Repository non trovato: " + rId.value()
                });
            }
        }

        auto ws = std::move(**wsRes);
        ws.groupIds = std::move(groupIds);
        ws.directRepositoryIds = std::move(directRepoIds);
        ws.updatedAtUnixEpochMs = currentUnixMs();

        if (const auto saveRes = wsStore_.save(ws); !saveRes)
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }

        return ws;
    }

    std::expected<bool, WorkspaceServiceError>
    WorkspaceService::deleteWorkspace(const WorkspaceId& id) const
    {
        auto res = wsStore_.remove(id);
        if (!res)
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return *res;
    }

    std::expected<std::optional<Workspace>, WorkspaceServiceError>
    WorkspaceService::getWorkspace(const WorkspaceId& id) const
    {
        auto res = wsStore_.getById(id);
        if (!res)
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return std::move(*res);
    }

    std::expected<std::vector<Workspace>, WorkspaceServiceError>
    WorkspaceService::getAllWorkspaces() const
    {
        auto res = wsStore_.getAll();
        if (!res)
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return std::move(*res);
    }

    std::expected<std::vector<Repository>, WorkspaceServiceError>
    WorkspaceService::resolveWorkspaceRepositories(const WorkspaceId& id) const
    {
        auto wsRes = wsStore_.getById(id);
        if (!wsRes || !wsRes->has_value())
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::NotFound,
                .message = "Workspace non trovato: " + id.value()
            });
        }

        const auto& ws = **wsRes;
        std::unordered_set<RepositoryId> seenIds;
        std::vector<Repository> resolved;

        // 1. Repository dai gruppi inclusi (in ordine)
        for (const auto& gId : ws.groupIds)
        {
            auto gRes = groupStore_.getById(gId);
            if (!gRes || !gRes->has_value())
            {
                continue;
            }
            for (const auto& rId : (*gRes)->repositoryIds)
            {
                if (seenIds.insert(rId).second)
                {
                    if (auto repoRes = repoStore_.getById(rId); repoRes && repoRes->has_value())
                    {
                        resolved.push_back(std::move(**repoRes));
                    }
                }
            }
        }

        // 2. Direct repositories of the workspace
        for (const auto& rId : ws.directRepositoryIds)
        {
            if (seenIds.insert(rId).second)
            {
                if (auto repoRes = repoStore_.getById(rId); repoRes && repoRes->has_value())
                {
                    resolved.push_back(std::move(**repoRes));
                }
            }
        }

        return resolved;
    }

    std::expected<WorkspaceSession, WorkspaceServiceError>
    WorkspaceService::loadOrCreateSession(const WorkspaceId& id) const
    {
        if (auto existing = wsStore_.getSession(id); existing && existing->has_value())
        {
            return std::move(**existing);
        }

        // If it does not exist yet, create a default one
        std::vector<RepositoryId> openTabs;
        std::optional<RepositoryId> activeRepo;
        if (const auto reposRes = resolveWorkspaceRepositories(id); reposRes && !reposRes->empty())
        {
            openTabs.push_back(reposRes->front().id);
            activeRepo = reposRes->front().id;
        }

        WorkspaceSession newSession{
            .workspaceId = id,
            .schemaVersion = 1,
            .openedRepositoryIds = std::move(openTabs),
            .activeRepositoryId = activeRepo,
            .layout = PanelLayoutState{},
            .customStateJson = "{}",
            .updatedAtUnixEpochMs = currentUnixMs()
        };
        if (const auto saveRes = wsStore_.saveSession(newSession); !saveRes)
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }
        return newSession;
    }

    std::expected<void, WorkspaceServiceError>
    WorkspaceService::saveSession(const WorkspaceSession& session) const
    {
        if (const auto saveRes = wsStore_.saveSession(session); !saveRes)
        {
            return std::unexpected(WorkspaceServiceError{
                .kind = WorkspaceServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }
        return {};
    }
} // namespace gf::workspaces::bl
