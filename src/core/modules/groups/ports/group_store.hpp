#pragma once

#include "core/modules/groups/bo/group.hpp"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace gf::groups::ports
{
    struct GroupStoreError
    {
        enum class Kind
        {
            NotFound,
            DuplicateId,
            DuplicateName,
            StorageError
        };

        Kind kind = Kind::StorageError;
        std::string message;
    };

    class IGroupStore
    {
    public:
        virtual ~IGroupStore() = default;

        [[nodiscard]] virtual std::expected<void, GroupStoreError>
        save(const bo::Group& group) = 0;

        [[nodiscard]] virtual std::expected<std::optional<bo::Group>, GroupStoreError>
        getById(const bo::GroupId& id) = 0;

        [[nodiscard]] virtual std::expected<std::vector<bo::Group>, GroupStoreError>
        getAll() = 0;

        [[nodiscard]] virtual std::expected<bool, GroupStoreError>
        remove(const bo::GroupId& id) = 0;

        [[nodiscard]] virtual std::expected<std::vector<bo::Group>, GroupStoreError>
        getGroupsContainingRepository(const repositories::bo::RepositoryId& repoId) = 0;
    };
} // namespace gf::groups::ports
