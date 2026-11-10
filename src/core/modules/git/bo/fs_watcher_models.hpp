#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::git::bo
{
    enum class FsChangeType
    {
        Created,
        Modified,
        Deleted,
        Unknown
    };

    enum class FsChangeClassification
    {
        Ignored, // .git/objects, .git/logs, editor swap files, etc.
        GitHeadChanged, // .git/HEAD -> branch switch or commit created
        GitRefsChanged, // .git/refs -> new/updated branch or tag
        GitIndexChanged, // .git/index -> staged/unstaged changes from external tool
        GitOperationState, // .git/MERGE_HEAD, .git/rebase-merge, .git/rebase-apply, etc.
        WorkingTreeModified // User file modified in working tree
    };

    struct FsRawEvent
    {
        std::filesystem::path repositoryPath{};
        std::filesystem::path changedPath{};
        FsChangeType changeType{FsChangeType::Modified};

        auto operator<=>(const FsRawEvent&) const = default;
    };

    struct FsDebouncedBatch
    {
        std::filesystem::path repositoryPath{};
        bool headChanged{false};
        bool refsChanged{false};
        bool indexChanged{false};
        bool operationStateChanged{false};
        bool workingTreeChanged{false};
        std::vector<std::filesystem::path> affectedPaths{};

        [[nodiscard]] bool hasAnyChange() const noexcept
        {
            return headChanged || refsChanged || indexChanged || operationStateChanged || workingTreeChanged;
        }

        [[nodiscard]] bool requiresWorkingTreeRefresh() const noexcept
        {
            return workingTreeChanged || indexChanged || headChanged || operationStateChanged;
        }

        [[nodiscard]] bool requiresBranchRefresh() const noexcept
        {
            return headChanged || refsChanged || operationStateChanged;
        }

        auto operator<=>(const FsDebouncedBatch&) const = default;
    };
} // namespace gf::git::bo
