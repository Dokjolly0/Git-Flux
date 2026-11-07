#pragma once

#include "core/modules/git/ports/repository_git_ops.hpp"
#include "core/modules/git/ports/git_command_executor.hpp"

namespace gf::infra::git {

class NativeRepositoryGitOps final : public gf::git::ports::IRepositoryGitOps {
public:
    explicit NativeRepositoryGitOps(gf::git::ports::IGitCommandExecutor& executor) noexcept;
    ~NativeRepositoryGitOps() override = default;

    [[nodiscard]] std::expected<gf::git::bo::RepoWorkingTreeStatus, gf::git::ports::GitOpsError>
    getWorkingTreeStatus(const std::filesystem::path& repoPath) override;

private:
    gf::git::ports::IGitCommandExecutor& m_executor;
};

} // namespace gf::infra::git
