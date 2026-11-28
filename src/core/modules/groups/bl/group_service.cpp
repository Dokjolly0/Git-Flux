#include "core/modules/groups/bl/group_service.hpp"
#include "core/shared/id_generator.hpp"
#include "core/shared/time_utils.hpp"

#include <algorithm>
#include <unordered_set>

namespace gf::groups::bl
{
    using bo::Group;
    using bo::GroupId;
    using repositories::bo::Repository;
    using repositories::bo::RepositoryId;
    using core::currentUnixMs;

    GroupService::GroupService(ports::IGroupStore& groupStore,
                               repositories::ports::IRepositoryStore& repoStore) noexcept
        : groupStore_(groupStore)
          , repoStore_(repoStore)
    {
    }

    std::expected<Group, GroupServiceError>
    GroupService::createGroup(std::string name, std::vector<RepositoryId> repoIds, std::string color) const
    {
        if (name.empty())
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::InvalidName,
                .message = "Il nome del gruppo non può essere vuoto"
            });
        }

        if (!bo::isValidGroupColor(color))
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::InvalidColor,
                .message = "Colore gruppo non valido (atteso formato #RRGGBB)"
            });
        }

        // Verify existence of all provided repositories
        for (const auto& rId : repoIds)
        {
            if (const auto r = repoStore_.getById(rId); !r || !r->has_value())
            {
                return std::unexpected(GroupServiceError{
                    .kind = GroupServiceError::Kind::RepositoryNotFound,
                    .message = "Repository non trovato nel catalogo: " + rId.value()
                });
            }
        }

        const auto now = currentUnixMs();
        Group g{
            .id = GroupId(core::generateUniqueId("grp")),
            .name = std::move(name),
            .color = std::move(color),
            .repositoryIds = std::move(repoIds),
            .createdAtUnixEpochMs = now,
            .updatedAtUnixEpochMs = now
        };

        if (const auto saveRes = groupStore_.save(g); !saveRes)
        {
            if (saveRes.error().kind == ports::GroupStoreError::Kind::DuplicateName)
            {
                return std::unexpected(GroupServiceError{
                    .kind = GroupServiceError::Kind::DuplicateName,
                    .message = "Esiste già un gruppo con questo nome"
                });
            }
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }

        return g;
    }

    std::expected<Group, GroupServiceError>
    GroupService::renameGroup(const GroupId& id, std::string newName) const
    {
        if (newName.empty())
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::InvalidName,
                .message = "Il nuovo nome non può essere vuoto"
            });
        }

        auto existing = groupStore_.getById(id);
        if (!existing || !existing->has_value())
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::NotFound,
                .message = "Gruppo non trovato: " + id.value()
            });
        }

        auto g = std::move(**existing);
        g.name = std::move(newName);
        g.updatedAtUnixEpochMs = currentUnixMs();

        if (const auto saveRes = groupStore_.save(g); !saveRes)
        {
            if (saveRes.error().kind == ports::GroupStoreError::Kind::DuplicateName)
            {
                return std::unexpected(GroupServiceError{
                    .kind = GroupServiceError::Kind::DuplicateName,
                    .message = "Esiste già un gruppo con questo nome"
                });
            }
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }

        return g;
    }

    std::expected<Group, GroupServiceError>
    GroupService::updateGroupColor(const GroupId& id, std::string color) const
    {
        if (!bo::isValidGroupColor(color))
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::InvalidColor,
                .message = "Colore gruppo non valido (atteso formato #RRGGBB)"
            });
        }

        auto existing = groupStore_.getById(id);
        if (!existing || !existing->has_value())
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::NotFound,
                .message = "Gruppo non trovato: " + id.value()
            });
        }

        auto g = std::move(**existing);
        g.color = std::move(color);
        g.updatedAtUnixEpochMs = currentUnixMs();

        if (const auto saveRes = groupStore_.save(g); !saveRes)
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }

        return g;
    }

    std::expected<Group, GroupServiceError>
    GroupService::addRepositoryToGroup(const GroupId& id, const RepositoryId& repoId) const
    {
        if (const auto repoRes = repoStore_.getById(repoId); !repoRes || !repoRes->has_value())
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::RepositoryNotFound,
                .message = "Repository non esistente nel catalogo"
            });
        }

        auto groupRes = groupStore_.getById(id);
        if (!groupRes || !groupRes->has_value())
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::NotFound,
                .message = "Gruppo non trovato"
            });
        }

        auto g = std::move(**groupRes);
        if (!g.contains(repoId))
        {
            g.repositoryIds.push_back(repoId);
            g.updatedAtUnixEpochMs = currentUnixMs();
            if (const auto saveRes = groupStore_.save(g); !saveRes)
            {
                return std::unexpected(GroupServiceError{
                    .kind = GroupServiceError::Kind::StorageError,
                    .message = saveRes.error().message
                });
            }
        }

        return g;
    }

    std::expected<Group, GroupServiceError>
    GroupService::removeRepositoryFromGroup(const GroupId& id, const RepositoryId& repoId) const
    {
        auto groupRes = groupStore_.getById(id);
        if (!groupRes || !groupRes->has_value())
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::NotFound,
                .message = "Gruppo non trovato"
            });
        }

        auto g = std::move(**groupRes);
        if (const auto it = std::ranges::find(g.repositoryIds, repoId); it != g.repositoryIds.end())
        {
            g.repositoryIds.erase(it);
            g.updatedAtUnixEpochMs = currentUnixMs();
            if (const auto saveRes = groupStore_.save(g); !saveRes)
            {
                return std::unexpected(GroupServiceError{
                    .kind = GroupServiceError::Kind::StorageError,
                    .message = saveRes.error().message
                });
            }
        }

        return g;
    }

    std::expected<Group, GroupServiceError>
    GroupService::reorderRepositories(const GroupId& id, std::vector<RepositoryId> orderedIds) const
    {
        auto groupRes = groupStore_.getById(id);
        if (!groupRes || !groupRes->has_value())
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::NotFound,
                .message = "Gruppo non trovato"
            });
        }

        auto g = std::move(**groupRes);
        g.repositoryIds = std::move(orderedIds);
        g.updatedAtUnixEpochMs = currentUnixMs();

        if (const auto saveRes = groupStore_.save(g); !saveRes)
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }

        return g;
    }

    std::expected<bool, GroupServiceError>
    GroupService::deleteGroup(const GroupId& id) const
    {
        auto res = groupStore_.remove(id);
        if (!res)
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return *res;
    }

    std::expected<std::optional<Group>, GroupServiceError>
    GroupService::getGroup(const GroupId& id) const
    {
        auto res = groupStore_.getById(id);
        if (!res)
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return std::move(*res);
    }

    std::expected<std::vector<Group>, GroupServiceError>
    GroupService::getAllGroups() const
    {
        auto res = groupStore_.getAll();
        if (!res)
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return std::move(*res);
    }

    std::expected<std::vector<Group>, GroupServiceError>
    GroupService::getGroupsForRepository(const RepositoryId& repoId) const
    {
        auto res = groupStore_.getGroupsContainingRepository(repoId);
        if (!res)
        {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return std::move(*res);
    }

    std::expected<QuickOpenContext, GroupServiceError>
    GroupService::resolveQuickOpenContext(const std::vector<GroupId>& selectedGroupIds) const
    {
        QuickOpenContext ctx;
        ctx.selectedGroupIds = selectedGroupIds;

        std::unordered_set<RepositoryId> seenRepoIds;

        for (const auto& gId : selectedGroupIds)
        {
            auto gRes = groupStore_.getById(gId);
            if (!gRes || !gRes->has_value())
            {
                continue; // Skip missing groups
            }

            const auto& grp = **gRes;
            ctx.totalRepositoryReferences += grp.repositoryIds.size();

            for (const auto& rId : grp.repositoryIds)
            {
                if (seenRepoIds.insert(rId).second)
                {
                    // First encounter of this repository: load complete entity
                    if (auto repoRes = repoStore_.getById(rId); repoRes && repoRes->has_value())
                    {
                        ctx.repositories.push_back(std::move(**repoRes));
                    }
                }
            }
        }

        return ctx;
    }
} // namespace gf::groups::bl
