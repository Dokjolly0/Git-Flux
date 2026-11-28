#pragma once

#include "core/shared/string_utils.hpp"

#include <optional>
#include <string_view>

namespace gf::core::syntax::detail
{
    struct IndentInfo
    {
        std::size_t wsEnd = 0;
        std::size_t wsColumns = 0;
    };

    [[nodiscard]] constexpr IndentInfo scanLeadingIndent(const std::string_view line) noexcept
    {
        IndentInfo info;
        while (info.wsEnd < line.size() && (line[info.wsEnd] == ' ' || line[info.wsEnd] == '\t'))
        {
            info.wsColumns += line[info.wsEnd] == '\t' ? 4 : 1;
            ++info.wsEnd;
        }
        return info;
    }

    [[nodiscard]] constexpr std::optional<std::size_t> matchAtxHeadingHashes(const std::string_view trimmed) noexcept
    {
        if (trimmed.empty() || trimmed[0] != '#') return std::nullopt;
        std::size_t hashes = 0;
        while (hashes < trimmed.size() && hashes < 7 && trimmed[hashes] == '#')
        {
            ++hashes;
        }
        if (hashes == 0 || hashes > 6) return std::nullopt;
        if (hashes < trimmed.size() && trimmed[hashes] != ' ' && trimmed[hashes] != '\t')
        {
            return std::nullopt;
        }
        return hashes;
    }

    [[nodiscard]] constexpr bool isThematicBreakLine(const std::string_view s) noexcept
    {
        if (s.empty()) return false;
        char marker = '\0';
        int count = 0;
        for (const char c : s)
        {
            if (c == ' ' || c == '\t') continue;
            if (c != '-' && c != '*' && c != '_') return false;
            if (marker == '\0') marker = c;
            else if (c != marker) return false;
            ++count;
        }
        return count >= 3;
    }

    struct FenceMatch
    {
        std::size_t wsEnd = 0;
        char fenceChar = '`';
        std::size_t fenceLen = 0;
        std::string_view infoRaw;
    };

    [[nodiscard]] constexpr std::optional<FenceMatch> matchFenceOpen(const std::string_view line) noexcept
    {
        const auto [wsEnd, wsColumns] = scanLeadingIndent(line);
        if (wsColumns > 3) return std::nullopt;

        const std::string_view trimmed = line.substr(wsEnd);
        if (trimmed.size() < 3 || (trimmed[0] != '`' && trimmed[0] != '~')) return std::nullopt;
        const char fChar = trimmed[0];
        std::size_t fLen = 0;
        while (fLen < trimmed.size() && trimmed[fLen] == fChar) ++fLen;
        if (fLen < 3) return std::nullopt;

        const std::string_view infoRaw = trimmed.substr(fLen);
        if (fChar == '`' && infoRaw.find('`') != std::string_view::npos) return std::nullopt;

        return FenceMatch{
            .wsEnd = wsEnd,
            .fenceChar = fChar,
            .fenceLen = fLen,
            .infoRaw = infoRaw
        };
    }

    struct FenceCloseMatch
    {
        std::size_t wsEnd = 0;
        std::size_t fenceLen = 0;
    };

    [[nodiscard]] constexpr std::optional<FenceCloseMatch> matchFenceClose(const std::string_view line,
                                                                           const char fenceChar,
                                                                           const std::size_t minLen) noexcept
    {
        const auto [wsEnd, wsColumns] = scanLeadingIndent(line);
        if (wsColumns > 3) return std::nullopt;

        const std::string_view trimmed = line.substr(wsEnd);
        if (trimmed.empty() || trimmed[0] != fenceChar) return std::nullopt;
        std::size_t fenceEnd = 0;
        while (fenceEnd < trimmed.size() && trimmed[fenceEnd] == fenceChar) ++fenceEnd;
        if (fenceEnd < minLen) return std::nullopt;

        for (std::size_t k = fenceEnd; k < trimmed.size(); ++k)
        {
            if (trimmed[k] != ' ' && trimmed[k] != '\t' && trimmed[k] != '\r')
            {
                return std::nullopt;
            }
        }
        return FenceCloseMatch{.wsEnd = wsEnd, .fenceLen = fenceEnd};
    }

    struct InlineCodeSpanMatch
    {
        std::size_t tickLen = 0;
        std::size_t contentStart = 0;
        std::size_t contentEnd = 0;
        std::size_t closeEnd = 0;
        bool found = false;
    };

