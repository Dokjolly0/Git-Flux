#include "core/modules/git/bl/async_git_service.hpp"

namespace gf::git::bl {

AsyncGitService::AsyncGitService(tasks::bl::BackgroundTaskRunner& taskRunner,
                                 GitRemoteMergeService& remoteMergeService,
                                 GitWorkingTreeService& workingTreeService)
    : m_taskRunner(taskRunner)
    , m_remoteMergeService(remoteMergeService)
    , m_workingTreeService(workingTreeService) {}

std::string AsyncGitService::fetchRemoteAsync(const std::filesystem::path& repoPath,
                                             const bo::RemoteOperationOptions& options,
                                             AsyncFetchCallback onCompleted,
                                             AsyncProgressCallback onProgress) {
    std::string taskName = "fetch (" + options.remoteName + ") on " + repoPath.filename().string();

    return m_taskRunner.postTask(
        taskName,
        [this, repoPath, options, onCompleted](tasks::ports::ITaskProgressSink& sink) -> std::expected<void, std::string> {
            sink.reportProgress(10, "Avvio fetch su remote...");

            if (sink.isCancelled()) {
                if (onCompleted) {
                    onCompleted(std::unexpected(ports::GitOpsError{
                        .kind = ports::GitOpsError::Kind::InvalidOperation,
                        .exitCode = -1,
                        .message = "Operazione annullata",
                        .command = ""
                    }));
                }
                return std::unexpected("Operazione annullata");
            }

            auto res = m_remoteMergeService.fetchRemote(repoPath, options);
            sink.reportProgress(90, "Finalizzazione fetch...");

            if (onCompleted) {
                onCompleted(res);
            }

            if (!res.has_value()) {
                return std::unexpected(res.error().message);
            }
            return {};
        },
        tasks::bo::TaskPriority::Normal,
        onProgress,
        nullptr
    );
}

std::string AsyncGitService::pullRemoteAsync(const std::filesystem::path& repoPath,
                                            const bo::RemoteOperationOptions& options,
                                            AsyncPullCallback onCompleted,
                                            AsyncProgressCallback onProgress) {
    std::string taskName = "pull (" + options.remoteName + ") on " + repoPath.filename().string();

    return m_taskRunner.postTask(
        taskName,
        [this, repoPath, options, onCompleted](tasks::ports::ITaskProgressSink& sink) -> std::expected<void, std::string> {
            sink.reportProgress(10, "Avvio pull su remote...");

            if (sink.isCancelled()) {
                if (onCompleted) {
                    onCompleted(std::unexpected(ports::GitOpsError{
                        .kind = ports::GitOpsError::Kind::InvalidOperation,
                        .exitCode = -1,
                        .message = "Operazione annullata",
                        .command = ""
                    }));
                }
                return std::unexpected("Operazione annullata");
            }

            auto res = m_remoteMergeService.pullRemote(repoPath, options);
            sink.reportProgress(90, "Finalizzazione pull...");

            if (onCompleted) {
                onCompleted(res);
            }

            if (!res.has_value()) {
                return std::unexpected(res.error().message);
            }
            return {};
        },
        tasks::bo::TaskPriority::Normal,
        onProgress,
        nullptr
    );
}

std::string AsyncGitService::pushRemoteAsync(const std::filesystem::path& repoPath,
                                            const bo::RemoteOperationOptions& options,
                                            AsyncPushCallback onCompleted,
                                            AsyncProgressCallback onProgress) {
    std::string taskName = "push (" + options.remoteName + ") on " + repoPath.filename().string();

    return m_taskRunner.postTask(
        taskName,
        [this, repoPath, options, onCompleted](tasks::ports::ITaskProgressSink& sink) -> std::expected<void, std::string> {
            sink.reportProgress(10, "Avvio push su remote...");

            if (sink.isCancelled()) {
                if (onCompleted) {
                    onCompleted(std::unexpected(ports::GitOpsError{
                        .kind = ports::GitOpsError::Kind::InvalidOperation,
                        .exitCode = -1,
                        .message = "Operazione annullata",
                        .command = ""
                    }));
                }
                return std::unexpected("Operazione annullata");
            }

            auto res = m_remoteMergeService.pushRemote(repoPath, options);
            sink.reportProgress(90, "Finalizzazione push...");

            if (onCompleted) {
                onCompleted(res);
            }

            if (!res.has_value()) {
                return std::unexpected(res.error().message);
            }
            return {};
        },
        tasks::bo::TaskPriority::Normal,
        onProgress,
        nullptr
    );
}

std::string AsyncGitService::refreshWorkingTreeAsync(const std::filesystem::path& repoPath,
                                                    AsyncWorkingTreeCallback onCompleted,
                                                    AsyncProgressCallback onProgress) {
    std::string taskName = "status on " + repoPath.filename().string();

    return m_taskRunner.postTask(
        taskName,
        [this, repoPath, onCompleted](tasks::ports::ITaskProgressSink& sink) -> std::expected<void, std::string> {
            sink.reportProgress(15, "Scansione working tree status...");

            if (sink.isCancelled()) {
                if (onCompleted) {
                    onCompleted(std::unexpected(ports::GitOpsError{
                        .kind = ports::GitOpsError::Kind::InvalidOperation,
                        .exitCode = -1,
                        .message = "Operazione annullata",
                        .command = ""
                    }));
                }
                return std::unexpected("Operazione annullata");
            }

            auto res = m_workingTreeService.getWorkingTreeStatus(repoPath);
            sink.reportProgress(95, "Elaborazione file modificati completata");

            if (onCompleted) {
                onCompleted(res);
            }

            if (!res.has_value()) {
                return std::unexpected(res.error().message);
            }
            return {};
        },
        tasks::bo::TaskPriority::High, // Scansione status priorità alta per UI
        onProgress,
        nullptr
    );
}

bool AsyncGitService::cancelOperation(const std::string& taskId) {
    return m_taskRunner.cancelTask(taskId);
}

} // namespace gf::git::bl
