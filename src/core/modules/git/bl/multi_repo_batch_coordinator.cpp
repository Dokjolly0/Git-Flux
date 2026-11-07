#include "core/modules/git/bl/multi_repo_batch_coordinator.hpp"

#include <algorithm>
#include <chrono>
#include <future>
#include <unordered_set>

namespace gf::git::bl {

MultiRepoBatchCoordinator::MultiRepoBatchCoordinator(repositories::bl::RepositoryService& repoService,
                                                     groups::bl::GroupService& groupService,
                                                     workspaces::bl::WorkspaceService& workspaceService,
                                                     GitRemoteMergeService& remoteMergeService,
                                                     GitWorkingTreeService& workingTreeService,
                                                     tasks::bl::BackgroundTaskRunner& taskRunner,
                                                     std::size_t maxConcurrency)
    : m_repoService(repoService)
    , m_groupService(groupService)
    , m_workspaceService(workspaceService)
    , m_remoteMergeService(remoteMergeService)
    , m_workingTreeService(workingTreeService)
    , m_taskRunner(taskRunner)
    , m_maxConcurrency(maxConcurrency == 0 ? 1 : maxConcurrency) {}

std::vector<bo::RepoBatchTarget>
MultiRepoBatchCoordinator::resolveTargets(const std::vector<std::string>& repoIds) {
    std::vector<bo::RepoBatchTarget> targets;
    std::unordered_set<std::string> seenIds;

    for (const auto& idStr : repoIds) {
        if (seenIds.contains(idStr)) {
            continue;
        }

        auto repoRes = m_repoService.getRepository(repositories::bo::RepositoryId{idStr});
        if (repoRes && repoRes->has_value()) {
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
MultiRepoBatchCoordinator::resolveGroupTargets(const std::string& groupId) {
    std::vector<bo::RepoBatchTarget> targets;
    auto ctxRes = m_groupService.resolveQuickOpenContext({groups::bo::GroupId{groupId}});

    if (ctxRes) {
        for (const auto& r : ctxRes->repositories) {
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
MultiRepoBatchCoordinator::resolveWorkspaceTargets(const std::string& workspaceId) {
    std::vector<bo::RepoBatchTarget> targets;
    auto res = m_workspaceService.resolveWorkspaceRepositories(workspaces::bo::WorkspaceId{workspaceId});

    if (res) {
        for (const auto& r : *res) {
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
                                        bo::BatchOperationType opType,
                                        const std::vector<bo::RepoBatchTarget>& targets,
                                        const std::string& remoteName,
                                        BatchProgressCallback onProgress,
                                        tasks::ports::ITaskProgressSink* sink) {
    auto overallStart = std::chrono::steady_clock::now();
    bo::BatchOperationReport report{
        .operationId = operationId,
        .operationType = opType,
        .totalCount = targets.size(),
        .succeededCount = 0,
        .failedCount = 0,
        .skippedCount = 0,
        .items = {},
        .totalDurationMs = std::chrono::milliseconds(0)
    };

    if (targets.empty()) {
        return report;
    }

    report.items.reserve(targets.size());
    std::size_t completed = 0;

    for (const auto& target : targets) {
        if (sink && sink->isCancelled()) {
            report.items.push_back(bo::RepoBatchItemResult{
                .repoId = target.repoId,
                .repoName = target.repoName,
                .repoPath = target.repoPath,
                .status = bo::RepoBatchItemStatus::Skipped,
                .message = "Operazione batch annullata",
                .durationMs = std::chrono::milliseconds(0)
            });
            report.skippedCount++;
            completed++;
            continue;
        }

        auto itemStart = std::chrono::steady_clock::now();
        bo::RepoBatchItemResult itemResult{
            .repoId = target.repoId,
            .repoName = target.repoName,
            .repoPath = target.repoPath,
            .status = bo::RepoBatchItemStatus::Running,
            .message = {},
            .durationMs = std::chrono::milliseconds(0)
        };

        if (onProgress) {
            onProgress(bo::BatchProgressUpdate{
                .operationId = operationId,
                .completedCount = completed,
                .totalCount = targets.size(),
                .currentRepoName = target.repoName,
                .currentMessage = "Esecuzione su " + target.repoName + "..."
            });
        }

        switch (opType) {
        case bo::BatchOperationType::Fetch: {
            auto res = m_remoteMergeService.fetchRemote(target.repoPath, bo::RemoteOperationOptions{.remoteName = remoteName});
            if (res) {
                itemResult.status = bo::RepoBatchItemStatus::Succeeded;
                itemResult.message = "Fetch completato con successo";
                report.succeededCount++;
            } else {
                itemResult.status = bo::RepoBatchItemStatus::Failed;
                itemResult.message = res.error().message;
                report.failedCount++;
            }
            break;
        }
        case bo::BatchOperationType::Pull: {
            auto res = m_remoteMergeService.pullRemote(target.repoPath, bo::RemoteOperationOptions{.remoteName = remoteName});
            if (res) {
                itemResult.status = bo::RepoBatchItemStatus::Succeeded;
                itemResult.message = "Pull completato con successo";
                report.succeededCount++;
            } else {
                itemResult.status = bo::RepoBatchItemStatus::Failed;
                itemResult.message = res.error().message;
                report.failedCount++;
            }
            break;
        }
        case bo::BatchOperationType::Status: {
            auto res = m_workingTreeService.getWorkingTreeStatus(target.repoPath);
            if (res) {
                itemResult.status = bo::RepoBatchItemStatus::Succeeded;
                itemResult.message = res->isClean() ? "Working tree pulito" : (std::to_string(res->totalChangesCount()) + " modifiche");
                report.succeededCount++;
            } else {
                itemResult.status = bo::RepoBatchItemStatus::Failed;
                itemResult.message = res.error().message;
                report.failedCount++;
            }
            break;
        }
        }

        auto itemEnd = std::chrono::steady_clock::now();
        itemResult.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(itemEnd - itemStart);
        report.items.push_back(std::move(itemResult));

        completed++;

        if (onProgress) {
            onProgress(bo::BatchProgressUpdate{
                .operationId = operationId,
                .completedCount = completed,
                .totalCount = targets.size(),
                .currentRepoName = target.repoName,
                .currentMessage = "Completato " + target.repoName
            });
        }

        if (sink) {
            int pct = targets.empty() ? 100 : static_cast<int>((completed * 100) / targets.size());
            sink->reportProgress(pct, "Completato " + std::to_string(completed) + "/" + std::to_string(targets.size()));
        }
    }

    auto overallEnd = std::chrono::steady_clock::now();
    report.totalDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(overallEnd - overallStart);

    return report;
}

std::string
MultiRepoBatchCoordinator::executeBatchFetchAsync(const std::vector<bo::RepoBatchTarget>& targets,
                                                 const std::string& remoteName,
                                                 BatchProgressCallback onProgress,
                                                 BatchCompletionCallback onCompleted) {
    uint64_t idNum = m_nextOpId.fetch_add(1);
    std::string opId = "batch-fetch-" + std::to_string(idNum);

    std::string taskId = m_taskRunner.postTask(
        opId,
        [this, opId, targets, remoteName, onProgress, onCompleted](tasks::ports::ITaskProgressSink& sink) -> std::expected<void, std::string> {
            auto report = runBatchLoop(opId, bo::BatchOperationType::Fetch, targets, remoteName, onProgress, &sink);

            {
                std::lock_guard<std::mutex> lock(m_activeOpsMutex);
                m_opToTaskId.erase(opId);
            }

            if (onCompleted) {
                onCompleted(report);
            }

            if (report.hasFailures()) {
                return std::unexpected(std::to_string(report.failedCount) + " repository hanno riportato errori");
            }
            return {};
        },
        tasks::bo::TaskPriority::Normal
    );

    {
        std::lock_guard<std::mutex> lock(m_activeOpsMutex);
        m_opToTaskId[opId] = taskId;
    }

    return opId;
}

std::string
MultiRepoBatchCoordinator::executeGroupFetchAsync(const std::string& groupId,
                                                 const std::string& remoteName,
                                                 BatchProgressCallback onProgress,
                                                 BatchCompletionCallback onCompleted) {
    auto targets = resolveGroupTargets(groupId);
    return executeBatchFetchAsync(targets, remoteName, onProgress, onCompleted);
}

std::string
MultiRepoBatchCoordinator::executeWorkspaceFetchAsync(const std::string& workspaceId,
                                                     const std::string& remoteName,
                                                     BatchProgressCallback onProgress,
                                                     BatchCompletionCallback onCompleted) {
    auto targets = resolveWorkspaceTargets(workspaceId);
    return executeBatchFetchAsync(targets, remoteName, onProgress, onCompleted);
}

std::string
MultiRepoBatchCoordinator::executeBatchPullAsync(const std::vector<bo::RepoBatchTarget>& targets,
                                                const std::string& remoteName,
                                                BatchProgressCallback onProgress,
                                                BatchCompletionCallback onCompleted) {
    uint64_t idNum = m_nextOpId.fetch_add(1);
    std::string opId = "batch-pull-" + std::to_string(idNum);

    std::string taskId = m_taskRunner.postTask(
        opId,
        [this, opId, targets, remoteName, onProgress, onCompleted](tasks::ports::ITaskProgressSink& sink) -> std::expected<void, std::string> {
            auto report = runBatchLoop(opId, bo::BatchOperationType::Pull, targets, remoteName, onProgress, &sink);

            {
                std::lock_guard<std::mutex> lock(m_activeOpsMutex);
                m_opToTaskId.erase(opId);
            }

            if (onCompleted) {
                onCompleted(report);
            }

            if (report.hasFailures()) {
                return std::unexpected(std::to_string(report.failedCount) + " repository hanno riportato errori");
            }
            return {};
        },
        tasks::bo::TaskPriority::Normal
    );

    {
        std::lock_guard<std::mutex> lock(m_activeOpsMutex);
        m_opToTaskId[opId] = taskId;
    }

    return opId;
}

std::string
MultiRepoBatchCoordinator::executeBatchStatusAsync(const std::vector<bo::RepoBatchTarget>& targets,
                                                  BatchProgressCallback onProgress,
                                                  BatchCompletionCallback onCompleted) {
    uint64_t idNum = m_nextOpId.fetch_add(1);
    std::string opId = "batch-status-" + std::to_string(idNum);

    std::string taskId = m_taskRunner.postTask(
        opId,
        [this, opId, targets, onProgress, onCompleted](tasks::ports::ITaskProgressSink& sink) -> std::expected<void, std::string> {
            auto report = runBatchLoop(opId, bo::BatchOperationType::Status, targets, "", onProgress, &sink);

            {
                std::lock_guard<std::mutex> lock(m_activeOpsMutex);
                m_opToTaskId.erase(opId);
            }

            if (onCompleted) {
                onCompleted(report);
            }

            if (report.hasFailures()) {
                return std::unexpected(std::to_string(report.failedCount) + " repository hanno riportato errori");
            }
            return {};
        },
        tasks::bo::TaskPriority::High
    );

    {
        std::lock_guard<std::mutex> lock(m_activeOpsMutex);
        m_opToTaskId[opId] = taskId;
    }

    return opId;
}

bo::BatchOperationReport
MultiRepoBatchCoordinator::executeBatchFetchSync(const std::vector<bo::RepoBatchTarget>& targets,
                                                const std::string& remoteName,
                                                BatchProgressCallback onProgress) {
    uint64_t idNum = m_nextOpId.fetch_add(1);
    std::string opId = "batch-fetch-sync-" + std::to_string(idNum);
    return runBatchLoop(opId, bo::BatchOperationType::Fetch, targets, remoteName, onProgress, nullptr);
}

bo::BatchOperationReport
MultiRepoBatchCoordinator::executeBatchStatusSync(const std::vector<bo::RepoBatchTarget>& targets,
                                                 BatchProgressCallback onProgress) {
    uint64_t idNum = m_nextOpId.fetch_add(1);
    std::string opId = "batch-status-sync-" + std::to_string(idNum);
    return runBatchLoop(opId, bo::BatchOperationType::Status, targets, "", onProgress, nullptr);
}

bool MultiRepoBatchCoordinator::cancelBatch(const std::string& operationId) {
    std::lock_guard<std::mutex> lock(m_activeOpsMutex);
    auto it = m_opToTaskId.find(operationId);
    if (it != m_opToTaskId.end()) {
        return m_taskRunner.cancelTask(it->second);
    }
    return false;
}

} // namespace gf::git::bl
