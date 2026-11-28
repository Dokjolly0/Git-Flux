#include "infrastructure/git/native/native_git_ops_internal.hpp"
#include "core/shared/utf8_path.hpp"

#include <algorithm>
#include <fstream>
#include <string>

namespace gf::infra::git
{
    namespace
    {
        [[nodiscard]] bool isGitObjectPathMissing(const std::string& err) noexcept
        {
            return err.find("does not exist") != std::string::npos ||
                   err.find("non esiste") != std::string::npos ||
                   err.find("exists on disk") != std::string::npos ||
                   err.find("Not a valid object name") != std::string::npos ||
                   err.find("Invalid object name") != std::string::npos ||
                   err.find("Path '") != std::string::npos;
        }
    } // namespace

    std::expected<std::string, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getFileContent(const std::filesystem::path& repoPath,
                                           const std::string& revision,
                                           const std::string& filePath)
    {
        if (filePath.empty())
        {
            return std::string{};
        }

        constexpr std::size_t kMaxContentBytes = 2 * 1024 * 1024;
        auto blob = getFileBlob(repoPath, revision, filePath, kMaxContentBytes);
        if (!blob)
        {
            return std::unexpected(blob.error());
        }

        const std::string commandLabel =
            (revision.empty() || revision == "WORKTREE") ? "read file" : "git show";
        if (blob->truncated || blob->bytes.size() > kMaxContentBytes)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "File troppo grande (> 2 MiB) per l'anteprima testuale",
                .command = commandLabel
            });
        }
        if (blob->bytes.find('\0') != std::string::npos)
        {
            return std::unexpected(gf::git::ports::GitOpsError{
                .kind = gf::git::ports::GitOpsError::Kind::InvalidOperation,
                .exitCode = -1,
                .message = "Il file contiene dati binari",
                .command = commandLabel
            });
        }

        core::stripUtf8Bom(blob->bytes);
        return std::move(blob->bytes);
    }

    std::expected<gf::git::bo::FileBlob, gf::git::ports::GitOpsError>
    NativeRepositoryGitOps::getFileBlob(const std::filesystem::path& repoPath,
                                        const std::string& revision,
                                        const std::string& filePath,
                                        const std::size_t maxBytes)
    {
        if (filePath.empty())
        {
            return gf::git::bo::FileBlob{};
        }

        if (revision.empty() || revision == "WORKTREE")
        {
            const auto fullPath = repoPath / core::utf8ToPath(filePath);
            std::error_code ec;
            if (!std::filesystem::exists(fullPath, ec) || !std::filesystem::is_regular_file(fullPath, ec))
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::PathNotFound,
                    .exitCode = -1,
                    .message = "File non trovato nella working tree: " + filePath,
                    .command = "read file"
                });
            }

            const std::uint64_t fileSize = std::filesystem::file_size(fullPath, ec);
            if (ec)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::StorageError,
                    .exitCode = -1,
                    .message = "Impossibile determinare la dimensione del file: " + filePath,
                    .command = "read file"
                });
            }

            std::ifstream ifs(fullPath, std::ios::binary);
            if (!ifs)
            {
                return std::unexpected(gf::git::ports::GitOpsError{
                    .kind = gf::git::ports::GitOpsError::Kind::StorageError,
                    .exitCode = -1,
                    .message = "Impossibile leggere il file: " + filePath,
                    .command = "read file"
                });
            }

            gf::git::bo::FileBlob blob;
            blob.totalSize = fileSize;
            const std::size_t toRead = static_cast<std::size_t>(std::min<std::uint64_t>(fileSize, maxBytes));
            blob.truncated = fileSize > maxBytes;
            if (toRead > 0)
            {
                blob.bytes.resize(toRead);
                ifs.read(blob.bytes.data(), static_cast<std::streamsize>(toRead));
                const auto actuallyRead = static_cast<std::size_t>(ifs.gcount());
                blob.bytes.resize(actuallyRead);
            }
            return blob;
        }

        std::string normPath = filePath;
        std::ranges::replace(normPath, '\\', '/');
        const std::string targetRev = revision == "INDEX" ? std::string(":0") : revision;
        const std::string spec = targetRev + ":" + normPath;

        // First probe exact blob size with `git cat-file -s` so oversized objects never OOM.
        auto sizeRes = detail::runGitChecked(
            m_executor, repoPath, {"cat-file", "-s", spec}, "git cat-file", "git cat-file",
            [](const gf::git::bo::GitProcessResult& res, const std::string& label)
            {
                const auto kind = isGitObjectPathMissing(res.stderrBytes)
                    ? gf::git::ports::GitOpsError::Kind::PathNotFound
                    : gf::git::ports::GitOpsError::Kind::ExecutionFailed;
                return gf::git::ports::GitOpsError{kind, res.exitCode, detail::pickErrorMessage(res), label};
            },
            gf::git::bo::GitLockPolicy::NoOptionalLocks, 10'000);
        if (!sizeRes) return std::unexpected(sizeRes.error());

        std::uint64_t objectSize = 0;
        try
        {
            if (const std::string trimmed = detail::trimString(sizeRes->stdoutBytes); !trimmed.empty())
            {
                objectSize = std::stoull(trimmed);
            }
        }
        catch (...)
        {
            objectSize = 0;
        }

        if (objectSize > maxBytes)
        {
            return gf::git::bo::FileBlob{
                .bytes = {},
                .totalSize = objectSize,
                .truncated = true
            };
        }

        auto execResult = detail::runGitChecked(
            m_executor, repoPath, {"show", spec}, "git show", "git show", detail::defaultGitFailure,
            gf::git::bo::GitLockPolicy::NoOptionalLocks, 20'000);
        if (!execResult) return std::unexpected(execResult.error());

        gf::git::bo::FileBlob blob;
        blob.bytes = std::move(execResult->stdoutBytes);
        blob.totalSize = std::max<std::uint64_t>(objectSize, blob.bytes.size());
        blob.truncated = false;
        return blob;
    }
} // namespace gf::infra::git
