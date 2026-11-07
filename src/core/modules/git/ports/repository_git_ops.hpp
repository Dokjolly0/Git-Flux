#pragma once

#include "core/modules/git/bo/branch_models.hpp"
#include "core/modules/git/bo/commit_options.hpp"
#include "core/modules/git/bo/diff_models.hpp"
#include "core/modules/git/bo/history_models.hpp"
#include "core/modules/git/bo/remote_merge_models.hpp"
#include "core/modules/git/bo/repo_working_tree_status.hpp"
#include "core/modules/git/bo/stash_models.hpp"
#include "core/modules/git/bo/rebase_models.hpp"
#include "core/modules/git/bo/conflict_models.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::git::ports {

struct GitOpsError {
    enum class Kind {
        InvalidRepositoryPath,
        PathNotFound,
        ExecutionFailed,
        IndexLocked,
        ConflictPresent,
        EmptyCommitMessage,
        NothingToCommit,
        BranchAlreadyExists,
        BranchNotFound,
        InvalidBranchName,
        DirtyWorkingTree,
        InvalidOperation,
        TagAlreadyExists,
        TagNotFound,
        InvalidTagName,
        RemoteNotFound,
        RemoteAlreadyExists,
        NetworkAuthenticationFailed,
        RemoteRejected,
        MergeConflict,
        StashNotFound,
        NothingToStash,
        RebaseInProgress,
        NoRebaseInProgress,
        CherryPickConflict,
        CommitNotFound,
        StorageError
    };

    Kind kind = Kind::ExecutionFailed;
    int exitCode = -1;
    std::string message;
    std::string command;
};

class IRepositoryGitOps {
public:
    virtual ~IRepositoryGitOps() = default;

    /// Returns the detailed, classified status of the repository working tree.
    [[nodiscard]] virtual std::expected<bo::RepoWorkingTreeStatus, GitOpsError>
    getWorkingTreeStatus(const std::filesystem::path& repoPath) = 0;

    /// Stages specific paths (`git add -- <paths>`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    stageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) = 0;

    /// Stages all working tree changes (`git add -A`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    stageAll(const std::filesystem::path& repoPath) = 0;

    /// Unstages specific paths from the index.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    unstageFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) = 0;

    /// Unstages all changes from the index.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    unstageAll(const std::filesystem::path& repoPath) = 0;

    /// Discards unstaged local modifications, restoring tracked files or removing untracked files.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    discardFiles(const std::filesystem::path& repoPath, const std::vector<std::string>& filePaths) = 0;

    /// Creates a commit with the currently staged files.
    [[nodiscard]] virtual std::expected<bo::CommitResult, GitOpsError>
    createCommit(const std::filesystem::path& repoPath, const bo::CommitOptions& options) = 0;

    /// Retrieves the diff for a specific file within the specified scope (staged, unstaged, or untracked).
    [[nodiscard]] virtual std::expected<bo::FileDiff, GitOpsError>
    getFileDiff(const std::filesystem::path& repoPath,
                const std::string& filePath,
                bo::DiffScope scope) = 0;

    /// Retrieves all diffs for the specified scope (staged or unstaged).
    [[nodiscard]] virtual std::expected<std::vector<bo::FileDiff>, GitOpsError>
    getAllDiffs(const std::filesystem::path& repoPath,
                bo::DiffScope scope) = 0;

    /// Retrieves the list of files modified, added, deleted or renamed by a commit.
    [[nodiscard]] virtual std::expected<std::vector<bo::CommitFileChange>, GitOpsError>
    getCommitFiles(const std::filesystem::path& repoPath,
                   const std::string& commitSha) {
        (void)repoPath;
        (void)commitSha;
        return std::vector<bo::CommitFileChange>{};
    }

