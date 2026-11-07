#include "core/modules/git/bl/unified_diff_parser.hpp"

#include <charconv>
#include <string_view>

namespace gf::git::bl {

namespace {

[[nodiscard]] std::string_view trimRight(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.back() == '\r' || sv.back() == '\n')) {
        sv.remove_suffix(1);
    }
    return sv;
}

[[nodiscard]] std::string stripPrefix(std::string_view path) {
    if (path.starts_with("a/") || path.starts_with("b/")) {
        path.remove_prefix(2);
    }
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') {
        path.remove_prefix(1);
        path.remove_suffix(1);
    }
    return std::string(path);
}

void parseDiffGitHeader(std::string_view line, bo::FileDiff& file) {
    // line: "diff --git a/path b/path"
    constexpr std::string_view prefix = "diff --git ";
    if (!line.starts_with(prefix)) {
        return;
    }
    std::string_view rest = line.substr(prefix.size());
    // Handle quoted paths or spaces
    if (rest.starts_with("\"")) {
        auto closeQuote = rest.find('"', 1);
        if (closeQuote != std::string_view::npos) {
            file.oldPath = stripPrefix(rest.substr(0, closeQuote + 1));
            auto secondStart = rest.find('"', closeQuote + 1);
            if (secondStart != std::string_view::npos) {
                auto secondClose = rest.find('"', secondStart + 1);
                if (secondClose != std::string_view::npos) {
                    file.newPath = stripPrefix(rest.substr(secondStart, secondClose - secondStart + 1));
                    return;
                }
            }
        }
    }

    auto spacePos = rest.find(" b/");
    if (spacePos != std::string_view::npos) {
        file.oldPath = stripPrefix(rest.substr(0, spacePos));
        file.newPath = stripPrefix(rest.substr(spacePos + 1));
    }
}

bool parseHunkHeader(std::string_view line, bo::DiffHunk& hunk) {
    // Format: @@ -oldStart[,oldCount] +newStart[,newCount] @@ [heading]
    if (!line.starts_with("@@ -")) {
        return false;
    }

    hunk.header = std::string(line);

    std::size_t idx = 4; // after "@@ -"
    int oldStart = 0;
    auto [p1, ec1] = std::from_chars(line.data() + idx, line.data() + line.size(), oldStart);
    if (ec1 != std::errc{}) {
        return false;
    }
    idx = p1 - line.data();

    int oldCount = 1;
    if (idx < line.size() && line[idx] == ',') {
        ++idx;
        auto [p2, ec2] = std::from_chars(line.data() + idx, line.data() + line.size(), oldCount);
        if (ec2 != std::errc{}) {
            return false;
        }
        idx = p2 - line.data();
    }

    // Skip spaces before '+'
    while (idx < line.size() && line[idx] == ' ') {
        ++idx;
    }
    if (idx >= line.size() || line[idx] != '+') {
        return false;
    }
    ++idx; // after '+'

    int newStart = 0;
    auto [p3, ec3] = std::from_chars(line.data() + idx, line.data() + line.size(), newStart);
    if (ec3 != std::errc{}) {
        return false;
    }
    idx = p3 - line.data();

    int newCount = 1;
    if (idx < line.size() && line[idx] == ',') {
        ++idx;
        auto [p4, ec4] = std::from_chars(line.data() + idx, line.data() + line.size(), newCount);
        if (ec4 != std::errc{}) {
            return false;
        }
    }

    hunk.oldStartLine = oldStart;
    hunk.oldCount = oldCount;
    hunk.newStartLine = newStart;
    hunk.newCount = newCount;
    return true;
}

} // namespace

