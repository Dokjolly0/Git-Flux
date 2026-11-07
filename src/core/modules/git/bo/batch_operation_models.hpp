#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::git::bo {

enum class BatchOperationType {
    Fetch,
    Pull,
    Status
};

enum class RepoBatchItemStatus {
    Pending,
    Running,
    Succeeded,
    Failed,
    Skipped
};

struct RepoBatchTarget {
    std::string repoId{};
    std::string repoName{};
    std::filesystem::path repoPath{};

    auto operator<=>(const RepoBatchTarget&) const = default;
};

struct RepoBatchItemResult {
    std::string repoId{};
    std::string repoName{};
    std::filesystem::path repoPath{};
    RepoBatchItemStatus status{RepoBatchItemStatus::Pending};
    std::string message{};
    std::chrono::milliseconds durationMs{0};

    [[nodiscard]] bool isSuccess() const noexcept {
        return status == RepoBatchItemStatus::Succeeded;
    }

    [[nodiscard]] bool isFailed() const noexcept {
        return status == RepoBatchItemStatus::Failed;
    }

    auto operator<=>(const RepoBatchItemResult&) const = default;
};

struct BatchProgressUpdate {
    std::string operationId{};
    std::size_t completedCount{0};
    std::size_t totalCount{0};
    std::string currentRepoName{};
    std::string currentMessage{};

    [[nodiscard]] int percentage() const noexcept {
        if (totalCount == 0) return 100;
        return static_cast<int>((completedCount * 100) / totalCount);
    }
};

struct BatchOperationReport {
    std::string operationId{};
    BatchOperationType operationType{BatchOperationType::Fetch};
    std::size_t totalCount{0};
    std::size_t succeededCount{0};
    std::size_t failedCount{0};
    std::size_t skippedCount{0};
    std::vector<RepoBatchItemResult> items{};
    std::chrono::milliseconds totalDurationMs{0};

    [[nodiscard]] bool isAllSucceeded() const noexcept {
        return totalCount > 0 && succeededCount == totalCount;
    }

    [[nodiscard]] bool hasFailures() const noexcept {
        return failedCount > 0;
    }

    auto operator<=>(const BatchOperationReport&) const = default;
};

} // namespace gf::git::bo
