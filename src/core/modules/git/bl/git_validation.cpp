#include "core/modules/git/bl/git_validation.hpp"

namespace gf::git::bl
{
    std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath,
                     const std::string_view commandContext)
    {
        if (repoPath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del repository non può essere vuoto",
                .command = std::string(commandContext)
            });
        }

        if (std::error_code ec; !std::filesystem::exists(repoPath, ec) || !std::filesystem::is_directory(repoPath, ec))
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::PathNotFound,
                .exitCode = -1,
                .message = "La directory del repository non esiste: " + repoPath.string(),
                .command = std::string(commandContext)
            });
        }

        return {};
    }

    std::expected<void, ports::GitOpsError>
    validateRelativeFilePath(const std::string_view filePath,
                             const std::string_view commandContext)
    {
        if (filePath.empty())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del file non può essere vuoto",
                .command = std::string(commandContext)
            });
        }

        const std::filesystem::path p(filePath);
        if (p.is_absolute() || p.has_root_path())
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                .exitCode = -1,
                .message = "Il percorso del file deve essere relativo al repository: " + std::string(filePath),
                .command = std::string(commandContext)
            });
        }

        for (const auto& part : p)
        {
            if (part == "..")
            {
                return std::unexpected(ports::GitOpsError{
                    .kind = ports::GitOpsError::Kind::InvalidRepositoryPath,
                    .exitCode = -1,
                    .message = "Rilevato tentativo di path traversal illegale: " + std::string(filePath),
                    .command = std::string(commandContext)
                });
            }
        }

        return {};
    }

    std::expected<void, ports::GitOpsError>
    validateRelativeFilePaths(const std::vector<std::string>& filePaths,
                              const std::string_view commandContext)
    {
        for (const auto& pathStr : filePaths)
        {
            if (auto res = validateRelativeFilePath(pathStr, commandContext); !res)
            {
                return res;
            }
        }
        return {};
    }

    bool isValidRefName(const std::string_view name) noexcept
    {
        if (name.empty() || name == "@")
        {
            return false;
        }
        if (name.front() == '.' || name.front() == '-' || name.front() == '/')
        {
            return false;
        }
        if (name.back() == '.' || name.back() == '/')
        {
            return false;
        }
        if (name.ends_with(".lock"))
        {
            return false;
        }

        for (const char ch : name)
        {
            const auto c = static_cast<unsigned char>(ch);
            if (c < 0x20 || c == 0x7f)
            {
                return false;
            }
            if (c == ' ' || c == '~' || c == '^' || c == ':' || c == '?' || c == '*' || c == '[' || c == '\\')
            {
                return false;
            }
        }

        if (name.find("..") != std::string_view::npos)
        {
            return false;
        }
        if (name.find("//") != std::string_view::npos)
        {
            return false;
        }
        if (name.find("@{") != std::string_view::npos)
        {
            return false;
        }

        return true;
    }
} // namespace gf::git::bl
