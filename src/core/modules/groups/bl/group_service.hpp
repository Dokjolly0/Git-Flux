#pragma once

#include "core/modules/groups/bo/group.hpp"
#include "core/modules/groups/ports/group_store.hpp"
#include "core/modules/repositories/bo/repository.hpp"
#include "core/modules/repositories/ports/repository_store.hpp"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace gf::groups::bl
{
    struct GroupServiceError
    {
        enum class Kind
        {
            NotFound,
            DuplicateName,
            RepositoryNotFound,
            InvalidName,
            StorageError
        };

        Kind kind = Kind::StorageError;
        std::string message;
    };

    struct QuickOpenContext
    {
        std::vector<bo::GroupId> selectedGroupIds;
        std::vector<repositories::bo::Repository> repositories; // Deduplicated, ordered
        std::size_t totalRepositoryReferences = 0; // Count before deduplication
    };

    class GroupService
    {
    public:
        GroupService(ports::IGroupStore& groupStore,
                     repositories::ports::IRepositoryStore& repoStore) noexcept;

        [[nodiscard]] std::expected<bo::Group, GroupServiceError>
        createGroup(std::string name, std::vector<repositories::bo::RepositoryId> repoIds = {});

        [[nodiscard]] std::expected<bo::Group, GroupServiceError>
        renameGroup(const bo::GroupId& id, std::string newName);

        [[nodiscard]] std::expected<bo::Group, GroupServiceError>
        addRepositoryToGroup(const bo::GroupId& id, const repositories::bo::RepositoryId& repoId);

        [[nodiscard]] std::expected<bo::Group, GroupServiceError>
        removeRepositoryFromGroup(const bo::GroupId& id, const repositories::bo::RepositoryId& repoId);

        [[nodiscard]] std::expected<bo::Group, GroupServiceError>
        reorderRepositories(const bo::GroupId& id, std::vector<repositories::bo::RepositoryId> orderedIds);

        [[nodiscard]] std::expected<bool, GroupServiceError>
        deleteGroup(const bo::GroupId& id);

        [[nodiscard]] std::expected<std::optional<bo::Group>, GroupServiceError>
        getGroup(const bo::GroupId& id);

        [[nodiscard]] std::expected<std::vector<bo::Group>, GroupServiceError>
        getAllGroups();

        [[nodiscard]] std::expected<QuickOpenContext, GroupServiceError>
        resolveQuickOpenContext(const std::vector<bo::GroupId>& selectedGroupIds);

    private:
        ports::IGroupStore& groupStore_;
        repositories::ports::IRepositoryStore& repoStore_;
    };
} // namespace gf::groups::bl
