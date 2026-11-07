#pragma once

#include "core/modules/repositories/bo/repository.hpp"
#include "core/modules/repositories/ports/repository_store.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gf::repositories::bl {

struct RegisterRepositoryRequest {
    std::filesystem::path localPath;
    std::optional<std::string> displayName;
};

struct RepositoryServiceError {
    enum class Kind {
        PathNotFound,
        NotAGitRepository,
        AlreadyRegistered,
        NotFound,
        InvalidPath,
        StorageError
    };
    Kind kind = Kind::StorageError;
    std::string message;
};

class RepositoryService {
public:
    explicit RepositoryService(ports::IRepositoryStore& store) noexcept;

    [[nodiscard]] std::expected<bo::Repository, RepositoryServiceError>
    registerRepository(const RegisterRepositoryRequest& request);

    [[nodiscard]] std::expected<bo::Repository, RepositoryServiceError>
    checkRepositoryStatus(const bo::RepositoryId& id);

    [[nodiscard]] std::expected<bo::Repository, RepositoryServiceError>
    reconnectRepositoryPath(const bo::RepositoryId& id, const std::filesystem::path& newPath);

    [[nodiscard]] std::expected<void, RepositoryServiceError>
    touchLastAccessed(const bo::RepositoryId& id);

    [[nodiscard]] std::expected<bool, RepositoryServiceError>
    unregisterRepository(const bo::RepositoryId& id);

    [[nodiscard]] std::expected<std::vector<bo::Repository>, RepositoryServiceError>
    getAllRepositories();

    [[nodiscard]] std::expected<std::optional<bo::Repository>, RepositoryServiceError>
    getRepository(const bo::RepositoryId& id);

    [[nodiscard]] std::expected<bo::Repository, RepositoryServiceError>
    updateDisplayName(const bo::RepositoryId& id, const std::string& newDisplayName);

    [[nodiscard]] static bool isGitRepositoryDir(const std::filesystem::path& path) noexcept;

private:
    ports::IRepositoryStore& store_;
};

} // namespace gf::repositories::bl
