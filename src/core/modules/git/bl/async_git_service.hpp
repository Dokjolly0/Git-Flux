#pragma once

#include "core/modules/git/bo/remote_merge_models.hpp"
#include "core/modules/git/bo/repo_working_tree_status.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/tasks/bl/background_task_runner.hpp"

#include <filesystem>
#include <functional>
#include <string>

namespace gf::git::bl
{
    using AsyncProgressCallback = std::function<void(const tasks::bo::TaskProgress &)>;

    using AsyncFetchCallback = std::function<void(std::expected<bo::RemoteOperationResult, ports::GitOpsError>)>;
    using AsyncPullCallback = std::function<void(std::expected<bo::RemoteOperationResult, ports::GitOpsError>)>;
    using AsyncPushCallback = std::function<void(std::expected<bo::RemoteOperationResult, ports::GitOpsError>)>;
    using AsyncWorkingTreeCallback = std::function<void(std::expected<bo::RepoWorkingTreeStatus, ports::GitOpsError>)>;

    class AsyncGitService
    {
    public:
        explicit AsyncGitService(tasks::bl::BackgroundTaskRunner& taskRunner,
                                 GitRemoteMergeService& remoteMergeService,
                                 GitWorkingTreeService& workingTreeService);
        ~AsyncGitService() = default;

        AsyncGitService(const AsyncGitService&) = delete;
        AsyncGitService& operator=(const AsyncGitService&) = delete;
        AsyncGitService(AsyncGitService&&) = delete;
        AsyncGitService& operator=(AsyncGitService&&) = delete;

        std::string fetchRemoteAsync(const std::filesystem::path& repoPath,
                                     const bo::RemoteOperationOptions& options,
                                     AsyncFetchCallback onCompleted = nullptr,
                                     AsyncProgressCallback onProgress = nullptr);

        std::string pullRemoteAsync(const std::filesystem::path& repoPath,
                                    const bo::RemoteOperationOptions& options,
                                    AsyncPullCallback onCompleted = nullptr,
                                    AsyncProgressCallback onProgress = nullptr);

        std::string pushRemoteAsync(const std::filesystem::path& repoPath,
                                    const bo::RemoteOperationOptions& options,
                                    AsyncPushCallback onCompleted = nullptr,
                                    AsyncProgressCallback onProgress = nullptr);

        std::string refreshWorkingTreeAsync(const std::filesystem::path& repoPath,
                                            AsyncWorkingTreeCallback onCompleted = nullptr,
                                            AsyncProgressCallback onProgress = nullptr);

        bool cancelOperation(const std::string& taskId);

        [[nodiscard]] tasks::bl::BackgroundTaskRunner& taskRunner() noexcept { return m_taskRunner; }

    private:
        tasks::bl::BackgroundTaskRunner& m_taskRunner;
        GitRemoteMergeService& m_remoteMergeService;
        GitWorkingTreeService& m_workingTreeService;
    };
} // namespace gf::git::bl
