#pragma once

#include "core/modules/repositories/bo/repository.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gf::repositories::ports {

struct RepositoryStoreError {
    enum class Kind {
        NotFound,
        DuplicateId,
        DuplicatePath,
        StorageError
    };
    Kind kind = Kind::StorageError;
    std::string message;
};

class IRepositoryStore {
public:
    virtual ~IRepositoryStore() = default;

    [[nodiscard]] virtual std::expected<void, RepositoryStoreError>
    save(const bo::Repository& repository) = 0;

    [[nodiscard]] virtual std::expected<std::optional<bo::Repository>, RepositoryStoreError>
    getById(const bo::RepositoryId& id) = 0;

    [[nodiscard]] virtual std::expected<std::optional<bo::Repository>, RepositoryStoreError>
    getByPath(const std::filesystem::path& path) = 0;

    [[nodiscard]] virtual std::expected<std::vector<bo::Repository>, RepositoryStoreError>
    getAll() = 0;

    [[nodiscard]] virtual std::expected<bool, RepositoryStoreError>
    remove(const bo::RepositoryId& id) = 0;

    [[nodiscard]] virtual std::expected<void, RepositoryStoreError>
    updatePath(const bo::RepositoryId& id, const std::filesystem::path& newPath) = 0;
};

} // namespace gf::repositories::ports