std::vector<bo::FileDiff> UnifiedDiffParser::parse(std::string_view diffOutput) {
    std::vector<bo::FileDiff> result;
    if (diffOutput.empty()) {
        return result;
    }

    bo::FileDiff currentFile;
    std::optional<bo::DiffHunk> currentHunk;
    int currentOldLine = 0;
    int currentNewLine = 0;
    bool inHunk = false;
    bool hasActiveFile = false;

    auto flushHunk = [&]() {
        if (currentHunk) {
            currentFile.hunks.push_back(std::move(*currentHunk));
            currentHunk.reset();
        }
        inHunk = false;
    };

    auto flushFile = [&]() {
        flushHunk();
        if (hasActiveFile) {
            result.push_back(std::move(currentFile));
            currentFile = bo::FileDiff{};
            hasActiveFile = false;
        }
    };

    std::size_t startPos = 0;
    while (startPos < diffOutput.size()) {
        auto endPos = diffOutput.find('\n', startPos);
        std::string_view rawLine = (endPos == std::string_view::npos)
                                       ? diffOutput.substr(startPos)
                                       : diffOutput.substr(startPos, endPos - startPos);
        startPos = (endPos == std::string_view::npos) ? diffOutput.size() : endPos + 1;

        std::string_view line = trimRight(rawLine);

        // Start of a new diff file
        if (line.starts_with("diff --git ")) {
            flushFile();
            hasActiveFile = true;
            parseDiffGitHeader(line, currentFile);
            continue;
        }

        if (line.starts_with("new file mode ")) {
            currentFile.isNewFile = true;
            continue;
        }
        if (line.starts_with("deleted file mode ")) {
            currentFile.isDeletedFile = true;
            continue;
        }
        if (line.starts_with("similarity index ")) {
            currentFile.isRenamed = true;
            continue;
        }
        if (line.starts_with("rename from ")) {
            currentFile.oldPath = std::string(line.substr(12));
            currentFile.isRenamed = true;
            continue;
        }
        if (line.starts_with("rename to ")) {
            currentFile.newPath = std::string(line.substr(10));
            currentFile.isRenamed = true;
            continue;
        }
        if (line.starts_with("Binary files ") && line.ends_with(" differ")) {
            currentFile.isBinary = true;
            continue;
        }

        // Standard diff file path header
        if (line.starts_with("--- ")) {
            std::string_view pathPart = line.substr(4);
            if (pathPart != "/dev/null") {
                currentFile.oldPath = stripPrefix(pathPart);
            }
            continue;
        }
        if (line.starts_with("+++ ")) {
            std::string_view pathPart = line.substr(4);
            if (pathPart != "/dev/null") {
                currentFile.newPath = stripPrefix(pathPart);
            }
            continue;
        }

        // Start of a new hunk
        if (line.starts_with("@@ -")) {
            flushHunk();
            bo::DiffHunk hunk;
            if (parseHunkHeader(line, hunk)) {
                currentOldLine = hunk.oldStartLine;
                currentNewLine = hunk.newStartLine;
                currentHunk = std::move(hunk);
                inHunk = true;
                if (!hasActiveFile) {
                    hasActiveFile = true;
                }
            }
            continue;
        }

        // Process line content if inside an active hunk
        if (inHunk && currentHunk) {
            if (line.starts_with('+')) {
                currentHunk->lines.push_back(bo::DiffLine{
                    .kind = bo::DiffLineKind::Addition,
                    .oldLineNumber = std::nullopt,
                    .newLineNumber = currentNewLine++,
                    .content = std::string(line.substr(1))
                });
                ++currentFile.additionsCount;
            } else if (line.starts_with('-')) {
                currentHunk->lines.push_back(bo::DiffLine{
                    .kind = bo::DiffLineKind::Deletion,
                    .oldLineNumber = currentOldLine++,
                    .newLineNumber = std::nullopt,
                    .content = std::string(line.substr(1))
                });
                ++currentFile.deletionsCount;
            } else if (line.starts_with(' ')) {
                currentHunk->lines.push_back(bo::DiffLine{
                    .kind = bo::DiffLineKind::Context,
                    .oldLineNumber = currentOldLine++,
                    .newLineNumber = currentNewLine++,
                    .content = std::string(line.substr(1))
                });
            } else if (line.starts_with('\\')) {
                currentHunk->lines.push_back(bo::DiffLine{
                    .kind = bo::DiffLineKind::NoNewline,
                    .oldLineNumber = std::nullopt,
                    .newLineNumber = std::nullopt,
                    .content = std::string(line)
                });
            } else if (line.empty()) {
                // Empty line inside a hunk treated as context
                currentHunk->lines.push_back(bo::DiffLine{
                    .kind = bo::DiffLineKind::Context,
                    .oldLineNumber = currentOldLine++,
                    .newLineNumber = currentNewLine++,
                    .content = {}
                });
            }
        }
    }

    flushFile();
    return result;
}

std::optional<bo::FileDiff> UnifiedDiffParser::parseSingle(std::string_view diffOutput) {
    auto all = parse(diffOutput);
    if (all.empty()) {
        return std::nullopt;
    }
    return std::move(all.front());
}

} // namespace gf::git::bl
