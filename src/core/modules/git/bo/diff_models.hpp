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
};

struct DiffHunk {
    int oldStartLine = 0;
    int oldCount = 0;
    int newStartLine = 0;
    int newCount = 0;
    std::string header;
    std::vector<DiffLine> lines;
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

    [[nodiscard]] bool empty() const noexcept {
        return !isBinary && hunks.empty() && !isNewFile && !isDeletedFile && !isRenamed;
    }

    [[nodiscard]] const std::string& displayPath() const noexcept {
        return newPath.empty() ? oldPath : newPath;
    }
};

} // namespace gf::git::bo
