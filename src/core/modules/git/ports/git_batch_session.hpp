#pragma once

#include "core/modules/git/bo/git_command.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace gf::git::ports {

struct GitObjectHeader {
    std::string oid;
    std::string type;
    std::size_t size = 0;
};

struct GitBatchObject {
    GitObjectHeader header;
    std::string content;
};

/// Sessione persistente verso un processo `git cat-file --batch` associato a un repository.
/// Evita di spawnare un nuovo processo Git per ogni lettura di commit/tree/blob.
class IGitBatchSession {
public:
    virtual ~IGitBatchSession() = default;

    [[nodiscard]] virtual std::expected<GitBatchObject, bo::GitExecutionError>
    readObject(std::string_view revSpec) = 0;

    [[nodiscard]] virtual bool isAlive() const noexcept = 0;
};

class IGitBatchSessionFactory {
public:
    virtual ~IGitBatchSessionFactory() = default;

    [[nodiscard]] virtual std::string_view adapterName() const noexcept = 0;

    [[nodiscard]] virtual std::expected<std::unique_ptr<IGitBatchSession>, bo::GitExecutionError>
    openSession(const std::filesystem::path& repositoryPath) = 0;
};

} // namespace gf::git::ports
