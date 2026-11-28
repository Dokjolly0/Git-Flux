#include "core/modules/git/bl/multi_repo_batch_coordinator.hpp"

#include <chrono>
#include <ranges>
#include <unordered_set>

namespace gf::git::bl
{
    MultiRepoBatchCoordinator::MultiRepoBatchCoordinator(repositories::bl::RepositoryService& repoService,
                                                         groups::bl::GroupService& groupService,
                                                         workspaces::bl::WorkspaceService& workspaceService,
                                                         GitRemoteMergeService& remoteMergeService,
                                                         GitWorkingTreeService& workingTreeService,
                                                         tasks::bl::BackgroundTaskRunner& taskRunner)
        : m_repoService(repoService)
          , m_groupService(groupService)
          , m_workspaceService(workspaceService)
          , m_remoteMergeService(remoteMergeService)
          , m_workingTreeService(workingTreeService)
          , m_taskRunner(taskRunner)
    {
    }

    MultiRepoBatchCoordinator::~MultiRepoBatchCoordinator()
    {
        {
            std::lock_guard lock(m_lifetime->mutex);
            m_lifetime->alive = false; // queued tasks that start from now on exit immediately
        }
        {
            std::lock_guard lock(m_activeOpsMutex);
            for (const auto& taskId : m_opToTaskId | std::views::values)
            {
                (void)m_taskRunner.cancelTask(taskId);
            }
        }
        // Tasks already inside their body still use this object: wait for them to leave.
        std::unique_lock lock(m_lifetime->mutex);
        m_lifetime->cv.wait(lock, [this] { return m_lifetime->running == 0; });
    }

    tasks::bl::TaskExecutable MultiRepoBatchCoordinator::guardTask(tasks::bl::TaskExecutable body) const
    {
        return [state = m_lifetime, body = std::move(body)](
            tasks::ports::ITaskProgressSink& sink) -> std::expected<void, std::string>
        {
            {
                std::lock_guard lock(state->mutex);
                if (!state->alive)
                {
                    return std::unexpected(std::string("Coordinatore batch non più disponibile"));
                }
                ++state->running;
            }

            struct RunningScope
            {
                LifetimeState& state;
                ~RunningScope()
                {
                    {
                        std::lock_guard lock(state.mutex);
                        --state.running;
                    }
                    state.cv.notify_all();
                }
            } scope{*state};

            return body(sink);
        };
    }

    std::vector<bo::RepoBatchTarget>
    MultiRepoBatchCoordinator::resolveTargets(const std::vector<std::string>& repoIds) const
    {
        std::vector<bo::RepoBatchTarget> targets;
        std::unordered_set<std::string> seenIds;

        for (const auto& idStr : repoIds)
        {
            if (seenIds.contains(idStr))
            {
                continue;
            }

            if (const auto repoRes = m_repoService.getRepository(repositories::bo::RepositoryId{idStr});
                repoRes && repoRes->has_value())
            {
                const auto& r = **repoRes;
                targets.push_back(bo::RepoBatchTarget{
                    .repoId = r.id.value(),
                    .repoName = r.displayName,
                    .repoPath = r.localPath
                });
                seenIds.insert(idStr);
            }
        }

        return targets;
    }

    std::vector<bo::RepoBatchTarget>
    MultiRepoBatchCoordinator::resolveGroupTargets(const std::string& groupId) const
    {
        std::vector<bo::RepoBatchTarget> targets;

        if (const auto ctxRes = m_groupService.resolveQuickOpenContext({groups::bo::GroupId{groupId}}); ctxRes)
        {
            for (const auto& r : ctxRes->repositories)
            {
                targets.push_back(bo::RepoBatchTarget{
                    .repoId = r.id.value(),
                    .repoName = r.displayName,
                    .repoPath = r.localPath
                });
            }
        }

        return targets;
    }

    std::vector<bo::RepoBatchTarget>
    MultiRepoBatchCoordinator::resolveWorkspaceTargets(const std::string& workspaceId) const
    {
        std::vector<bo::RepoBatchTarget> targets;

        if (const auto res = m_workspaceService.resolveWorkspaceRepositories(workspaces::bo::WorkspaceId{workspaceId});
            res)
        {
            for (const auto& r : *res)
            {
                targets.push_back(bo::RepoBatchTarget{
                    .repoId = r.id.value(),
                    .repoName = r.displayName,
                    .repoPath = r.localPath
                });
            }
        }

        return targets;
    }

