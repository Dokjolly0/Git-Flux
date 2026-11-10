#include "core/modules/syntax/markdown_parser.hpp"
#include "core/modules/syntax/syntax_highlighter.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

namespace gf::core::syntax
{
    namespace
    {
        [[nodiscard]] constexpr bool mdIsSpace(char c) noexcept
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }

        [[nodiscard]] constexpr bool mdIsDigit(char c) noexcept
        {
            return c >= '0' && c <= '9';
        }

        [[nodiscard]] constexpr char mdToLowerAscii(char c) noexcept
        {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        }

        [[nodiscard]] constexpr std::size_t utf8SeqLen(unsigned char b) noexcept
        {
            if (b < 0x80) return 1;
            if ((b & 0xE0) == 0xC0) return 2;
            if ((b & 0xF0) == 0xE0) return 3;
            if ((b & 0xF8) == 0xF0) return 4;
            return 1;
        }

        [[nodiscard]] std::string_view trimSpaces(std::string_view sv) noexcept
        {
            while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r'))
            {
                sv.remove_prefix(1);
            }
            while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r'))
            {
                sv.remove_suffix(1);
            }
            return sv;
        }

        [[nodiscard]] std::string toLowerCopy(std::string_view sv)
        {
            std::string out;
            out.reserve(sv.size());
            for (char c : sv)
            {
                out.push_back(mdToLowerAscii(c));
            }
            return out;
        }

