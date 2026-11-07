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

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    stageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    stageAll(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    unstageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    unstageAll(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    discardFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) override;

    [[nodiscard]] std::expected<gf::git::bo::CommitResult, gf::git::ports::GitOpsError>
    createCommit(const std::filesystem::path& repoPath, const gf::git::bo::CommitOptions& options) override;

private:
    gf::git::ports::IGitCommandExecutor& m_executor;
};

} // namespace gf::infra::git
