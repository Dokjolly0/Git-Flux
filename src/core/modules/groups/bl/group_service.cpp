#include "core/modules/groups/bl/group_service.hpp"
#include "core/shared/id_generator.hpp"

#include <algorithm>
#include <chrono>
#include <unordered_set>

namespace gf::groups::bl {

using bo::Group;
using bo::GroupId;
using repositories::bo::Repository;
using repositories::bo::RepositoryId;

namespace {

[[nodiscard]] std::int64_t currentUnixMs() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

GroupService::GroupService(ports::IGroupStore& groupStore,
                           repositories::ports::IRepositoryStore& repoStore) noexcept
    : groupStore_(groupStore)
    , repoStore_(repoStore) {}

std::expected<Group, GroupServiceError>
GroupService::createGroup(std::string name, std::vector<RepositoryId> repoIds) {
    if (name.empty()) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::InvalidName,
            .message = "Il nome del gruppo non può essere vuoto"
        });
    }

    // Verify existence of all provided repositories
    for (const auto& rId : repoIds) {
        auto r = repoStore_.getById(rId);
        if (!r || !r->has_value()) {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::RepositoryNotFound,
                .message = "Repository non trovato nel catalogo: " + rId.value()
            });
        }
    }

    const auto now = currentUnixMs();
    Group g{
        .id = GroupId(gf::core::generateUniqueId("grp")),
        .name = std::move(name),
        .repositoryIds = std::move(repoIds),
        .createdAtUnixEpochMs = now,
        .updatedAtUnixEpochMs = now
    };

    auto saveRes = groupStore_.save(g);
    if (!saveRes) {
        if (saveRes.error().kind == ports::GroupStoreError::Kind::DuplicateName) {
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
GroupService::renameGroup(const GroupId& id, std::string newName) {
    if (newName.empty()) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::InvalidName,
            .message = "Il nuovo nome non può essere vuoto"
        });
    }

    auto existing = groupStore_.getById(id);
    if (!existing || !existing->has_value()) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::NotFound,
            .message = "Gruppo non trovato: " + id.value()
        });
    }

    auto g = std::move(**existing);
    g.name = std::move(newName);
    g.updatedAtUnixEpochMs = currentUnixMs();

    auto saveRes = groupStore_.save(g);
    if (!saveRes) {
        if (saveRes.error().kind == ports::GroupStoreError::Kind::DuplicateName) {
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
GroupService::addRepositoryToGroup(const GroupId& id, const RepositoryId& repoId) {
    auto repoRes = repoStore_.getById(repoId);
    if (!repoRes || !repoRes->has_value()) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::RepositoryNotFound,
            .message = "Repository non esistente nel catalogo"
        });
    }

    auto groupRes = groupStore_.getById(id);
    if (!groupRes || !groupRes->has_value()) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::NotFound,
            .message = "Gruppo non trovato"
        });
    }

    auto g = std::move(**groupRes);
    if (!g.contains(repoId)) {
        g.repositoryIds.push_back(repoId);
        g.updatedAtUnixEpochMs = currentUnixMs();
        auto saveRes = groupStore_.save(g);
        if (!saveRes) {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }
    }

    return g;
}

std::expected<Group, GroupServiceError>
GroupService::removeRepositoryFromGroup(const GroupId& id, const RepositoryId& repoId) {
    auto groupRes = groupStore_.getById(id);
    if (!groupRes || !groupRes->has_value()) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::NotFound,
            .message = "Gruppo non trovato"
        });
    }

    auto g = std::move(**groupRes);
    auto it = std::ranges::find(g.repositoryIds, repoId);
    if (it != g.repositoryIds.end()) {
        g.repositoryIds.erase(it);
        g.updatedAtUnixEpochMs = currentUnixMs();
        auto saveRes = groupStore_.save(g);
        if (!saveRes) {
            return std::unexpected(GroupServiceError{
                .kind = GroupServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }
    }

    return g;
}

std::expected<Group, GroupServiceError>
GroupService::reorderRepositories(const GroupId& id, std::vector<RepositoryId> orderedIds) {
    auto groupRes = groupStore_.getById(id);
    if (!groupRes || !groupRes->has_value()) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::NotFound,
            .message = "Gruppo non trovato"
        });
    }

    auto g = std::move(**groupRes);
    g.repositoryIds = std::move(orderedIds);
    g.updatedAtUnixEpochMs = currentUnixMs();

    auto saveRes = groupStore_.save(g);
    if (!saveRes) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::StorageError,
            .message = saveRes.error().message
        });
    }

    return g;
}

std::expected<bool, GroupServiceError>
GroupService::deleteGroup(const GroupId& id) {
    auto res = groupStore_.remove(id);
    if (!res) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::StorageError,
            .message = res.error().message
        });
    }
    return *res;
}

std::expected<std::optional<Group>, GroupServiceError>
GroupService::getGroup(const GroupId& id) {
    auto res = groupStore_.getById(id);
    if (!res) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::StorageError,
            .message = res.error().message
        });
    }
    return std::move(*res);
}

std::expected<std::vector<Group>, GroupServiceError>
GroupService::getAllGroups() {
    auto res = groupStore_.getAll();
    if (!res) {
        return std::unexpected(GroupServiceError{
            .kind = GroupServiceError::Kind::StorageError,
            .message = res.error().message
        });
    }
    return std::move(*res);
}

std::expected<QuickOpenContext, GroupServiceError>
GroupService::resolveQuickOpenContext(const std::vector<GroupId>& selectedGroupIds) {
    QuickOpenContext ctx;
    ctx.selectedGroupIds = selectedGroupIds;

    std::unordered_set<RepositoryId> seenRepoIds;

    for (const auto& gId : selectedGroupIds) {
        auto gRes = groupStore_.getById(gId);
        if (!gRes || !gRes->has_value()) {
            continue; // Skip missing groups
        }

        const auto& grp = **gRes;
        ctx.totalRepositoryReferences += grp.repositoryIds.size();

        for (const auto& rId : grp.repositoryIds) {
            if (seenRepoIds.insert(rId).second) {
                // First encounter of this repository: load complete entity
                auto repoRes = repoStore_.getById(rId);
                if (repoRes && repoRes->has_value()) {
                    ctx.repositories.push_back(std::move(**repoRes));
                }
            }
        }
    }

    return ctx;
}

} // namespace gf::groups::bl
