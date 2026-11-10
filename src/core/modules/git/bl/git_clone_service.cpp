#include "core/modules/git/bl/git_clone_service.hpp"

#include <algorithm>

namespace gf::git::bl
{
    GitCloneService::GitCloneService(ports::IRepositoryGitOps& gitOps,
                                     repositories::bl::RepositoryService& repoService) noexcept
        : m_gitOps(gitOps), m_repoService(repoService)
    {
    }

    std::string GitCloneService::extractRepositoryNameFromUrl(std::string_view url) noexcept
    {
        while (!url.empty() && (url.front() == ' ' || url.front() == '\t' || url.front() == '\r' || url.front() ==
            '\n'))
        {
            url.remove_prefix(1);
        }
        while (!url.empty() && (url.back() == ' ' || url.back() == '\t' || url.back() == '\r' || url.back() == '\n' ||
            url.back() == '/' || url.back() == '\\'))
        {
            url.remove_suffix(1);
        }
        if (url.empty())
        {
            return "repository";
        }

        auto lastSlash = url.find_last_of("/:\\");
        std::string_view segment = (lastSlash == std::string_view::npos) ? url : url.substr(lastSlash + 1);

        if (segment.ends_with(".git"))
        {
            segment.remove_suffix(4);
        }

        if (segment.empty())
        {
            return "repository";
        }

        return std::string(segment);
    }

    std::expected<repositories::bo::Repository, ports::GitOpsError>
    GitCloneService::cloneAndRegister(const bo::CloneRepositoryOptions& options,
                                      const std::optional<std::string>& customDisplayName)
    {
        auto cloneRes = m_gitOps.cloneRepository(options);
        if (!cloneRes)
        {
            return std::unexpected(cloneRes.error());
        }

        std::string displayName = customDisplayName.value_or(
            !cloneRes->displayName.empty() ? cloneRes->displayName : extractRepositoryNameFromUrl(options.remoteUrl)
        );

        repositories::bl::RegisterRepositoryRequest regReq{
            .localPath = cloneRes->repositoryPath,
            .displayName = displayName
        };

        auto regRes = m_repoService.registerRepository(regReq);
        if (!regRes)
        {
            return std::unexpected(ports::GitOpsError{
                .kind = ports::GitOpsError::Kind::StorageError,
                .exitCode = -1,
                .message = "Repository clonato con successo, ma registrazione nel catalogo fallita: " + regRes.error().
                message,
                .command = "registerRepository"
            });
        }

        return *regRes;
    }
} // namespace gf::git::bl
