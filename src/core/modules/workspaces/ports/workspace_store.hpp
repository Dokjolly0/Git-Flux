#pragma once

#include "core/modules/workspaces/bo/workspace.hpp"
#include "core/modules/workspaces/bo/workspace_session.hpp"

#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace gf::workspaces::ports
{
    struct WorkspaceStoreError
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

    class IWorkspaceStore
    {
    public:
        virtual ~IWorkspaceStore() = default;

        [[nodiscard]] virtual std::expected<void, WorkspaceStoreError>
        save(const bo::Workspace& workspace) = 0;

        [[nodiscard]] virtual std::expected<std::optional<bo::Workspace>, WorkspaceStoreError>
        getById(const bo::WorkspaceId& id) = 0;

        [[nodiscard]] virtual std::expected<std::vector<bo::Workspace>, WorkspaceStoreError>
        getAll() = 0;

        [[nodiscard]] virtual std::expected<bool, WorkspaceStoreError>
        remove(const bo::WorkspaceId& id) = 0;

        [[nodiscard]] virtual std::expected<void, WorkspaceStoreError>
        saveSession(const bo::WorkspaceSession& session) = 0;

        [[nodiscard]] virtual std::expected<std::optional<bo::WorkspaceSession>, WorkspaceStoreError>
        getSession(const bo::WorkspaceId& id) = 0;
    };
} // namespace gf::workspaces::ports