    bo::BatchOperationReport
    MultiRepoBatchCoordinator::runBatchLoop(const std::string& operationId,
                                            const bo::BatchOperationType opType,
                                            const std::vector<bo::RepoBatchTarget>& targets,
                                            const std::string& remoteName,
                                            const BatchProgressCallback& onProgress,
                                            tasks::ports::ITaskProgressSink* sink) const
    {
        bo::BatchOperationReport report{
            .operationId = operationId,
            .operationType = opType,
            .totalCount = targets.size(),
            .succeededCount = 0,
            .failedCount = 0,
            .skippedCount = 0,
            .items = {}
        };

        if (targets.empty())
        {
            return report;
        }

        report.items.reserve(targets.size());
        std::size_t completed = 0;

        for (const auto& [repoId, repoName, repoPath] : targets)
        {
            if (sink && sink->isCancelled())
            {
                report.items.push_back(bo::RepoBatchItemResult{
                    .repoId = repoId,
                    .repoName = repoName,
                    .repoPath = repoPath,
                    .status = bo::RepoBatchItemStatus::Skipped,
                    .message = "Operazione batch annullata",
                    .durationMs = std::chrono::milliseconds(0)
                });
                report.skippedCount++;
                completed++;
                continue;
            }

            const auto itemStart = std::chrono::steady_clock::now();
            bo::RepoBatchItemResult itemResult{
                .repoId = repoId,
                .repoName = repoName,
                .repoPath = repoPath,
                .status = bo::RepoBatchItemStatus::Running,
                .message = {},
                .durationMs = std::chrono::milliseconds(0)
            };

            if (onProgress)
            {
                onProgress(bo::BatchProgressUpdate{
                    .operationId = operationId,
                    .completedCount = completed,
                    .totalCount = targets.size(),
                    .currentRepoName = repoName,
                    .currentMessage = "Esecuzione su " + repoName + "..."
                });
            }

            switch (opType)
            {
            case bo::BatchOperationType::Fetch:
                {
                    if (const auto res = m_remoteMergeService.fetchRemote(
                            repoPath, bo::RemoteOperationOptions{.remoteName = remoteName});
                        res)
                    {
                        itemResult.status = bo::RepoBatchItemStatus::Succeeded;
                        itemResult.message = "Fetch completato con successo";
                        report.succeededCount++;
                    }
                    else
                    {
                        itemResult.status = bo::RepoBatchItemStatus::Failed;
                        itemResult.message = res.error().message;
                        report.failedCount++;
                    }
                    break;
                }
            case bo::BatchOperationType::Status:
                {
                    if (const auto res = m_workingTreeService.getWorkingTreeStatus(repoPath); res)
                    {
                        itemResult.status = bo::RepoBatchItemStatus::Succeeded;
                        itemResult.message = res->isClean()
                                                 ? "Working tree pulito"
                                                 : std::to_string(res->totalChangesCount()) + " modifiche";
                        report.succeededCount++;
                    }
                    else
                    {
                        itemResult.status = bo::RepoBatchItemStatus::Failed;
                        itemResult.message = res.error().message;
                        report.failedCount++;
                    }
                    break;
                }
            }

            const auto itemEnd = std::chrono::steady_clock::now();
            itemResult.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(itemEnd - itemStart);
            report.items.push_back(std::move(itemResult));

            completed++;

            if (onProgress)
            {
                onProgress(bo::BatchProgressUpdate{
                    .operationId = operationId,
                    .completedCount = completed,
                    .totalCount = targets.size(),
                    .currentRepoName = repoName,
                    .currentMessage = "Completato " + repoName
                });
            }

            if (sink)
            {
                const int pct = targets.empty() ? 100 : static_cast<int>(completed * 100 / targets.size());
                sink->reportProgress(
                    pct, "Completato " + std::to_string(completed) + "/" + std::to_string(targets.size()));
            }
        }

        return report;
    }

