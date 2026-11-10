#pragma once

#include <optional>
#include <string>
#include <vector>

namespace gf::git::bo {

enum class DiffScope {
    Staged,
    Unstaged,
    Untracked
};

enum class DiffLineKind {
    Context,
    Addition,
    Deletion,
    Header,
    NoNewline
};

struct DiffLine {
    DiffLineKind kind = DiffLineKind::Context;
    std::optional<int> oldLineNumber = std::nullopt;
    std::optional<int> newLineNumber = std::nullopt;
    std::string content;

    bool operator==(const DiffLine&) const = default;
};

struct DiffHunk {
    int oldStartLine = 0;
    int oldCount = 0;
    int newStartLine = 0;
    int newCount = 0;
    std::string header;
    std::vector<DiffLine> lines;

    bool operator==(const DiffHunk&) const = default;
};

struct FileDiff {
    std::string oldPath{};
    std::string newPath{};
    bool isBinary = false;
    bool isNewFile = false;
    bool isDeletedFile = false;
    bool isRenamed = false;
    int additionsCount = 0;
    int deletionsCount = 0;
    std::vector<DiffHunk> hunks{};

    bool operator==(const FileDiff&) const = default;

    [[nodiscard]] bool empty() const noexcept {
        return !isBinary && hunks.empty() && !isNewFile && !isDeletedFile && !isRenamed;
    }

    [[nodiscard]] const std::string& displayPath() const noexcept {
        return newPath.empty() ? oldPath : newPath;
    }
};

enum class FileChangeStatus {
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Unknown
};

struct CommitFileChange {
    std::string path{};
    std::string oldPath{};
    FileChangeStatus status{FileChangeStatus::Modified};
    int additions = 0;
    int deletions = 0;
    bool isBinary = false;
};

struct BlameLine {
    std::string commitSha{};
    std::string shortSha{};
    std::string author{};
    std::string authorEmail{};
    std::string date{};
    int lineNumber = 0;
    std::string content{};
};

} // namespace gf::git::bo
