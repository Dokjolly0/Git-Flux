#pragma once

#include <string>
#include <vector>

namespace gf::git::bo
{
    struct GitBranchHead
    {
        std::string oid;
        std::string headName;
        std::string upstream;
        int ahead = 0;
        int behind = 0;
        bool detached = false;
        bool unborn = false;

        auto operator<=>(const GitBranchHead&) const = default;
    };

    struct GitFileStatusEntry
    {
        char stagedStatus = '.';
        char unstagedStatus = '.';
        std::string path;
        std::string originalPath; // Populated for rename/copy
        bool isConflict = false;
        bool isUntracked = false;

        [[nodiscard]] bool hasStagedChanges() const noexcept
        {
            return !isUntracked && !isConflict && stagedStatus != '.';
        }

        [[nodiscard]] bool hasUnstagedChanges() const noexcept
        {
            return !isUntracked && !isConflict && unstagedStatus != '.';
        }
    };

    struct GitRepositoryStatus
    {
        GitBranchHead branch;
        std::vector<GitFileStatusEntry> entries;

        [[nodiscard]] bool isClean() const noexcept { return entries.empty(); }
    };
} // namespace gf::git::bo
