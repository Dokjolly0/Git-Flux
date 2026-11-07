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

    [[nodiscard]] std::expected<gf::git::bo::FileDiff, gf::git::ports::GitOpsError>
    getFileDiff(const std::filesystem::path& repoPath,
                const std::string& filePath,
                gf::git::bo::DiffScope scope) override;

    [[nodiscard]] std::expected<std::vector<gf::git::bo::FileDiff>, gf::git::ports::GitOpsError>
    getAllDiffs(const std::filesystem::path& repoPath,
                gf::git::bo::DiffScope scope) override;

    [[nodiscard]] std::expected<std::vector<gf::git::bo::GitBranch>, gf::git::ports::GitOpsError>
    listBranches(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    createBranch(const std::filesystem::path& repoPath,
                 const gf::git::bo::CreateBranchOptions& options) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    checkoutBranch(const std::filesystem::path& repoPath,
                   const gf::git::bo::CheckoutOptions& options) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    deleteBranch(const std::filesystem::path& repoPath,
                 const std::string& branchName,
                 bool force) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    renameBranch(const std::filesystem::path& repoPath,
                 const std::string& oldName,
                 const std::string& newName) override;

    [[nodiscard]] std::expected<std::vector<gf::git::bo::GitCommitLogItem>, gf::git::ports::GitOpsError>
    getCommitLog(const std::filesystem::path& repoPath,
                 const gf::git::bo::CommitLogOptions& options) override;

    [[nodiscard]] std::expected<std::vector<gf::git::bo::GitTag>, gf::git::ports::GitOpsError>
    listTags(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    createTag(const std::filesystem::path& repoPath,
              const gf::git::bo::CreateTagOptions& options) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    deleteTag(const std::filesystem::path& repoPath,
              const std::string& tagName) override;

    [[nodiscard]] std::expected<std::vector<gf::git::bo::GitRemote>, gf::git::ports::GitOpsError>
    listRemotes(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    addRemote(const std::filesystem::path& repoPath,
              const std::string& name,
              const std::string& url) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    removeRemote(const std::filesystem::path& repoPath,
                 const std::string& name) override;

    [[nodiscard]] std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    fetchRemote(const std::filesystem::path& repoPath,
                const gf::git::bo::RemoteOperationOptions& options) override;

    [[nodiscard]] std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    pullRemote(const std::filesystem::path& repoPath,
               const gf::git::bo::RemoteOperationOptions& options) override;

    [[nodiscard]] std::expected<gf::git::bo::RemoteOperationResult, gf::git::ports::GitOpsError>
    pushRemote(const std::filesystem::path& repoPath,
               const gf::git::bo::RemoteOperationOptions& options) override;

    [[nodiscard]] std::expected<gf::git::bo::MergeResult, gf::git::ports::GitOpsError>
    mergeBranch(const std::filesystem::path& repoPath,
                const gf::git::bo::MergeOptions& options) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    abortMerge(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<std::vector<gf::git::bo::GitStashItem>, gf::git::ports::GitOpsError>
    listStashes(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    stashPush(const std::filesystem::path& repoPath,
              const gf::git::bo::StashPushOptions& options) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    stashPop(const std::filesystem::path& repoPath,
             const gf::git::bo::StashApplyOptions& options) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    stashApply(const std::filesystem::path& repoPath,
               const gf::git::bo::StashApplyOptions& options) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    stashDrop(const std::filesystem::path& repoPath,
              std::size_t stashIndex) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    stashClear(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    startRebase(const std::filesystem::path& repoPath,
                const gf::git::bo::RebaseOptions& options) override;

    [[nodiscard]] std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    continueRebase(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<gf::git::bo::RebaseResult, gf::git::ports::GitOpsError>
    skipRebase(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    abortRebase(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<bool, gf::git::ports::GitOpsError>
    isRebaseInProgress(const std::filesystem::path& repoPath) override;

    [[nodiscard]] std::expected<gf::git::bo::CherryPickResult, gf::git::ports::GitOpsError>
    cherryPick(const std::filesystem::path& repoPath,
               const gf::git::bo::CherryPickOptions& options) override;

    [[nodiscard]] std::expected<void, gf::git::ports::GitOpsError>
    abortCherryPick(const std::filesystem::path& repoPath) override;

private:
    gf::git::ports::IGitCommandExecutor& m_executor;
};

} // namespace gf::infra::git
