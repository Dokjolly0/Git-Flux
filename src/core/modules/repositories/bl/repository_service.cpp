#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/shared/id_generator.hpp"
#include "core/shared/utf8_path.hpp"

#include <chrono>

namespace gf::repositories::bl
{
    using bo::Repository;
    using bo::RepositoryId;
    using bo::RepositoryPathStatus;

    namespace
    {
        [[nodiscard]] std::int64_t currentUnixMs() noexcept
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                .count();
        }
    } // namespace

    RepositoryService::RepositoryService(ports::IRepositoryStore& store) noexcept
        : store_(store)
    {
    }

    bool RepositoryService::isGitRepositoryDir(const std::filesystem::path& path) noexcept
    {
        if (path.empty())
        {
            return false;
        }
        std::error_code ec;
        auto target = std::filesystem::weakly_canonical(path, ec);
        if (ec)
        {
            target = path.lexically_normal();
        }
        if (target.filename() == ".git")
        {
            target = target.parent_path();
        }
        if (!std::filesystem::is_directory(target, ec))
        {
            return false;
        }
        const auto gitEntry = target / ".git";
        return std::filesystem::exists(gitEntry, ec);
    }

    std::expected<Repository, RepositoryServiceError>
    RepositoryService::registerRepository(const RegisterRepositoryRequest& req)
    {
        if (req.localPath.empty())
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::InvalidPath,
                .message = "Specified path is empty"
            });
        }

        std::error_code ec;
        auto targetRepoPath = std::filesystem::weakly_canonical(req.localPath, ec);
        if (ec || !std::filesystem::exists(targetRepoPath, ec))
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::PathNotFound,
                .message = "Specified folder does not exist: " + gf::core::pathToUtf8(req.localPath)
            });
        }

        if (targetRepoPath.filename() == ".git")
        {
            targetRepoPath = targetRepoPath.parent_path();
        }

        if (!isGitRepositoryDir(targetRepoPath))
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::NotAGitRepository,
                .message = "Directory does not contain a valid Git repository (.git not found): " +
                gf::core::pathToUtf8(targetRepoPath)
            });
        }

        // Check if already registered by path
        auto existingByPath = store_.getByPath(targetRepoPath);
        if (!existingByPath)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = existingByPath.error().message
            });
        }
        if (existingByPath->has_value())
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::AlreadyRegistered,
                .message = "Repository already registered in catalog with ID: " + (*existingByPath)->id.value()
            });
        }

        // Compute display name if omitted
        std::string displayName;
        if (req.displayName && !req.displayName->empty())
        {
            displayName = *req.displayName;
        }
        else
        {
            displayName = targetRepoPath.filename().string();
            if (displayName.empty())
            {
                displayName = "Repository";
            }
        }

        const auto now = currentUnixMs();
        Repository repo{
            .id = RepositoryId(gf::core::generateUniqueId("repo")),
            .displayName = std::move(displayName),
            .localPath = targetRepoPath,
            .pathStatus = RepositoryPathStatus::Valid,
            .registeredAtUnixEpochMs = now,
            .lastAccessedAtUnixEpochMs = now
        };

        auto saveRes = store_.save(repo);
        if (!saveRes)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }

        return repo;
    }

    std::expected<Repository, RepositoryServiceError>
    RepositoryService::checkRepositoryStatus(const RepositoryId& id)
    {
        auto repoRes = store_.getById(id);
        if (!repoRes)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = repoRes.error().message
            });
        }
        if (!repoRes->has_value())
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::NotFound,
                .message = "Repository non trovato nel catalogo: " + id.value()
            });
        }

        auto repo = std::move(**repoRes);
        std::error_code ec;
        if (!std::filesystem::exists(repo.localPath, ec))
        {
            repo.pathStatus = RepositoryPathStatus::Missing;
        }
        else if (!isGitRepositoryDir(repo.localPath))
        {
            repo.pathStatus = RepositoryPathStatus::NotAGitRepository;
        }
        else
        {
            repo.pathStatus = RepositoryPathStatus::Valid;
        }

        (void)store_.save(repo);
        return repo;
    }

    std::expected<Repository, RepositoryServiceError>
    RepositoryService::reconnectRepositoryPath(const RepositoryId& id, const std::filesystem::path& newPath)
    {
        auto existingRes = store_.getById(id);
        if (!existingRes)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = existingRes.error().message
            });
        }
        if (!existingRes->has_value())
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::NotFound,
                .message = "Repository non trovato per riconnessione: " + id.value()
            });
        }

        std::error_code ec;
        auto targetRepoPath = std::filesystem::weakly_canonical(newPath, ec);
        if (ec || !std::filesystem::exists(targetRepoPath, ec))
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::PathNotFound,
                .message = "The new path does not exist: " + gf::core::pathToUtf8(newPath)
            });
        }

        if (targetRepoPath.filename() == ".git")
        {
            targetRepoPath = targetRepoPath.parent_path();
        }

        if (!isGitRepositoryDir(targetRepoPath))
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::NotAGitRepository,
                .message = "The new path does not contain a valid .git directory: " + gf::core::pathToUtf8(
                    targetRepoPath)
            });
        }

        auto updateRes = store_.updatePath(id, targetRepoPath);
        if (!updateRes)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = updateRes.error().message
            });
        }

        auto reloaded = store_.getById(id);
        if (!reloaded || !reloaded->has_value())
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = "Errore durante il recupero del repository riconnesso"
            });
        }

        return std::move(**reloaded);
    }

    std::expected<void, RepositoryServiceError>
    RepositoryService::touchLastAccessed(const RepositoryId& id)
    {
        auto repoRes = store_.getById(id);
        if (!repoRes || !repoRes->has_value())
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::NotFound,
                .message = "Repository non trovato"
            });
        }
        auto repo = std::move(**repoRes);
        repo.lastAccessedAtUnixEpochMs = currentUnixMs();
        auto saveRes = store_.save(repo);
        if (!saveRes)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }
        return {};
    }

    std::expected<bool, RepositoryServiceError>
    RepositoryService::unregisterRepository(const RepositoryId& id)
    {
        auto res = store_.remove(id);
        if (!res)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return *res;
    }

    std::expected<std::vector<Repository>, RepositoryServiceError>
    RepositoryService::getAllRepositories()
    {
        auto res = store_.getAll();
        if (!res)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return std::move(*res);
    }

    std::expected<std::optional<Repository>, RepositoryServiceError>
    RepositoryService::getRepository(const RepositoryId& id)
    {
        auto res = store_.getById(id);
        if (!res)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = res.error().message
            });
        }
        return std::move(*res);
    }

    std::expected<Repository, RepositoryServiceError>
    RepositoryService::updateDisplayName(const RepositoryId& id, const std::string& newDisplayName)
    {
        auto repoRes = store_.getById(id);
        if (!repoRes)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = repoRes.error().message
            });
        }
        if (!repoRes->has_value())
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::NotFound,
                .message = "Repository not found: " + id.value()
            });
        }

        auto repo = std::move(**repoRes);
        repo.displayName = newDisplayName.empty() ? repo.localPath.filename().string() : newDisplayName;
        auto saveRes = store_.save(repo);
        if (!saveRes)
        {
            return std::unexpected(RepositoryServiceError{
                .kind = RepositoryServiceError::Kind::StorageError,
                .message = saveRes.error().message
            });
        }
        return repo;
    }
} // namespace gf::repositories::bl