        [[nodiscard]] std::string normalizeLineEndings(std::string_view input)
        {
            std::string out;
            out.reserve(input.size());
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                if (input[i] == '\r')
                {
                    if (i + 1 < input.size() && input[i + 1] == '\n')
                    {
                        ++i;
                    }
                    out.push_back('\n');
                }
                else
                {
                    out.push_back(input[i]);
                }
            }
            return out;
        }

        struct LineSlice
        {
            std::string_view text;
            int lineNumber = 1; // 1-based
        };

        [[nodiscard]] std::vector<LineSlice> splitNormalizedLines(std::string_view text)
        {
            std::vector<LineSlice> lines;
            if (text.empty())
            {
                return lines;
            }
            std::size_t pos = 0;
            int lineNum = 1;
            while (pos <= text.size())
            {
                if (pos == text.size())
                {
                    break;
                }
                auto nl = text.find('\n', pos);
                if (nl == std::string_view::npos)
                {
                    lines.push_back({text.substr(pos), lineNum});
                    break;
                }
                lines.push_back({text.substr(pos, nl - pos), lineNum});
                pos = nl + 1;
                ++lineNum;
            }
            return lines;
        }

        [[nodiscard]] std::string escapeInlineText(std::string_view text)
        {
            std::string out;
            out.reserve(text.size() + 8);
            for (char c : text)
            {
                switch (c)
                {
                case '&': out.append("&amp;");
                    break;
                case '<': out.append("&lt;");
                    break;
                case '>': out.append("&gt;");
                    break;
                case '"': out.append("&quot;");
                    break;
                default: out.push_back(c);
                    break;
                }
            }
            return out;
        }

        [[nodiscard]] std::string escapeHrefAttr(std::string_view url)
        {
            std::string out;
            out.reserve(url.size() + 8);
            for (char c : url)
            {
                if (c == '"') out.append("%22");
                else if (c == '&') out.append("&amp;");
                else if (c == '<') out.append("%3C");
                else if (c == '>') out.append("%3E");
                else out.push_back(c);
            }
            return out;
        }

        [[nodiscard]] bool parseBracketLink(std::string_view sv,
                                            std::size_t startPos,
                                            bool& outIsImage,
                                            std::string_view& outLabel,
                                            std::string_view& outUrl,
                                            std::size_t& outEndPos) noexcept
        {
            const std::size_t n = sv.size();
            if (startPos >= n) return false;

            bool isImg = false;
            std::size_t openBracket = startPos;
            if (sv[startPos] == '!')
            {
                if (startPos + 1 >= n || sv[startPos + 1] != '[') return false;
                isImg = true;
                openBracket = startPos + 1;
            }
            else if (sv[startPos] != '[')
            {
                return false;
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
                if (sv[k] == '[')
                {
                    ++depth;
                }
                else if (sv[k] == ']')
                {
                    --depth;
                    if (depth == 0)
                    {
                        closeBracket = k;
                        break;
                    }
                }
                ++k;
            }

            if (closeBracket == std::string_view::npos || closeBracket + 1 >= n || sv[closeBracket + 1] != '(')
            {
                return false;
            }

            std::size_t urlStart = closeBracket + 2;
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
                if (sv[p] == '(')
                {
                    ++parenDepth;
                }
                else if (sv[p] == ')')
                {
                    --parenDepth;
                    if (parenDepth == 0)
                    {
                        closeParen = p;
                        break;
                    }
                }
                ++p;
            }
            if (closeParen == std::string_view::npos)
            {
                return false;
            }

            std::string_view urlPart = trimSpaces(sv.substr(urlStart, closeParen - urlStart));
            // Strip optional link title `url "title"`
            if (!urlPart.empty() && urlPart.front() == '<')
            {
                auto gt = urlPart.find('>');
                if (gt != std::string_view::npos)
                {
                    urlPart = urlPart.substr(1, gt - 1);
                }
            }
            else
            {
                auto sp = urlPart.find(' ');
                if (sp != std::string_view::npos)
                {
                    urlPart = trimSpaces(urlPart.substr(0, sp));
                }
            }

            outIsImage = isImg;
            outLabel = sv.substr(openBracket + 1, closeBracket - openBracket - 1);
            outUrl = urlPart;
            outEndPos = closeParen + 1;
            return true;
        }

        [[nodiscard]] std::string badgeColorFromImageUrl(std::string_view imgUrl)
        {
            std::string lower = toLowerCopy(imgUrl);
            if (lower.find("41cd52") != std::string::npos || lower.find("brightgreen") != std::string::npos ||
                lower.find("-green") != std::string::npos)
            {
                return "#238636";
            }
            if (lower.find("-blue") != std::string::npos || lower.find("0969da") != std::string::npos)
            {
                return "#1F6FEB";
            }
            if (lower.find("-yellow") != std::string::npos || lower.find("-orange") != std::string::npos)
            {
                return "#9E6A03";
            }
            if (lower.find("-red") != std::string::npos || lower.find("critical") != std::string::npos)
            {
                return "#DA3633";
            }
            if (lower.find("-purple") != std::string::npos || lower.find("-blueviolet") != std::string::npos)
            {
                return "#8957E5";
            }
            if (lower.find("lightgrey") != std::string::npos || lower.find("grey") != std::string::npos ||
                lower.find("gray") != std::string::npos)
            {
                return "#30363D";
            }
            return "#1F6FEB";
        }

        [[nodiscard]] bool tryParseBadgeLine(std::string_view line, std::vector<MarkdownBadge>& outBadges)
        {
            std::string_view s = trimSpaces(line);
            if (s.empty()) return false;

            std::vector<MarkdownBadge> parsed;
            std::size_t i = 0;
            while (i < s.size())
            {
                while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
                if (i >= s.size()) break;

                bool isImg = false;
                std::string_view label;
                std::string_view url;
                std::size_t endPos = i;
                if (!parseBracketLink(s, i, isImg, label, url, endPos))
                {
                    return false;
                }

                if (isImg)
                {
                    std::string cleanLabel = std::string(trimSpaces(label));
                    if (cleanLabel.empty()) cleanLabel = "badge";
                    parsed.push_back(MarkdownBadge{
                        .label = std::move(cleanLabel),
                        .href = "",
                        .color = badgeColorFromImageUrl(url)
                    });
                    i = endPos;
                    continue;
                }

                // Check if link label itself is ![alt](img)
                std::string_view trimmedLabel = trimSpaces(label);
                bool innerIsImg = false;
                std::string_view innerAlt;
                std::string_view innerImgUrl;
                std::size_t innerEnd = 0;
                if (parseBracketLink(trimmedLabel, 0, innerIsImg, innerAlt, innerImgUrl, innerEnd) &&
                    innerIsImg && innerEnd == trimmedLabel.size())
                {
                    std::string cleanLabel = std::string(trimSpaces(innerAlt));
                    if (cleanLabel.empty()) cleanLabel = std::string(url);
                    parsed.push_back(MarkdownBadge{
                        .label = std::move(cleanLabel),
                        .href = std::string(url),
                        .color = badgeColorFromImageUrl(innerImgUrl)
                    });
                    i = endPos;
                    continue;
                }

                return false;
            }

            if (parsed.empty()) return false;
            outBadges.insert(outBadges.end(), parsed.begin(), parsed.end());
            return true;
        }

        [[nodiscard]] std::string stripMarkupToPlainText(std::string_view text)
        {
            std::string out;
            out.reserve(text.size());
            std::size_t i = 0;
            const std::size_t n = text.size();

            while (i < n)
            {
                const char c = text[i];
                if (c == '\\' && i + 1 < n)
                {
                    out.push_back(text[i + 1]);
                    i += 2;
                    continue;
                }
                if (c == '<')
                {
                    auto gt = text.find('>', i + 1);
                    if (gt != std::string_view::npos && gt - i < 80)
                    {
                        i = gt + 1;
                        continue;
                    }
                }
                if (c == '`')
                {
                    ++i;
                    continue;
                }
                if ((c == '*' || c == '~') && i + 1 < n && text[i + 1] == c)
                {
                    i += 2;
                    continue;
                }
                if (c == '*')
                {
                    ++i;
                    continue;
                }
                if ((c == '[' || (c == '!' && i + 1 < n && text[i + 1] == '[')))
                {
                    bool isImg = false;
                    std::string_view lbl;
                    std::string_view url;
                    std::size_t endPos = i;
                    if (parseBracketLink(text, i, isImg, lbl, url, endPos))
                    {
                        out.append(stripMarkupToPlainText(lbl));
                        i = endPos;
                        continue;
                    }
                }
                out.push_back(c);
                ++i;
            }
            return out;
        }

        [[nodiscard]] bool tryExtractHeading(std::string_view line, int& outLevel, std::string_view& outTitle) noexcept
        {
            std::size_t leadingCols = 0;
            std::size_t leadIdx = 0;
            while (leadIdx < line.size() && (line[leadIdx] == ' ' || line[leadIdx] == '\t'))
            {
                leadingCols += (line[leadIdx] == '\t') ? 4 : 1;
                ++leadIdx;
            }
            if (leadingCols >= 4) return false;

            std::string_view trimmed = trimSpaces(line.substr(leadIdx));
            if (trimmed.empty() || trimmed[0] != '#') return false;

            std::size_t hashes = 0;
            while (hashes < trimmed.size() && hashes < 7 && trimmed[hashes] == '#')
            {
                ++hashes;
            }
            if (hashes == 0 || hashes > 6) return false;
            if (hashes < trimmed.size() && trimmed[hashes] != ' ' && trimmed[hashes] != '\t')
            {
                return false;
            }

            std::string_view rest = trimSpaces(trimmed.substr(hashes));
            // Remove optional closing ATX hashes (`## Title ##`)
            while (!rest.empty() && rest.back() == '#')
            {
                if (rest.size() >= 2 && rest[rest.size() - 2] != ' ')
                {
                    break;
                }
                rest.remove_suffix(1);
                rest = trimSpaces(rest);
            }

            outLevel = static_cast<int>(hashes);
            outTitle = rest;
            return true;
        }

        [[nodiscard]] bool isThematicBreak(std::string_view line) noexcept
        {
            std::string_view s = trimSpaces(line);
            if (s.empty()) return false;
            char marker = '\0';
            int count = 0;
            for (char c : s)
            {
                if (c == ' ' || c == '\t') continue;
                if (c != '-' && c != '*' && c != '_') return false;
                if (marker == '\0') marker = c;
                else if (c != marker) return false;
                ++count;
            }
            return count >= 3;
        }

        [[nodiscard]] bool tryExtractFenceOpen(std::string_view line,
                                               char& outChar,
                                               std::size_t& outLen,
                                               std::string& outTag) noexcept
        {
            std::size_t indent = 0;
            while (indent < line.size() && indent < 4 && line[indent] == ' ') ++indent;
            if (indent >= 4) return false;

            std::string_view s = line.substr(indent);
            if (s.size() < 3 || (s[0] != '`' && s[0] != '~')) return false;
            const char fChar = s[0];
            std::size_t len = 0;
            while (len < s.size() && s[len] == fChar) ++len;
            if (len < 3) return false;

            std::string_view info = trimSpaces(s.substr(len));
            if (fChar == '`' && info.find('`') != std::string_view::npos) return false;

            std::size_t tagEnd = 0;
            while (tagEnd < info.size() && !mdIsSpace(info[tagEnd])) ++tagEnd;

            outChar = fChar;
            outLen = len;
            outTag = std::string(info.substr(0, tagEnd));
            return true;
        }

        [[nodiscard]] bool isFenceClose(std::string_view line, char fenceChar, std::size_t minLen) noexcept
        {
            std::size_t indent = 0;
            while (indent < line.size() && indent < 4 && line[indent] == ' ') ++indent;
            if (indent >= 4) return false;

            std::string_view s = line.substr(indent);
            if (s.empty() || s[0] != fenceChar) return false;
            std::size_t len = 0;
            while (len < s.size() && s[len] == fenceChar) ++len;
            if (len < minLen) return false;

            return trimSpaces(s.substr(len)).empty();
        }

        [[nodiscard]] bool tryExtractListItem(std::string_view line,
                                              int& outIndentLevel,
                                              bool& outOrdered,
                                              std::string& outMarker,
                                              bool& outIsTask,
                                              bool& outTaskChecked,
                                              std::string_view& outText) noexcept
        {
            std::size_t wsCols = 0;
            std::size_t i = 0;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            {
                wsCols += (line[i] == '\t') ? 4 : 1;
                ++i;
            }

            std::string_view s = line.substr(i);
            if (s.empty()) return false;

            bool ordered = false;
            std::string marker;
            std::size_t afterMarker = 0;

            if (s.size() >= 2 && (s[0] == '-' || s[0] == '*' || s[0] == '+') && (s[1] == ' ' || s[1] == '\t'))
            {
                if (isThematicBreak(s)) return false;
                ordered = false;
                marker = "•";
                afterMarker = 1;
            }
            else if (mdIsDigit(s[0]))
            {
                std::size_t d = 0;
                while (d < s.size() && d < 9 && mdIsDigit(s[d])) ++d;
                if (d < s.size() && (s[d] == '.' || s[d] == ')') &&
                    d + 1 < s.size() && (s[d + 1] == ' ' || s[d + 1] == '\t'))
                {
                    ordered = true;
                    marker = std::string(s.substr(0, d + 1));
                    afterMarker = d + 1;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }

            while (afterMarker < s.size() && (s[afterMarker] == ' ' || s[afterMarker] == '\t'))
            {
                ++afterMarker;
            }

            std::string_view rest = s.substr(afterMarker);
            bool isTask = false;
            bool taskChecked = false;
            if (!ordered && rest.size() >= 3 && rest[0] == '[' && rest[2] == ']' &&
                (rest[1] == ' ' || rest[1] == 'x' || rest[1] == 'X') &&
                (rest.size() == 3 || rest[3] == ' ' || rest[3] == '\t'))
            {
                isTask = true;
                taskChecked = (rest[1] == 'x' || rest[1] == 'X');
                rest.remove_prefix(3);
                while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t'))
                {
                    rest.remove_prefix(1);
                }
            }

            outIndentLevel = static_cast<int>(wsCols / 2);
            outOrdered = ordered;
            outMarker = std::move(marker);
            outIsTask = isTask;
            outTaskChecked = taskChecked;
            outText = rest;
            return true;
        }

        [[nodiscard]] std::vector<std::string_view> splitTableCells(std::string_view row)
        {
            std::string_view s = trimSpaces(row);
            if (!s.empty() && s.front() == '|') s.remove_prefix(1);
            if (!s.empty() && s.back() == '|') s.remove_suffix(1);

            std::vector<std::string_view> cells;
            std::size_t start = 0;
            for (std::size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] == '\\' && i + 1 < s.size())
                {
                    ++i;
                    continue;
                }
                if (s[i] == '`')
                {
                    ++i;
                    while (i < s.size() && s[i] != '`') ++i;
                    continue;
                }
                if (s[i] == '|')
                {
                    cells.push_back(trimSpaces(s.substr(start, i - start)));
                    start = i + 1;
                }
            }
            cells.push_back(trimSpaces(s.substr(start)));
            return cells;
        }

        [[nodiscard]] bool tryParseTableSeparator(std::string_view row, std::vector<std::string>& outAlignments)
        {
            std::string_view trimmed = trimSpaces(row);
            if (trimmed.find('|') == std::string_view::npos && trimmed.find('-') == std::string_view::npos)
            {
                return false;
            }
            auto cells = splitTableCells(trimmed);
            if (cells.empty()) return false;

            std::vector<std::string> aligns;
            aligns.reserve(cells.size());
            for (std::string_view cell : cells)
            {
                if (cell.empty()) return false;
                bool leftColon = (cell.front() == ':');
                bool rightColon = (cell.back() == ':');
                std::size_t start = leftColon ? 1 : 0;
                std::size_t end = rightColon ? (cell.size() - 1) : cell.size();
                if (start >= end) return false;

                int dashes = 0;
                for (std::size_t i = start; i < end; ++i)
                {
                    if (cell[i] != '-') return false;
                    ++dashes;
                }
                if (dashes < 1) return false;

                if (leftColon && rightColon) aligns.emplace_back("center");
                else if (rightColon) aligns.emplace_back("right");
                else aligns.emplace_back("left");
            }

            outAlignments = std::move(aligns);
            return true;
        }

        [[nodiscard]] bool isHtmlWrapperOnlyLine(std::string_view trimmed,
                                                 bool& ioCentered,
                                                 std::string& outExtractedSummary)
        {
            outExtractedSummary.clear();

            if (trimmed.empty() || trimmed.front() != '<') return false;
            std::string lower = toLowerCopy(trimmed);

            if (lower.starts_with("<div") && lower.find("align=\"center\"") != std::string::npos)
            {
                ioCentered = true;
                return true;
            }
            if (lower.starts_with("<center>"))
            {
                ioCentered = true;
                return true;
            }
            if (lower == "</div>" || lower == "</center>")
            {
                ioCentered = false;
                return true;
            }
            if (lower == "<br>" || lower == "<br/>" || lower == "<br />")
            {
                return true;
            }
            if (lower == "<details>" || lower == "</details>" || lower.starts_with("<p ") ||
                lower == "<p>" || lower == "</p>")
            {
                return true;
            }
            if (lower.starts_with("<summary>") && lower.ends_with("</summary>"))
            {
                outExtractedSummary = std::string(trimmed.substr(9, trimmed.size() - 19));
                return false;
            }
            if (lower.starts_with("<!--") && lower.ends_with("-->"))
            {
                return true;
            }
            return false;
        }

        [[nodiscard]] std::string formatInlineCodeHtml(std::string_view code)
        {
            std::string html = "<span style=\"background-color:#26303D;color:#A5D6FF;"
                "font-family:'Cascadia Mono','Consolas','JetBrains Mono','Courier New',monospace;\">";
            html.append("&nbsp;");
            html.append(escapeInlineText(code));
            html.append("&nbsp;");
            html.append("</span>");
            return html;
        }
    } // namespace

    std::string MarkdownParser::toGithubSlug(std::string_view headingText) noexcept
    {
        std::string plain = stripMarkupToPlainText(headingText);
        std::string_view sv = trimSpaces(plain);

        std::string slug;
        slug.reserve(sv.size());

        std::size_t i = 0;
        const std::size_t n = sv.size();
        bool started = false;
        while (i < n)
        {
            const unsigned char uc = static_cast<unsigned char>(sv[i]);
            if (uc >= 0x80)
            {
                const std::size_t seqLen = utf8SeqLen(uc);
                const std::size_t clampedLen = std::min(seqLen, n - i);
                if (clampedLen == 2)
                {
                    // Keep 2-byte UTF-8 letters (e.g. accented Latin characters)
                    unsigned char b1 = uc;
                    unsigned char b2 = (i + 1 < n) ? static_cast<unsigned char>(sv[i + 1]) : 0;
                    if (b1 == 0xC3 && b2 >= 0x80 && b2 <= 0x9E && b2 != 0x97)
                    {
                        slug.push_back(static_cast<char>(b1));
                        slug.push_back(static_cast<char>(b2 + 0x20));
                    }
                    else
                    {
                        slug.append(sv.substr(i, clampedLen));
                    }
                    started = true;
                }
                // Skip 3-byte / 4-byte symbols & emojis (e.g. ⚡, 🏛️) like GitHub anchors do
                i += clampedLen;
                continue;
            }

            const char c = sv[i];
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
            {
                slug.push_back(c);
                started = true;
            }
            else if (c >= 'A' && c <= 'Z')
            {
                slug.push_back(mdToLowerAscii(c));
                started = true;
            }
            else if (c == ' ')
            {
                if (started)
                {
                    slug.push_back('-');
                }
            }
            ++i;
        }

        while (!slug.empty() && slug.back() == '-')
        {
            slug.pop_back();
        }
        return slug;
    }

    std::string MarkdownParser::renderInlineHtml(std::string_view text) noexcept
    {
        std::string out;
        out.reserve(text.size() * 2 + 16);

        const std::size_t n = text.size();
        std::size_t i = 0;

        while (i < n)
        {
            const unsigned char uc = static_cast<unsigned char>(text[i]);
            const char c = text[i];

            // Preserve full UTF-8 multi-byte sequences intact
            if (uc >= 0x80)
            {
                std::size_t start = i;
                while (i < n && static_cast<unsigned char>(text[i]) >= 0x80)
                {
                    i = std::min(n, i + utf8SeqLen(static_cast<unsigned char>(text[i])));
                }
                out.append(text.substr(start, i - start));
                continue;
            }

            // Escaped character
            if (c == '\\' && i + 1 < n)
            {
                out.append(escapeInlineText(text.substr(i + 1, 1)));
                i += 2;
                continue;
            }

            // Inline code span (`code` or ``code``)
            if (c == '`')
            {
                std::size_t tickStart = i;
                while (i < n && text[i] == '`') ++i;
                const std::size_t tickLen = i - tickStart;

                std::size_t search = i;
                bool found = false;
                std::size_t codeEnd = i;
                std::size_t afterClose = i;
                while (search < n)
                {
                    if (text[search] == '`')
                    {
                        std::size_t runEnd = search;
                        while (runEnd < n && text[runEnd] == '`') ++runEnd;
                        if (runEnd - search == tickLen)
                        {
                            found = true;
                            codeEnd = search;
                            afterClose = runEnd;
                            break;
                        }
                        search = runEnd;
                    }
                    else
                    {
                        ++search;
                    }
                }

                if (found)
                {
                    out.append(formatInlineCodeHtml(text.substr(i, codeEnd - i)));
                    i = afterClose;
                }
                else
                {
                    out.append(text.substr(tickStart, tickLen));
                }
                continue;
            }

            // Simple inline HTML tags
            if (c == '<')
            {
                std::string_view sub = text.substr(i);
                if (sub.starts_with("<!--"))
                {
                    auto endPos = text.find("-->", i + 4);
                    if (endPos != std::string_view::npos)
                    {
                        i = endPos + 3;
                    }
                    else
                    {
                        break;
                    }
                    continue;
                }
                if (sub.starts_with("http://") || sub.starts_with("https://"))
                {
                    // Handled below
                }
                else if (i + 1 < n && (sub[1] == 'h' || sub[1] == 'H'))
                {
                    auto gt = text.find('>', i + 1);
                    if (gt != std::string_view::npos &&
                        (sub.substr(1).starts_with("http://") || sub.substr(1).starts_with("https://")))
                    {
                        std::string_view url = text.substr(i + 1, gt - (i + 1));
                        out.append("<a href=\"");
                        out.append(escapeHrefAttr(url));
                        out.append("\" style=\"color:#58A6FF;text-decoration:none;\">");
                        out.append(escapeInlineText(url));
                        out.append("</a>");
                        i = gt + 1;
                        continue;
                    }
                }

                auto gt = text.find('>', i + 1);
                if (gt != std::string_view::npos && gt - i < 120)
                {
                    std::string tagLower = toLowerCopy(text.substr(i, gt - i + 1));
                    if (tagLower == "<br>" || tagLower == "<br/>" || tagLower == "<br />")
                    {
                        out.append("<br/>");
                        i = gt + 1;
                        continue;
                    }
                    if (tagLower == "<b>" || tagLower == "<strong>")
                    {
                        out.append("<b>");
                        i = gt + 1;
                        continue;
                    }
                    if (tagLower == "</b>" || tagLower == "</strong>")
                    {
                        out.append("</b>");
                        i = gt + 1;
                        continue;
                    }
                    if (tagLower == "<i>" || tagLower == "<em>")
                    {
                        out.append("<i>");
                        i = gt + 1;
                        continue;
                    }
                    if (tagLower == "</i>" || tagLower == "</em>")
                    {
                        out.append("</i>");
                        i = gt + 1;
                        continue;
                    }
                    if (tagLower == "<s>" || tagLower == "<del>")
                    {
                        out.append("<s>");
                        i = gt + 1;
                        continue;
                    }
                    if (tagLower == "</s>" || tagLower == "</del>")
                    {
                        out.append("</s>");
                        i = gt + 1;
                        continue;
                    }
                    if (tagLower == "<code>" || tagLower == "<kbd>")
                    {
                        out.append("<span style=\"background-color:#26303D;color:#A5D6FF;"
                            "font-family:'Cascadia Mono','Consolas','JetBrains Mono','Courier New',monospace;\">&nbsp;");
                        i = gt + 1;
                        continue;
                    }
                    if (tagLower == "</code>" || tagLower == "</kbd>")
                    {
                        out.append("&nbsp;</span>");
                        i = gt + 1;
                        continue;
                    }
                    if (tagLower == "</div>" || tagLower == "</center>" || tagLower == "<center>" ||
                        tagLower.starts_with("<div") || tagLower.starts_with("<summary") || tagLower == "</summary>")
                    {
                        i = gt + 1;
                        continue;
                    }
                }
            }

            // Bare HTTP/HTTPS autolink
            if (c == 'h' && (text.substr(i).starts_with("https://") || text.substr(i).starts_with("http://")) &&
                (i == 0 || mdIsSpace(text[i - 1]) || text[i - 1] == '('))
            {
                std::size_t start = i;
                int parenDepth = 0;
                while (i < n && !mdIsSpace(text[i]) && text[i] != '"' && text[i] != '<' && text[i] != '>')
                {
                    if (text[i] == '(') ++parenDepth;
                    else if (text[i] == ')')
                    {
                        if (parenDepth == 0) break;
                        --parenDepth;
                    }
                    ++i;
                }
                while (i > start && (text[i - 1] == '.' || text[i - 1] == ',' || text[i - 1] == ';'))
                {
                    --i;
                }
                std::string_view url = text.substr(start, i - start);
                out.append("<a href=\"");
                out.append(escapeHrefAttr(url));
                out.append("\" style=\"color:#58A6FF;text-decoration:none;\">");
                out.append(escapeInlineText(url));
                out.append("</a>");
                continue;
            }

            // Markdown link or image: [label](url) / ![alt](url)
            if ((c == '[' || (c == '!' && i + 1 < n && text[i + 1] == '[')))
            {
                bool isImg = false;
                std::string_view label;
                std::string_view url;
                std::size_t endPos = i;
                if (parseBracketLink(text, i, isImg, label, url, endPos))
                {
                    if (isImg)
                    {
                        out.append("<span style=\"background-color:#1F6FEB;color:#FFFFFF;font-weight:bold;\">&nbsp;");
                        out.append(escapeInlineText(label.empty() ? std::string_view("image") : label));
                        out.append("&nbsp;</span>");
                    }
                    else
                    {
                        out.append("<a href=\"");
                        out.append(escapeHrefAttr(url));
                        out.append("\" style=\"color:#58A6FF;text-decoration:none;\">");
                        out.append(renderInlineHtml(label));
                        out.append("</a>");
                    }
                    i = endPos;
                    continue;
                }
            }

            // Bold **...** or __...__
            if ((c == '*' || c == '_') && i + 1 < n && text[i + 1] == c && i + 2 < n && !mdIsSpace(text[i + 2]))
            {
                const char delim[3] = {c, c, '\0'};
                auto closePos = text.find(delim, i + 2);
                if (closePos != std::string_view::npos && closePos > i + 2 && !mdIsSpace(text[closePos - 1]))
                {
                    out.append("<b style=\"color:#F0F6FC;\">");
                    out.append(renderInlineHtml(text.substr(i + 2, closePos - (i + 2))));
                    out.append("</b>");
                    i = closePos + 2;
                    continue;
                }
            }

            // Italic *...*
            if (c == '*' && !(i + 1 < n && text[i + 1] == '*') && i + 1 < n && !mdIsSpace(text[i + 1]))
            {
                std::size_t search = i + 1;
                std::size_t closePos = std::string_view::npos;
                while (search < n)
                {
                    if (text[search] == '\\' && search + 1 < n)
                    {
                        search += 2;
                        continue;
                    }
                    if (text[search] == '*')
                    {
                        if (search + 1 < n && text[search + 1] == '*')
                        {
                            search += 2;
                            continue;
                        }
                        if (!mdIsSpace(text[search - 1]))
                        {
                            closePos = search;
                        }
                        break;
                    }
                    ++search;
                }
                if (closePos != std::string_view::npos)
                {
                    out.append("<i>");
                    out.append(renderInlineHtml(text.substr(i + 1, closePos - (i + 1))));
                    out.append("</i>");
                    i = closePos + 1;
                    continue;
                }
            }

            // Strikethrough ~~...~~
            if (c == '~' && i + 1 < n && text[i + 1] == '~' && i + 2 < n && !mdIsSpace(text[i + 2]))
            {
                auto closePos = text.find("~~", i + 2);
                if (closePos != std::string_view::npos && closePos > i + 2)
                {
                    out.append("<s style=\"color:#8B949E;\">");
                    out.append(renderInlineHtml(text.substr(i + 2, closePos - (i + 2))));
                    out.append("</s>");
                    i = closePos + 2;
                    continue;
                }
            }

            // Plain escaped character
            switch (c)
            {
            case '&': out.append("&amp;");
                break;
            case '<': out.append("&lt;");
                break;
            case '>': out.append("&gt;");
                break;
            case '"': out.append("&quot;");
                break;
            default: out.push_back(c);
                break;
            }
            ++i;
        }

        return out;
    }

    MarkdownDocument MarkdownParser::parse(std::string_view markdownText) noexcept
    {
        MarkdownDocument doc;
        std::string normalized = normalizeLineEndings(markdownText);
        auto lines = splitNormalizedLines(normalized);
        if (lines.empty())
        {
            return doc;
        }

        std::unordered_map<std::string, int> slugCounts;
        bool centeredBlock = false;
        const std::size_t n = lines.size();
        std::size_t idx = 0;

        while (idx < n)
        {
            const auto& cur = lines[idx];
            std::string_view trimmed = trimSpaces(cur.text);

            if (trimmed.empty())
            {
                ++idx;
                continue;
            }

            // Skip multi-line HTML comments
            if (trimmed.starts_with("<!--") && trimmed.find("-->") == std::string_view::npos)
            {
                ++idx;
                while (idx < n && lines[idx].text.find("-->") == std::string_view::npos)
                {
                    ++idx;
                }
                if (idx < n) ++idx;
                continue;
            }

            // Wrapper HTML tags (<div align="center">, </div>, <br/>, <details>, etc.)
            std::string summaryInner;
            if (isHtmlWrapperOnlyLine(trimmed, centeredBlock, summaryInner))
            {
                ++idx;
                continue;
            }
            if (!summaryInner.empty())
            {
                MarkdownBlock b;
                b.kind = MarkdownBlockKind::Paragraph;
                b.centered = centeredBlock;
                b.rawText = summaryInner;
                b.html = "<b style=\"color:#79C0FF;\">&#9654; " + renderInlineHtml(summaryInner) + "</b>";
                b.startLine = cur.lineNumber;
                b.endLine = cur.lineNumber;
                doc.blocks.push_back(std::move(b));
                ++idx;
                continue;
            }

            // 1. Fenced code block (``` or ~~~)
            char fenceChar = '`';
            std::size_t fenceLen = 0;
            std::string langTag;
            if (tryExtractFenceOpen(cur.text, fenceChar, fenceLen, langTag))
            {
                const int startLine = cur.lineNumber;
                std::string rawCode;
                ++idx;
                while (idx < n && !isFenceClose(lines[idx].text, fenceChar, fenceLen))
                {
                    if (!rawCode.empty()) rawCode.push_back('\n');
                    rawCode.append(lines[idx].text);
                    ++idx;
                }
                int endLine = (idx < n) ? lines[idx].lineNumber : lines.back().lineNumber;
                if (idx < n) ++idx;

                const Language detectedLang = LanguageDetector::fromCodeFenceInfo(langTag);
                std::string lowerTag = toLowerCopy(langTag);
                const bool isDiagram = (lowerTag == "mermaid" || lowerTag == "plantuml" || lowerTag == "dot");

                MarkdownBlock b;
                b.kind = MarkdownBlockKind::CodeBlock;
                b.centered = false;
                b.isDiagram = isDiagram;
                b.languageTag = langTag.empty() ? "text" : std::move(langTag);
                b.detectedLanguage = detectedLang;
                b.rawText = rawCode;
                b.html = SyntaxHighlighter::highlightCodeBlockHtml(rawCode, detectedLang);
                b.startLine = startLine;
                b.endLine = endLine;
                doc.blocks.push_back(std::move(b));
                continue;
            }

            // 1b. CommonMark Indented Code Block (4 spaces or tab, outside <div align="center"> and nested list items)
            if (!centeredBlock && (cur.text.starts_with("    ") || cur.text.starts_with('\t')))
            {
                int tmpInd = 0;
                bool tmpOrd = false;
                std::string tmpMarker;
                bool tmpTask = false;
                bool tmpChk = false;
                std::string_view tmpTxt;
                const bool isNestedList = tryExtractListItem(cur.text, tmpInd, tmpOrd, tmpMarker, tmpTask, tmpChk,
                                                             tmpTxt);
                if (!isNestedList)
                {
                    auto stripFourSpaceIndent = [](std::string_view line) -> std::string_view
                    {
                        if (line.starts_with('\t')) return line.substr(1);
                        std::size_t s = 0;
                        while (s < 4 && s < line.size() && line[s] == ' ') ++s;
                        return line.substr(s);
                    };

                    const int startLine = cur.lineNumber;
                    int endLine = cur.lineNumber;
                    std::string rawCode;

                    while (idx < n)
                    {
                        std::string_view rawLine = lines[idx].text;
                        std::string_view lineTrim = trimSpaces(rawLine);
                        if (lineTrim.empty())
                        {
                            std::size_t peek = idx + 1;
                            while (peek < n && trimSpaces(lines[peek].text).empty())
                            {
                                ++peek;
                            }
                            bool peekIsNestedList = false;
                            if (peek < n)
                            {
                                peekIsNestedList = tryExtractListItem(
                                    lines[peek].text, tmpInd, tmpOrd, tmpMarker, tmpTask, tmpChk, tmpTxt);
                            }
                            if (peek < n && !peekIsNestedList &&
                                (lines[peek].text.starts_with("    ") || lines[peek].text.starts_with('\t')))
                            {
                                rawCode.push_back('\n');
                                ++idx;
                                continue;
                            }
                            break;
                        }
                        if (!rawLine.starts_with("    ") && !rawLine.starts_with('\t'))
                        {
                            break;
                        }
                        if (!rawCode.empty()) rawCode.push_back('\n');
                        rawCode.append(stripFourSpaceIndent(rawLine));
                        endLine = lines[idx].lineNumber;
                        ++idx;
                    }

                    MarkdownBlock b;
                    b.kind = MarkdownBlockKind::CodeBlock;
                    b.centered = false;
                    b.isDiagram = false;
                    b.languageTag = "text";
                    b.detectedLanguage = Language::Unknown;
                    b.rawText = rawCode;
                    b.html = SyntaxHighlighter::highlightCodeBlockHtml(rawCode, Language::Unknown);
                    b.startLine = startLine;
                    b.endLine = endLine;
                    doc.blocks.push_back(std::move(b));
                    continue;
                }
            }

            // 2. ATX Heading (# Title)
            int hLevel = 1;
            std::string_view hTitle;
            if (tryExtractHeading(cur.text, hLevel, hTitle))
            {
                std::string plainTitle = std::string(trimSpaces(stripMarkupToPlainText(hTitle)));
                std::string baseSlug = toGithubSlug(hTitle);
                if (baseSlug.empty()) baseSlug = "section";

                int& count = slugCounts[baseSlug];
                std::string uniqueSlug = (count == 0) ? baseSlug : (baseSlug + "-" + std::to_string(count));
                ++count;

                MarkdownBlock b;
                b.kind = MarkdownBlockKind::Heading;
                b.level = hLevel;
                b.centered = centeredBlock;
                b.rawText = plainTitle;
                b.html = renderInlineHtml(hTitle);
                b.anchor = uniqueSlug;
                b.startLine = cur.lineNumber;
                b.endLine = cur.lineNumber;

                const int blockIndex = static_cast<int>(doc.blocks.size());
                doc.blocks.push_back(std::move(b));
                doc.toc.push_back(MarkdownTocEntry{
                    .level = hLevel,
                    .title = std::move(plainTitle),
                    .anchor = std::move(uniqueSlug),
                    .blockIndex = blockIndex,
                    .startLine = cur.lineNumber
                });
                ++idx;
                continue;
            }

            // 3. Thematic break (---, ***, ___)
            if (isThematicBreak(cur.text))
            {
                MarkdownBlock b;
                b.kind = MarkdownBlockKind::ThematicBreak;
                b.startLine = cur.lineNumber;
                b.endLine = cur.lineNumber;
                doc.blocks.push_back(std::move(b));
                ++idx;
                continue;
            }

            // 4. Consecutive badge row ([![alt](img)](url) ...)
            std::vector<MarkdownBadge> badges;
            if (tryParseBadgeLine(trimmed, badges))
            {
                const int startLine = cur.lineNumber;
                int endLine = cur.lineNumber;
                ++idx;
                while (idx < n)
                {
                    std::string_view nextTrim = trimSpaces(lines[idx].text);
                    if (nextTrim.empty()) break;
                    if (!tryParseBadgeLine(nextTrim, badges)) break;
                    endLine = lines[idx].lineNumber;
                    ++idx;
                }
                MarkdownBlock b;
                b.kind = MarkdownBlockKind::BadgeRow;
                b.centered = centeredBlock;
                b.badges = std::move(badges);
                b.startLine = startLine;
                b.endLine = endLine;
                doc.blocks.push_back(std::move(b));
                continue;
            }

            // 5. Blockquote & GitHub Callout Alerts (> ...)
            if (trimmed[0] == '>')
            {
                const int startLine = cur.lineNumber;
                int endLine = cur.lineNumber;
                MarkdownAlertType alertType = MarkdownAlertType::None;
                std::string bodyText;

                bool firstLine = true;
                while (idx < n)
                {
                    std::string_view qt = trimSpaces(lines[idx].text);
                    if (qt.empty() || qt[0] != '>') break;
                    endLine = lines[idx].lineNumber;

                    std::size_t skip = 1;
                    if (skip < qt.size() && qt[skip] == ' ') ++skip;
                    std::string_view inner = trimSpaces(qt.substr(skip));

                    if (firstLine)
                    {
                        firstLine = false;
                        struct AlertDef
                        {
                            std::string_view tag;
                            MarkdownAlertType type;
                        };
                        constexpr AlertDef kAlerts[] = {
                            {"[!NOTE]", MarkdownAlertType::Note},
                            {"[!TIP]", MarkdownAlertType::Tip},
                            {"[!IMPORTANT]", MarkdownAlertType::Important},
                            {"[!WARNING]", MarkdownAlertType::Warning},
                            {"[!CAUTION]", MarkdownAlertType::Caution}
                        };
                        bool matchedAlert = false;
                        for (const auto& a : kAlerts)
                        {
                            if (inner.starts_with(a.tag))
                            {
                                alertType = a.type;
                                inner = trimSpaces(inner.substr(a.tag.size()));
                                matchedAlert = true;
                                break;
                            }
                        }
                        if (matchedAlert && inner.empty())
                        {
                            ++idx;
                            continue;
                        }
                    }

                    if (!inner.empty())
                    {
                        if (!bodyText.empty()) bodyText.push_back(' ');
                        bodyText.append(inner);
                    }
                    else if (!bodyText.empty())
                    {
                        bodyText.append("\n");
                    }
                    ++idx;
                }

                MarkdownBlock b;
                b.kind = MarkdownBlockKind::Blockquote;
                b.centered = centeredBlock;
                b.alertType = alertType;
                b.rawText = bodyText;
                b.html = renderInlineHtml(bodyText);
                b.startLine = startLine;
                b.endLine = endLine;
                doc.blocks.push_back(std::move(b));
                continue;
            }

            // 6. GFM Pipe Table (header row + separator row)
            if (idx + 1 < n && trimmed.find('|') != std::string_view::npos)
            {
                std::vector<std::string> alignments;
                if (tryParseTableSeparator(lines[idx + 1].text, alignments))
                {
                    const int startLine = cur.lineNumber;
                    int endLine = lines[idx + 1].lineNumber;

                    auto rawHeaders = splitTableCells(cur.text);
                    const std::size_t colCount = std::max(rawHeaders.size(), alignments.size());
                    alignments.resize(colCount, "left");

                    std::vector<std::string> headersHtml;
                    headersHtml.reserve(colCount);
                    for (std::size_t c = 0; c < colCount; ++c)
                    {
                        headersHtml.push_back(c < rawHeaders.size() ? renderInlineHtml(rawHeaders[c]) : "");
                    }

                    idx += 2;
                    std::vector<std::vector<std::string>> rowsHtml;
                    while (idx < n)
                    {
                        std::string_view rowTrim = trimSpaces(lines[idx].text);
                        if (rowTrim.empty() || rowTrim.find('|') == std::string_view::npos) break;
                        if (rowTrim[0] == '>' || isThematicBreak(rowTrim)) break;

                        auto cells = splitTableCells(rowTrim);
                        std::vector<std::string> rowOut;
                        rowOut.reserve(colCount);
                        for (std::size_t c = 0; c < colCount; ++c)
                        {
                            rowOut.push_back(c < cells.size() ? renderInlineHtml(cells[c]) : "");
                        }
                        rowsHtml.push_back(std::move(rowOut));
                        endLine = lines[idx].lineNumber;
                        ++idx;
                    }

                    MarkdownBlock b;
                    b.kind = MarkdownBlockKind::Table;
                    b.tableHeaders = std::move(headersHtml);
                    b.tableAlignments = std::move(alignments);
                    b.tableRows = std::move(rowsHtml);
                    b.startLine = startLine;
                    b.endLine = endLine;
                    doc.blocks.push_back(std::move(b));
                    continue;
                }
            }

            // 7. List Item (-, *, +, 1.)
            {
                int indentLevel = 0;
                bool ordered = false;
                std::string marker;
                bool isTask = false;
                bool taskChecked = false;
                std::string_view itemText;

                if (tryExtractListItem(cur.text, indentLevel, ordered, marker, isTask, taskChecked, itemText))
                {
                    const int startLine = cur.lineNumber;
                    int endLine = cur.lineNumber;
                    std::string combinedText(itemText);
                    ++idx;

                    // Gather lazy continuation lines indented without starting a new block
                    while (idx < n)
                    {
                        std::string_view nextRaw = lines[idx].text;
                        std::string_view nextTrim = trimSpaces(nextRaw);
                        if (nextTrim.empty()) break;

                        int tmpIndent = 0;
                        bool tmpOrd = false;
                        std::string tmpMarker;
                        bool tmpTask = false;
                        bool tmpChk = false;
                        std::string_view tmpTxt;
                        if (tryExtractListItem(nextRaw, tmpIndent, tmpOrd, tmpMarker, tmpTask, tmpChk, tmpTxt)) break;

                        int tmpH = 0;
                        std::string_view tmpHT;
                        char tmpFc = '`';
                        std::size_t tmpFl = 0;
                        std::string tmpTag;
                        if (tryExtractHeading(nextRaw, tmpH, tmpHT) ||
                            tryExtractFenceOpen(nextRaw, tmpFc, tmpFl, tmpTag) ||
                            isThematicBreak(nextRaw) || nextTrim[0] == '>')
                        {
                            break;
                        }

                        if (!combinedText.empty()) combinedText.push_back(' ');
                        combinedText.append(nextTrim);
                        endLine = lines[idx].lineNumber;
                        ++idx;
                    }

                    MarkdownBlock b;
                    b.kind = MarkdownBlockKind::ListItem;
                    b.indentLevel = indentLevel;
                    b.ordered = ordered;
                    b.listMarker = std::move(marker);
                    b.isTask = isTask;
                    b.taskChecked = taskChecked;
                    b.rawText = combinedText;
                    b.html = renderInlineHtml(combinedText);
                    b.startLine = startLine;
                    b.endLine = endLine;
                    doc.blocks.push_back(std::move(b));
                    continue;
                }
            }

            // 8. Paragraph (gather contiguous non-blank lines that do not open another block)
            {
                const int startLine = cur.lineNumber;
                int endLine = cur.lineNumber;
                std::string paraText(trimmed);
                ++idx;

                while (idx < n)
                {
                    std::string_view nextRaw = lines[idx].text;
                    std::string_view nextTrim = trimSpaces(nextRaw);
                    if (nextTrim.empty()) break;

                    bool dummyCenter = centeredBlock;
                    std::string tmpSum;
                    if (isHtmlWrapperOnlyLine(nextTrim, dummyCenter, tmpSum) || !tmpSum.empty())
                    {
                        break;
                    }

                    int tmpH = 0;
                    std::string_view tmpHT;
                    char tmpFc = '`';
                    std::size_t tmpFl = 0;
                    std::string tmpTag;
                    int tmpInd = 0;
                    bool tmpOrd = false;
                    std::string tmpMarker;
                    bool tmpTask = false;
                    bool tmpChk = false;
                    std::string_view tmpListTxt;
                    std::vector<MarkdownBadge> tmpBadges;

                    if (tryExtractHeading(nextRaw, tmpH, tmpHT) ||
                        tryExtractFenceOpen(nextRaw, tmpFc, tmpFl, tmpTag) ||
                        isThematicBreak(nextRaw) ||
                        nextTrim[0] == '>' ||
                        tryParseBadgeLine(nextTrim, tmpBadges) ||
                        tryExtractListItem(nextRaw, tmpInd, tmpOrd, tmpMarker, tmpTask, tmpChk, tmpListTxt))
                    {
                        break;
                    }

                    if (idx + 1 < n && nextTrim.find('|') != std::string_view::npos)
                    {
                        std::vector<std::string> aligns;
                        if (tryParseTableSeparator(lines[idx + 1].text, aligns))
                        {
                            break;
                        }
                    }

                    if (!paraText.empty())
                    {
                        // Preserve explicit trailing two-space / <br/> break if present
                        if (paraText.ends_with("  ") || paraText.ends_with("<br/>") || paraText.ends_with("<br>"))
                        {
                            paraText.append("<br/>");
                        }
                        else
                        {
                            paraText.push_back(' ');
                        }
                    }
                    paraText.append(nextTrim);
                    endLine = lines[idx].lineNumber;
                    ++idx;
                }

                MarkdownBlock b;
                b.kind = MarkdownBlockKind::Paragraph;
                b.centered = centeredBlock;
                b.rawText = paraText;
                b.html = renderInlineHtml(paraText);
                b.startLine = startLine;
                b.endLine = endLine;
                doc.blocks.push_back(std::move(b));
            }
        }

        return doc;
    }

    MarkdownDocument MarkdownParser::parseWithDiff(std::string_view markdownText,
                                                   const gf::git::bo::FileDiff* diff) noexcept
    {
        MarkdownDocument doc = parse(markdownText);
        if (diff == nullptr || diff->hunks.empty())
        {
            return doc;
        }

        if (diff->isDeletedFile)
        {
            for (auto& b : doc.blocks)
            {
                b.changeStatus = MarkdownChangeStatus::Deleted;
                b.deletedLineCount = std::max(1, b.endLine - b.startLine + 1);
            }
            return doc;
        }

        std::set<int> addedLines;
        std::map<int, std::vector<std::string>> deletionsBeforeNewLine;
        std::vector<std::string> trailingDeletions;

        for (const auto& hunk : diff->hunks)
        {
            std::vector<std::string> pendingDels;
            for (const auto& line : hunk.lines)
            {
                if (line.kind == gf::git::bo::DiffLineKind::Deletion)
                {
                    pendingDels.push_back(line.content);
                }
                else if (line.kind == gf::git::bo::DiffLineKind::Addition)
                {
                    if (line.newLineNumber.has_value())
                    {
                        addedLines.insert(*line.newLineNumber);
                        if (!pendingDels.empty())
                        {
                            auto& vec = deletionsBeforeNewLine[*line.newLineNumber];
                            vec.insert(vec.end(), pendingDels.begin(), pendingDels.end());
                            pendingDels.clear();
                        }
                    }
                }
                else if (line.kind == gf::git::bo::DiffLineKind::Context)
                {
                    if (!pendingDels.empty() && line.newLineNumber.has_value())
                    {
                        auto& vec = deletionsBeforeNewLine[*line.newLineNumber];
                        vec.insert(vec.end(), pendingDels.begin(), pendingDels.end());
                        pendingDels.clear();
                    }
                }
            }
            if (!pendingDels.empty())
            {
                trailingDeletions.insert(trailingDeletions.end(), pendingDels.begin(), pendingDels.end());
            }
        }

        std::vector<MarkdownBlock> finalBlocks;
        finalBlocks.reserve(doc.blocks.size() + 4);
        std::unordered_map<int, int> oldToNewBlockIndex;

        auto delIt = deletionsBeforeNewLine.begin();

        for (std::size_t bIdx = 0; bIdx < doc.blocks.size(); ++bIdx)
        {
            MarkdownBlock block = doc.blocks[bIdx];

            // Emit pure deletion blocks strictly preceding this block (outside added replacements)
            std::vector<std::string> pureBeforeDels;
            while (delIt != deletionsBeforeNewLine.end() && delIt->first < block.startLine)
            {
                pureBeforeDels.insert(pureBeforeDels.end(), delIt->second.begin(), delIt->second.end());
                ++delIt;
            }
            if (!pureBeforeDels.empty())
            {
                MarkdownBlock delSnippet;
                delSnippet.kind = MarkdownBlockKind::DeletedSnippet;
                delSnippet.changeStatus = MarkdownChangeStatus::Deleted;
                delSnippet.deletedLineCount = static_cast<int>(pureBeforeDels.size());
                delSnippet.deletedLines = std::move(pureBeforeDels);
                delSnippet.startLine = block.startLine;
                delSnippet.endLine = block.startLine;
                finalBlocks.push_back(std::move(delSnippet));
            }

            // Count added lines within [startLine, endLine]
            int addedInBlock = 0;
            for (int lineNum = block.startLine; lineNum <= block.endLine; ++lineNum)
            {
                if (addedLines.contains(lineNum))
                {
                    ++addedInBlock;
                }
            }

            // Gather deletions attached to lines within [startLine, endLine]
            std::vector<std::string> blockDels;
            while (delIt != deletionsBeforeNewLine.end() && delIt->first <= block.endLine)
            {
                blockDels.insert(blockDels.end(), delIt->second.begin(), delIt->second.end());
                ++delIt;
            }

            if (addedInBlock > 0 && !blockDels.empty())
            {
                block.changeStatus = MarkdownChangeStatus::Modified;
                block.addedLineCount = addedInBlock;
                block.deletedLineCount = static_cast<int>(blockDels.size());
                block.deletedLines = std::move(blockDels);
            }
            else if (addedInBlock > 0)
            {
                block.changeStatus = MarkdownChangeStatus::Added;
                block.addedLineCount = addedInBlock;
            }
            else if (!blockDels.empty())
            {
                // Deletion immediately at the start/inside an unchanged block
                MarkdownBlock delSnippet;
                delSnippet.kind = MarkdownBlockKind::DeletedSnippet;
                delSnippet.changeStatus = MarkdownChangeStatus::Deleted;
                delSnippet.deletedLineCount = static_cast<int>(blockDels.size());
                delSnippet.deletedLines = std::move(blockDels);
                delSnippet.startLine = block.startLine;
                delSnippet.endLine = block.startLine;
                finalBlocks.push_back(std::move(delSnippet));
            }

            oldToNewBlockIndex[static_cast<int>(bIdx)] = static_cast<int>(finalBlocks.size());
            finalBlocks.push_back(std::move(block));
        }

        // Any remaining deletions after the last block
        std::vector<std::string> eofDels;
        while (delIt != deletionsBeforeNewLine.end())
        {
            eofDels.insert(eofDels.end(), delIt->second.begin(), delIt->second.end());
            ++delIt;
        }
        eofDels.insert(eofDels.end(), trailingDeletions.begin(), trailingDeletions.end());
        if (!eofDels.empty())
        {
            MarkdownBlock delSnippet;
            delSnippet.kind = MarkdownBlockKind::DeletedSnippet;
            delSnippet.changeStatus = MarkdownChangeStatus::Deleted;
            delSnippet.deletedLineCount = static_cast<int>(eofDels.size());
            delSnippet.deletedLines = std::move(eofDels);
            delSnippet.startLine = doc.blocks.empty() ? 1 : (doc.blocks.back().endLine + 1);
            delSnippet.endLine = delSnippet.startLine;
            finalBlocks.push_back(std::move(delSnippet));
        }

        for (auto& tocEntry : doc.toc)
        {
            if (auto it = oldToNewBlockIndex.find(tocEntry.blockIndex); it != oldToNewBlockIndex.end())
            {
                tocEntry.blockIndex = it->second;
            }
        }

        doc.blocks = std::move(finalBlocks);
        return doc;
    }
} // namespace gf::core::syntax