    std::string
    MultiRepoBatchCoordinator::executeBatchAsync(const bo::BatchOperationType opType,
                                                 const std::string_view opPrefix,
                                                 const std::vector<bo::RepoBatchTarget>& targets,
                                                 const std::string& remoteName,
                                                 const tasks::bo::TaskPriority priority,
                                                 const BatchProgressCallback& onProgress,
                                                 const BatchCompletionCallback& onCompleted)
    {
        const uint64_t idNum = m_nextOpId.fetch_add(1);
        std::string opId = std::string(opPrefix) + "-" + std::to_string(idNum);

        std::lock_guard opsLock(m_activeOpsMutex); // see m_opToTaskId
        const std::string taskId = m_taskRunner.postTask(
            opId,
            guardTask([this, opId, opType, targets, remoteName, onProgress, onCompleted](
            tasks::ports::ITaskProgressSink& sink) -> std::expected<void, std::string>
            {
                const auto report =
                    runBatchLoop(opId, opType, targets, remoteName, onProgress, &sink);

                {
                    std::lock_guard lock(m_activeOpsMutex);
                    m_opToTaskId.erase(opId);
                }

                if (onCompleted)
                {
                    onCompleted(report);
                }

                if (report.hasFailures())
                {
                    return std::unexpected(std::to_string(report.failedCount) + " repository hanno riportato errori");
                }
                return {};
            }),
            priority
        );
        m_opToTaskId[opId] = taskId;

        return opId;
    }

    std::string
    MultiRepoBatchCoordinator::executeBatchFetchAsync(const std::vector<bo::RepoBatchTarget>& targets,
                                                      const std::string& remoteName,
                                                      const BatchProgressCallback& onProgress,
                                                      const BatchCompletionCallback& onCompleted)
    {
        return executeBatchAsync(bo::BatchOperationType::Fetch, "batch-fetch", targets, remoteName,
                                 tasks::bo::TaskPriority::Normal, onProgress, onCompleted);
    }

    std::string
    MultiRepoBatchCoordinator::executeGroupFetchAsync(const std::string& groupId,
                                                      const std::string& remoteName,
                                                      const BatchProgressCallback& onProgress,
                                                      const BatchCompletionCallback& onCompleted)
    {
        const auto targets = resolveGroupTargets(groupId);
        return executeBatchFetchAsync(targets, remoteName, onProgress, onCompleted);
    }

    std::string
    MultiRepoBatchCoordinator::executeWorkspaceFetchAsync(const std::string& workspaceId,
                                                          const std::string& remoteName,
                                                          const BatchProgressCallback& onProgress,
                                                          const BatchCompletionCallback& onCompleted)
    {
        const auto targets = resolveWorkspaceTargets(workspaceId);
        return executeBatchFetchAsync(targets, remoteName, onProgress, onCompleted);
    }

    std::string
    MultiRepoBatchCoordinator::executeBatchStatusAsync(const std::vector<bo::RepoBatchTarget>& targets,
                                                       const BatchProgressCallback& onProgress,
                                                       const BatchCompletionCallback& onCompleted)
    {
        return executeBatchAsync(bo::BatchOperationType::Status, "batch-status", targets, "",
                                 tasks::bo::TaskPriority::High, onProgress, onCompleted);
    }

    bo::BatchOperationReport
    MultiRepoBatchCoordinator::executeBatchFetchSync(const std::vector<bo::RepoBatchTarget>& targets,
                                                     const std::string& remoteName,
                                                     const BatchProgressCallback& onProgress)
    {
        const uint64_t idNum = m_nextOpId.fetch_add(1);
        const std::string opId = "batch-fetch-sync-" + std::to_string(idNum);
        return runBatchLoop(opId, bo::BatchOperationType::Fetch, targets, remoteName, onProgress, nullptr);
    }

    bo::BatchOperationReport
    MultiRepoBatchCoordinator::executeBatchStatusSync(const std::vector<bo::RepoBatchTarget>& targets,
                                                      const BatchProgressCallback& onProgress)
    {
        const uint64_t idNum = m_nextOpId.fetch_add(1);
        const std::string opId = "batch-status-sync-" + std::to_string(idNum);
        return runBatchLoop(opId, bo::BatchOperationType::Status, targets, "", onProgress, nullptr);
    }

    bool MultiRepoBatchCoordinator::cancelBatch(const std::string& operationId) const
    {
        std::lock_guard lock(m_activeOpsMutex);
        if (const auto it = m_opToTaskId.find(operationId); it != m_opToTaskId.end())
        {
            return m_taskRunner.cancelTask(it->second);
        }
        return false;
    }
} // namespace gf::git::bl
