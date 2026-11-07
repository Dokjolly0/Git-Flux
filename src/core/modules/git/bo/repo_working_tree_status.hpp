#pragma once

#include "core/modules/git/bo/git_status.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace gf::git::bo {

enum class FileChangeKind {
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Conflicted
};

[[nodiscard]] constexpr std::string_view fileChangeKindToString(FileChangeKind kind) noexcept {
    switch (kind) {
    case FileChangeKind::Modified:   return "modified";
    case FileChangeKind::Added:      return "added";
    case FileChangeKind::Deleted:    return "deleted";
    case FileChangeKind::Renamed:    return "renamed";
    case FileChangeKind::Copied:     return "copied";
    case FileChangeKind::Untracked:  return "untracked";
    case FileChangeKind::Conflicted: return "conflicted";
    }
    return "unknown";
}

struct WorkingTreeFileItem {
    std::string path;
    std::string originalPath; // Populated for rename and copy
    FileChangeKind kind = FileChangeKind::Modified;
    char statusCode = 'M';

    auto operator<=>(const WorkingTreeFileItem&) const = default;
};

struct RepoWorkingTreeStatus {
    GitBranchHead branch;
    std::vector<WorkingTreeFileItem> stagedFiles;
    std::vector<WorkingTreeFileItem> unstagedFiles;
    std::vector<WorkingTreeFileItem> untrackedFiles;
    std::vector<WorkingTreeFileItem> conflictedFiles;

    [[nodiscard]] bool isClean() const noexcept {
        return stagedFiles.empty() && unstagedFiles.empty() &&
               untrackedFiles.empty() && conflictedFiles.empty();
    }

    [[nodiscard]] std::size_t totalChangesCount() const noexcept {
        return stagedFiles.size() + unstagedFiles.size() +
               untrackedFiles.size() + conflictedFiles.size();
    }

    [[nodiscard]] std::size_t stagedCount() const noexcept {
        return stagedFiles.size();
    }

    [[nodiscard]] std::size_t unstagedCount() const noexcept {
        return unstagedFiles.size();
    }

    [[nodiscard]] std::size_t untrackedCount() const noexcept {
        return untrackedFiles.size();
    }

    [[nodiscard]] std::size_t conflictedCount() const noexcept {
        return conflictedFiles.size();
    }

    auto operator<=>(const RepoWorkingTreeStatus&) const = default;
};

} // namespace gf::git::bo
