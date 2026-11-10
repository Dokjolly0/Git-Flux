#pragma once

#include "core/modules/git/bo/clone_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"
#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/repositories/bo/repository.hpp"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace gf::git::bl
{
    class GitCloneService
    {
    public:
        explicit GitCloneService(ports::IRepositoryGitOps& gitOps,
                                 repositories::bl::RepositoryService& repoService) noexcept;
        ~GitCloneService() = default;

        /// Extracts the repository name from a git remote URL or path (e.g. "https://github.com/foo/bar.git" -> "bar").
        [[nodiscard]] static std::string extractRepositoryNameFromUrl(std::string_view url) noexcept;

        /// Clones a remote repository and registers it into the local catalog.
        [[nodiscard]] std::expected<repositories::bo::Repository, ports::GitOpsError>
        cloneAndRegister(const bo::CloneRepositoryOptions& options,
                         const std::optional<std::string>& customDisplayName = std::nullopt);

    private:
        ports::IRepositoryGitOps& m_gitOps;
        repositories::bl::RepositoryService& m_repoService;
    };
} // namespace gf::git::bl
