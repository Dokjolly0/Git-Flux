#pragma once

#include "core/modules/git/bo/batch_operation_models.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/tasks/bl/background_task_runner.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace gf::git::bl {

using BatchProgressCallback = std::function<void(const bo::BatchProgressUpdate&)>;
using BatchCompletionCallback = std::function<void(const bo::BatchOperationReport&)>;

class MultiRepoBatchCoordinator {
public:
    explicit MultiRepoBatchCoordinator(repositories::bl::RepositoryService& repoService,
                                       groups::bl::GroupService& groupService,
                                       workspaces::bl::WorkspaceService& workspaceService,
                                       GitRemoteMergeService& remoteMergeService,
                                       GitWorkingTreeService& workingTreeService,
                                       tasks::bl::BackgroundTaskRunner& taskRunner,
                                       std::size_t maxConcurrency = 4);
    ~MultiRepoBatchCoordinator() = default;

    MultiRepoBatchCoordinator(const MultiRepoBatchCoordinator&) = delete;
    MultiRepoBatchCoordinator& operator=(const MultiRepoBatchCoordinator&) = delete;
    MultiRepoBatchCoordinator(MultiRepoBatchCoordinator&&) = delete;
    MultiRepoBatchCoordinator& operator=(MultiRepoBatchCoordinator&&) = delete;

    [[nodiscard]] std::vector<bo::RepoBatchTarget>
    resolveTargets(const std::vector<std::string>& repoIds);

    [[nodiscard]] std::vector<bo::RepoBatchTarget>
    resolveGroupTargets(const std::string& groupId);

    [[nodiscard]] std::vector<bo::RepoBatchTarget>
    resolveWorkspaceTargets(const std::string& workspaceId);

    // Esecuzione Asincrona
    std::string executeBatchFetchAsync(const std::vector<bo::RepoBatchTarget>& targets,
                                       const std::string& remoteName = "origin",
                                       BatchProgressCallback onProgress = nullptr,
                                       BatchCompletionCallback onCompleted = nullptr);

    std::string executeGroupFetchAsync(const std::string& groupId,
                                       const std::string& remoteName = "origin",
                                       BatchProgressCallback onProgress = nullptr,
                                       BatchCompletionCallback onCompleted = nullptr);

    std::string executeWorkspaceFetchAsync(const std::string& workspaceId,
                                           const std::string& remoteName = "origin",
                                           BatchProgressCallback onProgress = nullptr,
                                           BatchCompletionCallback onCompleted = nullptr);

    std::string executeBatchPullAsync(const std::vector<bo::RepoBatchTarget>& targets,
                                      const std::string& remoteName = "origin",
                                      BatchProgressCallback onProgress = nullptr,
                                      BatchCompletionCallback onCompleted = nullptr);

    std::string executeBatchStatusAsync(const std::vector<bo::RepoBatchTarget>& targets,
                                        BatchProgressCallback onProgress = nullptr,
                                        BatchCompletionCallback onCompleted = nullptr);

    // Esecuzione Sincrona
    bo::BatchOperationReport executeBatchFetchSync(const std::vector<bo::RepoBatchTarget>& targets,
                                                  const std::string& remoteName = "origin",
                                                  BatchProgressCallback onProgress = nullptr);

    bo::BatchOperationReport executeBatchStatusSync(const std::vector<bo::RepoBatchTarget>& targets,
                                                   BatchProgressCallback onProgress = nullptr);

    bool cancelBatch(const std::string& operationId);

    [[nodiscard]] std::size_t maxConcurrency() const noexcept { return m_maxConcurrency; }
    void setMaxConcurrency(std::size_t limit) noexcept { m_maxConcurrency = (limit == 0 ? 1 : limit); }

private:
    bo::BatchOperationReport runBatchLoop(const std::string& operationId,
                                         bo::BatchOperationType opType,
                                         const std::vector<bo::RepoBatchTarget>& targets,
                                         const std::string& remoteName,
                                         BatchProgressCallback onProgress,
                                         tasks::ports::ITaskProgressSink* sink);

    repositories::bl::RepositoryService& m_repoService;
    groups::bl::GroupService& m_groupService;
    workspaces::bl::WorkspaceService& m_workspaceService;
    GitRemoteMergeService& m_remoteMergeService;
    GitWorkingTreeService& m_workingTreeService;
    tasks::bl::BackgroundTaskRunner& m_taskRunner;

    std::size_t m_maxConcurrency{4};
    std::atomic<uint64_t> m_nextOpId{1};

    mutable std::mutex m_activeOpsMutex;
    std::unordered_map<std::string, std::string> m_opToTaskId;
};

} // namespace gf::git::bl
