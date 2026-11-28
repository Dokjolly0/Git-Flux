#include "core/modules/git/bl/conflict_marker_parser.hpp"
#include "core/shared/string_utils.hpp"

namespace gf::git::bl
{
    namespace
    {
        enum class ParseState
        {
            Normal,
            InOurs,
            InBase,
            InTheirs
        };

        template <typename OnMarkerLine, typename OnContentLine>
        std::expected<void, std::string> scanConflictLines(const std::string_view content,
                                                           OnMarkerLine&& onHunkStart,
                                                           OnContentLine&& onContentLine)
        {
            auto state = ParseState::Normal;
            std::size_t currentLineNum = 1;
            std::size_t startPos = 0;

            while (startPos < content.size())
            {
                const auto endPos = content.find('\n', startPos);
                const std::string_view fullLine = endPos == std::string_view::npos
                                                      ? content.substr(startPos)
                                                      : content.substr(startPos, endPos - startPos + 1);
                const std::string_view trimmedLine = core::trimTrailingNewlines(fullLine);

                if (trimmedLine.starts_with("<<<<<<<"))
                {
                    state = ParseState::InOurs;
                    if (auto res = onHunkStart(currentLineNum); !res)
                    {
                        return res;
                    }
                }
                else if (state == ParseState::InOurs && trimmedLine.starts_with("|||||||"))
                {
                    state = ParseState::InBase;
                }
                else if ((state == ParseState::InOurs || state == ParseState::InBase) &&
                    trimmedLine.starts_with("======="))
                {
                    state = ParseState::InTheirs;
                }
                else if (state == ParseState::InTheirs && trimmedLine.starts_with(">>>>>>>"))
                {
                    state = ParseState::Normal;
                    onContentLine(state, fullLine, true);
                }
                else
                {
                    onContentLine(state, fullLine, false);
                }

                if (endPos == std::string_view::npos)
                {
                    break;
                }
                startPos = endPos + 1;
                ++currentLineNum;
            }
            return {};
        }
    } // namespace

    bool ConflictMarkerParser::hasConflictMarkers(const std::string_view content) noexcept
    {
        return content.find("<<<<<<<") != std::string_view::npos &&
            content.find("=======") != std::string_view::npos &&
            content.find(">>>>>>>") != std::string_view::npos;
    }

    std::vector<bo::ConflictHunk>
    ConflictMarkerParser::parseHunks(const std::string_view content)
    {
        std::vector<bo::ConflictHunk> hunks;
        if (content.empty())
        {
            return hunks;
        }

        bo::ConflictHunk currentHunk;
        (void)scanConflictLines(
            content,
            [&](const std::size_t lineNum) -> std::expected<void, std::string>
            {
                currentHunk = bo::ConflictHunk{};
                currentHunk.hunkIndex = hunks.size();
                currentHunk.startLine = lineNum;
                return {};
            },
            [&](const ParseState state, const std::string_view fullLine, const bool closedHunk)
            {
                if (closedHunk)
                {
                    hunks.push_back(std::move(currentHunk));
                    currentHunk = bo::ConflictHunk{};
                    return;
                }
                switch (state)
                {
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
            });

        return hunks;
    }

    std::expected<std::string, std::string>
    ConflictMarkerParser::reconstruct(const std::string_view originalContent,
                                      const std::vector<bo::ConflictHunk>& hunks)
    {
        std::string result;
        result.reserve(originalContent.size());
        std::size_t currentHunkIndex = 0;

        auto res = scanConflictLines(
            originalContent,
            [&](std::size_t) -> std::expected<void, std::string>
            {
                if (currentHunkIndex >= hunks.size())
                {
                    return std::unexpected("Trovato blocco conflitto inatteso senza hunk associato");
                }
                const auto& hunk = hunks[currentHunkIndex];
                if (!hunk.isResolved)
                {
                    return std::unexpected(
                        "Il conflitto #" + std::to_string(currentHunkIndex) + " non è ancora stato risolto");
                }
                result.append(hunk.resolvedContent());
                ++currentHunkIndex;
                return {};
            },
            [&](const ParseState state, const std::string_view fullLine, const bool closedHunk)
            {
                if (!closedHunk && state == ParseState::Normal)
                {
                    result.append(fullLine);
                }
            });

        if (!res)
        {
            return std::unexpected(res.error());
        }
        return result;
    }
} // namespace gf::git::bl