    [[nodiscard]] constexpr InlineCodeSpanMatch scanInlineCodeSpan(const std::string_view sv,
                                                                   const std::size_t tickStart) noexcept
    {
        const std::size_t n = sv.size();
        std::size_t i = tickStart;
        while (i < n && sv[i] == '`') ++i;
        const std::size_t tickLen = i - tickStart;

        std::size_t search = i;
        while (search < n)
        {
            if (sv[search] == '`')
            {
                std::size_t runEnd = search;
                while (runEnd < n && sv[runEnd] == '`') ++runEnd;
                if (runEnd - search == tickLen)
                {
                    return InlineCodeSpanMatch{
                        .tickLen = tickLen,
                        .contentStart = i,
                        .contentEnd = search,
                        .closeEnd = runEnd,
                        .found = true
                    };
                }
                search = runEnd;
            }
            else
            {
                ++search;
            }
        }
        return InlineCodeSpanMatch{
            .tickLen = tickLen,
            .contentStart = i,
            .contentEnd = i,
            .closeEnd = i,
            .found = false
        };
    }

    struct BracketLinkSpan
    {
        bool isImage = false;
        std::size_t bracketOpen = 0;
        std::size_t bracketClose = 0;
        std::size_t urlStart = 0;
        std::size_t parenClose = 0;
        std::string_view label;
        std::string_view rawUrl;
    };

    [[nodiscard]] constexpr std::optional<BracketLinkSpan> scanBracketLinkSpan(const std::string_view sv,
                                                                               const std::size_t startPos) noexcept
    {
        const std::size_t n = sv.size();
        if (startPos >= n) return std::nullopt;

        bool isImg = false;
        std::size_t openBracket = startPos;
        if (sv[startPos] == '!')
        {
            if (startPos + 1 >= n || sv[startPos + 1] != '[') return std::nullopt;
            isImg = true;
            openBracket = startPos + 1;
        }
        else if (sv[startPos] != '[')
        {
            return std::nullopt;
        }

        int depth = 1;
        std::size_t k = openBracket + 1;
        std::size_t closeBracket = std::string_view::npos;
        while (k < n)
        {
            if (sv[k] == '\\' && k + 1 < n)
            {
                k += 2;
                continue;
            }
            if (sv[k] == '`')
            {
                ++k;
                while (k < n && sv[k] != '`') ++k;
                if (k < n) ++k;
                continue;
            }
            if (sv[k] == '[') ++depth;
            else if (sv[k] == ']')
            {
                if (--depth == 0)
                {
                    closeBracket = k;
                    break;
                }
            }
            ++k;
        }

        if (closeBracket == std::string_view::npos || closeBracket + 1 >= n || sv[closeBracket + 1] != '(')
        {
            return std::nullopt;
        }

        const std::size_t urlStart = closeBracket + 2;
        int parenDepth = 1;
        std::size_t p = urlStart;
        std::size_t closeParen = std::string_view::npos;
        while (p < n)
        {
            if (sv[p] == '\\' && p + 1 < n)
            {
                p += 2;
                continue;
            }
            if (sv[p] == '(') ++parenDepth;
            else if (sv[p] == ')')
            {
                if (--parenDepth == 0)
                {
                    closeParen = p;
                    break;
                }
            }
            ++p;
        }
        if (closeParen == std::string_view::npos) return std::nullopt;

        return BracketLinkSpan{
            .isImage = isImg,
            .bracketOpen = openBracket,
            .bracketClose = closeBracket,
            .urlStart = urlStart,
            .parenClose = closeParen,
            .label = sv.substr(openBracket + 1, closeBracket - openBracket - 1),
            .rawUrl = sv.substr(urlStart, closeParen - urlStart)
        };
    }

    [[nodiscard]] constexpr std::size_t scanBareHttpUrlEnd(const std::string_view sv,
                                                             const std::size_t startPos,
                                                             const bool stopAtRightBracket) noexcept
    {
        std::size_t i = startPos;
        int parenDepth = 0;
        while (i < sv.size() && !isAsciiWhitespace(sv[i]) && sv[i] != '"' && sv[i] != '<' && sv[i] != '>')
        {
            if (sv[i] == '(') ++parenDepth;
            else if (sv[i] == ')')
            {
                if (parenDepth == 0) break;
                --parenDepth;
            }
            else if (stopAtRightBracket && sv[i] == ']')
            {
                break;
            }
            ++i;
        }
        while (i > startPos && (sv[i - 1] == '.' || sv[i - 1] == ',' || sv[i - 1] == ';'))
        {
            --i;
        }
        return i;
    }

    [[nodiscard]] constexpr std::size_t findClosingItalicAsterisk(const std::string_view sv,
                                                                  const std::size_t openAsteriskPos) noexcept
    {
        std::size_t search = openAsteriskPos + 1;
        while (search < sv.size())
        {
            if (sv[search] == '\\' && search + 1 < sv.size())
            {
                search += 2;
                continue;
            }
            if (sv[search] == '*')
            {
                if (search + 1 < sv.size() && sv[search + 1] == '*')
                {
                    search += 2;
                    continue;
                }
                if (!isAsciiWhitespace(sv[search - 1]))
                {
                    return search;
                }
                return std::string_view::npos;
            }
            ++search;
        }
        return std::string_view::npos;
    }
} // namespace gf::core::syntax::detail
