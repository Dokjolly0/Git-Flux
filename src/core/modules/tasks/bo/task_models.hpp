#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace gf::tasks::bo
{
    enum class TaskState
    {
        Pending,
        Running,
        Completed,
        Failed,
        Cancelled
    };

    enum class TaskPriority
    {
        Low,
        Normal,
        High
    };

    struct TaskProgress
    {
        std::optional<int> percentage{std::nullopt};
        std::string message{};
        int currentStep{0};
        int totalSteps{0};

        auto operator<=>(const TaskProgress&) const = default;
    };

    struct TaskReport
    {
        std::string taskId{};
        std::string taskName{};
        TaskState state{TaskState::Pending};
        std::string errorMessage{};
        std::chrono::milliseconds durationMs{0};

        [[nodiscard]] bool isSuccess() const noexcept
        {
            return state == TaskState::Completed;
        }

        [[nodiscard]] bool isCancelled() const noexcept
        {
            return state == TaskState::Cancelled;
        }

        [[nodiscard]] bool isFailed() const noexcept
        {
            return state == TaskState::Failed;
        }

        auto operator<=>(const TaskReport&) const = default;
    };
} // namespace gf::tasks::bo