    /// Retrieves the unified diff for a single file in a specific commit.
    [[nodiscard]] virtual std::expected<bo::FileDiff, GitOpsError>
    getCommitFileDiff(const std::filesystem::path& repoPath,
                      const std::string& commitSha,
                      const std::string& filePath) {
        (void)repoPath;
        (void)commitSha;
        (void)filePath;
        return bo::FileDiff{};
    }

    /// Retrieves the line-by-line blame annotation for a file at a specific revision (or HEAD if empty).
    [[nodiscard]] virtual std::expected<std::vector<bo::BlameLine>, GitOpsError>
    getFileBlame(const std::filesystem::path& repoPath,
                 const std::string& revision,
                 const std::string& filePath) {
        (void)repoPath;
        (void)revision;
        (void)filePath;
        return std::vector<bo::BlameLine>{};
    }

    /// Returns the list of local and remote branches in the repository.
    [[nodiscard]] virtual std::expected<std::vector<bo::GitBranch>, GitOpsError>
    listBranches(const std::filesystem::path& repoPath) = 0;

    /// Creates a new branch from an optional starting point.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    createBranch(const std::filesystem::path& repoPath,
                 const bo::CreateBranchOptions& options) = 0;

    /// Checks out / switches to a branch or commit.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    checkoutBranch(const std::filesystem::path& repoPath,
                   const bo::CheckoutOptions& options) = 0;

    /// Deletes an existing branch (with force flag for unmerged branches).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    deleteBranch(const std::filesystem::path& repoPath,
                 const std::string& branchName,
                 bool force) = 0;

    /// Renames an existing branch.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    renameBranch(const std::filesystem::path& repoPath,
                 const std::string& oldName,
                 const std::string& newName) = 0;

    /// Returns the commit log history with advanced filters and topological ordering.
    [[nodiscard]] virtual std::expected<std::vector<bo::GitCommitLogItem>, GitOpsError>
    getCommitLog(const std::filesystem::path& repoPath,
                 const bo::CommitLogOptions& options) = 0;

    /// Returns the list of local tags (lightweight and annotated).
    [[nodiscard]] virtual std::expected<std::vector<bo::GitTag>, GitOpsError>
    listTags(const std::filesystem::path& repoPath) = 0;

    /// Creates a new tag (lightweight if message is empty/null, annotated if provided).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    createTag(const std::filesystem::path& repoPath,
              const bo::CreateTagOptions& options) = 0;

    /// Deletes a local tag.
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    deleteTag(const std::filesystem::path& repoPath,
              const std::string& tagName) = 0;

    /// Returns the list of configured remotes (`git remote -v`).
    [[nodiscard]] virtual std::expected<std::vector<bo::GitRemote>, GitOpsError>
    listRemotes(const std::filesystem::path& repoPath) = 0;

    /// Adds a new remote (`git remote add <name> <url>`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    addRemote(const std::filesystem::path& repoPath,
              const std::string& name,
              const std::string& url) = 0;

    /// Removes a configured remote (`git remote remove <name>`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    removeRemote(const std::filesystem::path& repoPath,
                 const std::string& name) = 0;

    /// Updates the URL of an existing remote (`git remote set-url <name> <url>`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    setRemoteUrl(const std::filesystem::path& repoPath,
                 const std::string& name,
                 const std::string& url) {
        (void)repoPath;
        (void)name;
        (void)url;
        return {};
    }

    /// Fetches updates from a remote (`git fetch`).
    [[nodiscard]] virtual std::expected<bo::RemoteOperationResult, GitOpsError>
    fetchRemote(const std::filesystem::path& repoPath,
                const bo::RemoteOperationOptions& options) = 0;

    /// Pulls changes from a remote (`git pull`).
    [[nodiscard]] virtual std::expected<bo::RemoteOperationResult, GitOpsError>
    pullRemote(const std::filesystem::path& repoPath,
               const bo::RemoteOperationOptions& options) = 0;

    /// Pushes commits to a remote (`git push`).
    [[nodiscard]] virtual std::expected<bo::RemoteOperationResult, GitOpsError>
    pushRemote(const std::filesystem::path& repoPath,
               const bo::RemoteOperationOptions& options) = 0;

    /// Merges a branch or commit into the active branch (`git merge`).
    [[nodiscard]] virtual std::expected<bo::MergeResult, GitOpsError>
    mergeBranch(const std::filesystem::path& repoPath,
                const bo::MergeOptions& options) = 0;

    /// Aborts an in-progress merge (`git merge --abort`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    abortMerge(const std::filesystem::path& repoPath) = 0;

    /// Returns the list of available stashes (`git stash list`).
    [[nodiscard]] virtual std::expected<std::vector<bo::GitStashItem>, GitOpsError>
    listStashes(const std::filesystem::path& repoPath) = 0;

    /// Saves the current state into a new stash (`git stash push`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    stashPush(const std::filesystem::path& repoPath,
              const bo::StashPushOptions& options) = 0;

    /// Applies and drops a stash (`git stash pop`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    stashPop(const std::filesystem::path& repoPath,
             const bo::StashApplyOptions& options) = 0;

    /// Applies a stash while retaining it in the list (`git stash apply`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    stashApply(const std::filesystem::path& repoPath,
               const bo::StashApplyOptions& options) = 0;

    /// Drops a specific stash (`git stash drop`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    stashDrop(const std::filesystem::path& repoPath,
              std::size_t stashIndex) = 0;

    /// Clears all stashes (`git stash clear`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    stashClear(const std::filesystem::path& repoPath) = 0;

    /// Starts a rebase session (`git rebase`).
    [[nodiscard]] virtual std::expected<bo::RebaseResult, GitOpsError>
    startRebase(const std::filesystem::path& repoPath,
                const bo::RebaseOptions& options) = 0;

    /// Continues an in-progress rebase after resolving conflicts (`git rebase --continue`).
    [[nodiscard]] virtual std::expected<bo::RebaseResult, GitOpsError>
    continueRebase(const std::filesystem::path& repoPath) = 0;

    /// Skips the current commit during a rebase (`git rebase --skip`).
    [[nodiscard]] virtual std::expected<bo::RebaseResult, GitOpsError>
    skipRebase(const std::filesystem::path& repoPath) = 0;

    /// Aborts an in-progress rebase, restoring the previous state (`git rebase --abort`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    abortRebase(const std::filesystem::path& repoPath) = 0;

    /// Checks if a rebase is currently in progress (`.git/rebase-merge` or `.git/rebase-apply`).
    [[nodiscard]] virtual std::expected<bool, GitOpsError>
    isRebaseInProgress(const std::filesystem::path& repoPath) = 0;

    /// Cherry-picks a commit (`git cherry-pick`).
    [[nodiscard]] virtual std::expected<bo::CherryPickResult, GitOpsError>
    cherryPick(const std::filesystem::path& repoPath,
               const bo::CherryPickOptions& options) = 0;

    /// Aborts an in-progress cherry-pick (`git cherry-pick --abort`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    abortCherryPick(const std::filesystem::path& repoPath) = 0;

    /// Inspects a conflicted file and parses hunks with markers and stages.
    [[nodiscard]] virtual std::expected<bo::ConflictFile, GitOpsError>
    getConflictFile(const std::filesystem::path& repoPath,
                    const std::string& filePath) = 0;

    /// Retrieves the content of a specific stage for a file (1=base, 2=ours, 3=theirs) via `git show :<stage>:<path>`.
    [[nodiscard]] virtual std::expected<std::string, GitOpsError>
    getFileStageContent(const std::filesystem::path& repoPath,
                        const std::string& filePath,
                        int stage) = 0;

    /// Saves the resolved content of a file and optionally stages it (`git add`).
    [[nodiscard]] virtual std::expected<void, GitOpsError>
    applyConflictResolution(const std::filesystem::path& repoPath,
                            const bo::ApplyConflictResolutionOptions& options) = 0;
};

} // namespace gf::git::ports
