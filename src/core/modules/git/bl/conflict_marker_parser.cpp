#include "core/modules/git/bl/conflict_marker_parser.hpp"

namespace gf::git::bl {

namespace {

enum class ParseState {
    Normal,
    InOurs,
    InBase,
    InTheirs
};

} // namespace

bool ConflictMarkerParser::hasConflictMarkers(std::string_view content) noexcept {
    return content.find("<<<<<<<") != std::string_view::npos &&
           content.find("=======") != std::string_view::npos &&
           content.find(">>>>>>>") != std::string_view::npos;
}

std::vector<bo::ConflictHunk>
ConflictMarkerParser::parseHunks(std::string_view content) {
    std::vector<bo::ConflictHunk> hunks;
    if (content.empty()) {
        return hunks;
    }

    ParseState state = ParseState::Normal;
    bo::ConflictHunk currentHunk;
    std::size_t currentLineNum = 1;

    std::size_t startPos = 0;
    while (startPos < content.size()) {
        auto endPos = content.find('\n', startPos);
        std::string_view fullLine = (endPos == std::string_view::npos)
                                        ? content.substr(startPos)
                                        : content.substr(startPos, endPos - startPos + 1);

        std::string_view trimmedLine = fullLine;
        while (!trimmedLine.empty() && (trimmedLine.back() == '\n' || trimmedLine.back() == '\r')) {
            trimmedLine.remove_suffix(1);
        }

        if (trimmedLine.starts_with("<<<<<<<")) {
            state = ParseState::InOurs;
            currentHunk = bo::ConflictHunk{};
            currentHunk.hunkIndex = hunks.size();
            currentHunk.startLine = currentLineNum;
        } else if (state == ParseState::InOurs && trimmedLine.starts_with("|||||||")) {
            state = ParseState::InBase;
        } else if ((state == ParseState::InOurs || state == ParseState::InBase) && trimmedLine.starts_with("=======")) {
            state = ParseState::InTheirs;
        } else if (state == ParseState::InTheirs && trimmedLine.starts_with(">>>>>>>")) {
            state = ParseState::Normal;
            hunks.push_back(std::move(currentHunk));
        } else {
            switch (state) {
            case ParseState::InOurs:
                currentHunk.oursContent.append(fullLine);
                break;
            case ParseState::InBase:
                currentHunk.baseContent.append(fullLine);
                break;
            case ParseState::InTheirs:
                currentHunk.theirsContent.append(fullLine);
                break;
            case ParseState::Normal:
                break;
            }
        }

        if (endPos == std::string_view::npos) {
            break;
        }
        startPos = endPos + 1;
        ++currentLineNum;
    }

    return hunks;
}

std::expected<std::string, std::string>
ConflictMarkerParser::reconstruct(std::string_view originalContent,
                                  const std::vector<bo::ConflictHunk>& hunks) {
    std::string result;
    result.reserve(originalContent.size());

    ParseState state = ParseState::Normal;
    std::size_t currentHunkIndex = 0;

    std::size_t startPos = 0;
    while (startPos < originalContent.size()) {
        auto endPos = originalContent.find('\n', startPos);
        std::string_view fullLine = (endPos == std::string_view::npos)
                                        ? originalContent.substr(startPos)
                                        : originalContent.substr(startPos, endPos - startPos + 1);

        std::string_view trimmedLine = fullLine;
        while (!trimmedLine.empty() && (trimmedLine.back() == '\n' || trimmedLine.back() == '\r')) {
            trimmedLine.remove_suffix(1);
        }

        if (trimmedLine.starts_with("<<<<<<<")) {
            state = ParseState::InOurs;
            if (currentHunkIndex >= hunks.size()) {
                return std::unexpected("Trovato blocco conflitto inatteso senza hunk associato");
            }
            const auto& hunk = hunks[currentHunkIndex];
            if (!hunk.isResolved) {
                return std::unexpected("Il conflitto #" + std::to_string(currentHunkIndex) + " non è ancora stato risolto");
            }
            result.append(hunk.resolvedContent());
            ++currentHunkIndex;
        } else if (state == ParseState::InOurs && trimmedLine.starts_with("|||||||")) {
            state = ParseState::InBase;
        } else if ((state == ParseState::InOurs || state == ParseState::InBase) && trimmedLine.starts_with("=======")) {
            state = ParseState::InTheirs;
        } else if (state == ParseState::InTheirs && trimmedLine.starts_with(">>>>>>>")) {
            state = ParseState::Normal;
        } else if (state == ParseState::Normal) {
            result.append(fullLine);
        }

        if (endPos == std::string_view::npos) {
            break;
        }
        startPos = endPos + 1;
    }

    return result;
}

} // namespace gf::git::bl
