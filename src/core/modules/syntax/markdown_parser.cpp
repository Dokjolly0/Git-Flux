#include "core/modules/syntax/markdown_parser.hpp"
#include "core/modules/syntax/markdown_scan_utils.hpp"
#include "core/modules/syntax/syntax_highlighter.hpp"
#include "core/shared/html_utils.hpp"
#include "core/shared/string_utils.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

namespace gf::core::syntax
{
    namespace
    {
        [[nodiscard]] constexpr bool mdIsSpace(const char c) noexcept
        {
            return isAsciiWhitespace(c);
        }

        [[nodiscard]] constexpr bool mdIsDigit(const char c) noexcept
        {
            return c >= '0' && c <= '9';
        }

        [[nodiscard]] constexpr std::size_t utf8SeqLen(const unsigned char b) noexcept
        {
            return core::utf8SeqLen(b);
        }

        [[nodiscard]] constexpr std::string_view trimSpaces(const std::string_view sv) noexcept
        {
            return core::trim(sv);
        }

        [[nodiscard]] std::string toLowerCopy(const std::string_view sv)
        {
            return core::toLowerAscii(sv);
        }

        [[nodiscard]] std::string normalizeLineEndings(const std::string_view input)
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

        [[nodiscard]] std::vector<LineSlice> splitNormalizedLines(const std::string_view text)
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
                const auto nl = text.find('\n', pos);
                if (nl == std::string_view::npos)
                {
                    lines.push_back({.text = text.substr(pos), .lineNumber = lineNum});
                    break;
                }
                lines.push_back({.text = text.substr(pos, nl - pos), .lineNumber = lineNum});
                pos = nl + 1;
                ++lineNum;
            }
            return lines;
        }

        [[nodiscard]] std::string escapeInlineText(const std::string_view text)
        {
            return core::escapeHtmlBasic(text);
        }

        [[nodiscard]] std::string escapeHrefAttr(const std::string_view url)
        {
            std::string out;
            out.reserve(url.size() + 8);
            for (const char c : url)
            {
                if (c == '"') out.append("%22");
                else if (c == '&') out.append("&amp;");
                else if (c == '<') out.append("%3C");
                else if (c == '>') out.append("%3E");
                else out.push_back(c);
            }
            return out;
        }

        struct BracketLink
        {
            bool isImage = false;
            std::string_view label;
            std::string_view url;
            std::size_t endPos = 0;
        };

        [[nodiscard]] std::optional<BracketLink> parseBracketLink(const std::string_view sv,
                                                                  const std::size_t startPos) noexcept
        {
            const auto span = detail::scanBracketLinkSpan(sv, startPos);
            if (!span) return std::nullopt;

            std::string_view urlPart = trimSpaces(span->rawUrl);
            // Strip optional link title `url "title"`
            if (!urlPart.empty() && urlPart.front() == '<')
            {
                if (const auto gt = urlPart.find('>'); gt != std::string_view::npos)
                {
                    urlPart = urlPart.substr(1, gt - 1);
                }
            }
            else if (const auto sp = urlPart.find(' '); sp != std::string_view::npos)
            {
                urlPart = trimSpaces(urlPart.substr(0, sp));
            }

            return BracketLink{
                .isImage = span->isImage,
                .label = span->label,
                .url = urlPart,
                .endPos = span->parenClose + 1
            };
        }

        [[nodiscard]] std::string badgeColorFromImageUrl(const std::string_view imgUrl)
        {
            const std::string lower = toLowerCopy(imgUrl);
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

        [[nodiscard]] std::optional<std::vector<MarkdownBadge>> tryParseBadgeLine(const std::string_view line)
        {
            const std::string_view s = trimSpaces(line);
            if (s.empty()) return std::nullopt;

            std::vector<MarkdownBadge> parsed;
            std::size_t i = 0;
            while (i < s.size())
            {
                while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
                if (i >= s.size()) break;

                const auto link = parseBracketLink(s, i);
                if (!link)
                {
                    return std::nullopt;
                }

                if (link->isImage)
                {
                    auto cleanLabel = std::string(trimSpaces(link->label));
                    if (cleanLabel.empty()) cleanLabel = "badge";
                    parsed.push_back(MarkdownBadge{
                        .label = std::move(cleanLabel),
                        .href = "",
                        .color = badgeColorFromImageUrl(link->url)
                    });
                    i = link->endPos;
                    continue;
                }

                // Check if link label itself is ![alt](img)
                const std::string_view trimmedLabel = trimSpaces(link->label);
                if (const auto inner = parseBracketLink(trimmedLabel, 0);
                    inner && inner->isImage && inner->endPos == trimmedLabel.size())
                {
                    auto cleanLabel = std::string(trimSpaces(inner->label));
                    if (cleanLabel.empty()) cleanLabel = std::string(link->url);
                    parsed.push_back(MarkdownBadge{
                        .label = std::move(cleanLabel),
                        .href = std::string(link->url),
                        .color = badgeColorFromImageUrl(inner->url)
                    });
                    i = link->endPos;
                    continue;
                }

                return std::nullopt;
            }

            if (parsed.empty()) return std::nullopt;
            return parsed;
        }

        [[nodiscard]] std::string stripMarkupToPlainText(const std::string_view text)
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
                    if (const auto gt = text.find('>', i + 1); gt != std::string_view::npos && gt - i < 80)
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
                if (c == '[' || (c == '!' && i + 1 < n && text[i + 1] == '['))
                {
                    if (const auto link = parseBracketLink(text, i))
                    {
                        out.append(stripMarkupToPlainText(link->label));
                        i = link->endPos;
                        continue;
                    }
                }
                out.push_back(c);
                ++i;
            }
            return out;
        }

        struct HeadingInfo
        {
            int level = 1;
            std::string_view title;
        };

        [[nodiscard]] std::optional<HeadingInfo> tryExtractHeading(const std::string_view line) noexcept
        {
            const auto [leadIdx, leadingCols] = detail::scanLeadingIndent(line);
            if (leadingCols >= 4) return std::nullopt;

            const std::string_view trimmed = trimSpaces(line.substr(leadIdx));
            const auto hashes = detail::matchAtxHeadingHashes(trimmed);
            if (!hashes) return std::nullopt;

            std::string_view rest = trimSpaces(trimmed.substr(*hashes));
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

            return HeadingInfo{
                .level = static_cast<int>(*hashes),
                .title = rest
            };
        }

        [[nodiscard]] bool isThematicBreak(const std::string_view line) noexcept
        {
            return detail::isThematicBreakLine(trimSpaces(line));
        }

        struct FenceOpenInfo
        {
            char fenceChar = '`';
            std::size_t fenceLen = 0;
            std::string langTag;
        };

        [[nodiscard]] std::optional<FenceOpenInfo> tryExtractFenceOpen(const std::string_view line) noexcept
        {
            const auto match = detail::matchFenceOpen(line);
            if (!match) return std::nullopt;

            const std::string_view info = trimSpaces(match->infoRaw);
            std::size_t tagEnd = 0;
            while (tagEnd < info.size() && !mdIsSpace(info[tagEnd])) ++tagEnd;

            return FenceOpenInfo{
                .fenceChar = match->fenceChar,
                .fenceLen = match->fenceLen,
                .langTag = std::string(info.substr(0, tagEnd))
            };
        }

        [[nodiscard]] bool isFenceClose(const std::string_view line,
                                        const char fenceChar,
                                        const std::size_t minLen) noexcept
        {
            return detail::matchFenceClose(line, fenceChar, minLen).has_value();
        }

        struct ListItemInfo
        {
            int indentLevel = 0;
            bool ordered = false;
            std::string marker;
            bool isTask = false;
            bool taskChecked = false;
            std::string_view text;
        };

        [[nodiscard]] std::optional<ListItemInfo> tryExtractListItem(const std::string_view line) noexcept
        {
            std::size_t wsCols = 0;
            std::size_t i = 0;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
            {
                wsCols += line[i] == '\t' ? 4 : 1;
                ++i;
            }

            const std::string_view s = line.substr(i);
            if (s.empty()) return std::nullopt;

            bool ordered;
            std::string marker;
            std::size_t afterMarker;

            if (s.size() >= 2 && (s[0] == '-' || s[0] == '*' || s[0] == '+') && (s[1] == ' ' || s[1] == '\t'))
            {
                if (isThematicBreak(s)) return std::nullopt;
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
                    return std::nullopt;
                }
            }
            else
            {
                return std::nullopt;
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
                taskChecked = rest[1] == 'x' || rest[1] == 'X';
                rest.remove_prefix(3);
                while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t'))
                {
                    rest.remove_prefix(1);
                }
            }

            return ListItemInfo{
                .indentLevel = static_cast<int>(wsCols / 2),
                .ordered = ordered,
                .marker = std::move(marker),
                .isTask = isTask,
                .taskChecked = taskChecked,
                .text = rest
            };
        }

        [[nodiscard]] std::vector<std::string_view> splitTableCells(const std::string_view row)
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

        [[nodiscard]] std::optional<std::vector<std::string>> tryParseTableSeparator(const std::string_view row)
        {
            const std::string_view trimmed = trimSpaces(row);
            if (trimmed.find('|') == std::string_view::npos && trimmed.find('-') == std::string_view::npos)
            {
                return std::nullopt;
            }
            const auto cells = splitTableCells(trimmed);
            if (cells.empty()) return std::nullopt;

            std::vector<std::string> aligns;
            aligns.reserve(cells.size());
            for (const std::string_view cell : cells)
            {
                if (cell.empty()) return std::nullopt;
                const bool leftColon = cell.front() == ':';
                const bool rightColon = cell.back() == ':';
                const std::size_t start = leftColon ? 1 : 0;
                const std::size_t end = rightColon ? cell.size() - 1 : cell.size();
                if (start >= end) return std::nullopt;

                int dashes = 0;
                for (std::size_t i = start; i < end; ++i)
                {
                    if (cell[i] != '-') return std::nullopt;
                    ++dashes;
                }
                if (dashes < 1) return std::nullopt;

                if (leftColon && rightColon) aligns.emplace_back("center");
                else if (rightColon) aligns.emplace_back("right");
                else aligns.emplace_back("left");
            }

            return aligns;
        }

        [[nodiscard]] bool isHtmlWrapperOnlyLine(const std::string_view trimmed,
                                                 bool& ioCentered,
                                                 std::string& outExtractedSummary)
        {
            outExtractedSummary.clear();

            if (trimmed.empty() || trimmed.front() != '<') return false;
            const std::string lower = toLowerCopy(trimmed);

            if (lower.starts_with("<div") && lower.find(R"(align="center")") != std::string::npos)
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

        [[nodiscard]] std::string formatInlineCodeHtml(const std::string_view code)
        {
            std::string html =
                R"(<span style="background-color:#26303D;color:#A5D6FF;font-family:'Cascadia Mono','Consolas','JetBrains Mono','Courier New',monospace;">)";
            html.append("&nbsp;");
            html.append(escapeInlineText(code));
            html.append("&nbsp;");
            html.append("</span>");
            return html;
        }
    } // namespace

    std::string MarkdownParser::toGithubSlug(const std::string_view headingText) noexcept
    {
        const std::string plain = stripMarkupToPlainText(headingText);
        const std::string_view sv = trimSpaces(plain);

        std::string slug;
        slug.reserve(sv.size());

        std::size_t i = 0;
        const std::size_t n = sv.size();
        bool started = false;
        while (i < n)
        {
            if (const auto uc = static_cast<unsigned char>(sv[i]); uc >= 0x80)
            {
                const std::size_t seqLen = utf8SeqLen(uc);
                const std::size_t clampedLen = std::min(seqLen, n - i);
                if (clampedLen == 2)
                {
                    // Keep 2-byte UTF-8 letters (e.g. accented Latin characters)
                    const unsigned char b1 = uc;
                    if (const unsigned char b2 = i + 1 < n ? static_cast<unsigned char>(sv[i + 1]) : 0;
                        b1 == 0xC3 && b2 >= 0x80 && b2 <= 0x9E && b2 != 0x97)
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

            if (const char c = sv[i]; (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
            {
                slug.push_back(c);
                started = true;
            }
            else if (c >= 'A' && c <= 'Z')
            {
                slug.push_back(core::toLowerAsciiChar(c));
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

    std::string MarkdownParser::renderInlineHtml(const std::string_view text) noexcept
    {
        std::string out;
        out.reserve(text.size() * 2 + 16);

        const std::size_t n = text.size();
        std::size_t i = 0;

        while (i < n)
        {
            const auto uc = static_cast<unsigned char>(text[i]);
            const char c = text[i];

            // Preserve full UTF-8 multi-byte sequences intact
            if (uc >= 0x80)
            {
                const std::size_t start = i;
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
                if (const auto span = detail::scanInlineCodeSpan(text, i); span.found)
                {
                    out.append(formatInlineCodeHtml(text.substr(span.contentStart, span.contentEnd - span.contentStart)));
                    i = span.closeEnd;
                }
                else
                {
                    out.append(text.substr(i, span.tickLen));
                    i = span.contentStart;
                }
                continue;
            }

            // Simple inline HTML tags
            if (c == '<')
            {
                const std::string_view sub = text.substr(i);
                if (sub.starts_with("<!--"))
                {
                    if (const auto endPos = text.find("-->", i + 4); endPos != std::string_view::npos)
                    {
                        i = endPos + 3;
                    }
                    else
                    {
                        break;
                    }
                    continue;
                }
                if (i + 1 < n && (sub[1] == 'h' || sub[1] == 'H'))
                {
                    if (const auto gt = text.find('>', i + 1);
                        gt != std::string_view::npos &&
                        (sub.substr(1).starts_with("http://") || sub.substr(1).starts_with("https://")))
                    {
                        const std::string_view url = text.substr(i + 1, gt - (i + 1));
                        out.append("<a href=\"");
                        out.append(escapeHrefAttr(url));
                        out.append(R"(" style="color:#58A6FF;text-decoration:none;">)");
                        out.append(escapeInlineText(url));
                        out.append("</a>");
                        i = gt + 1;
                        continue;
                    }
                }

                if (const auto gt = text.find('>', i + 1); gt != std::string_view::npos && gt - i < 120)
                {
                    const std::string tagLower = toLowerCopy(text.substr(i, gt - i + 1));
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
                        out.append(
                            R"(<span style="background-color:#26303D;color:#A5D6FF;font-family:'Cascadia Mono','Consolas','JetBrains Mono','Courier New',monospace;">&nbsp;)");
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
                const std::size_t start = i;
                i = detail::scanBareHttpUrlEnd(text, i, false);
                const std::string_view url = text.substr(start, i - start);
                out.append("<a href=\"");
                out.append(escapeHrefAttr(url));
                out.append(R"(" style="color:#58A6FF;text-decoration:none;">)");
                out.append(escapeInlineText(url));
                out.append("</a>");
                continue;
            }

            // Markdown link or image: [label](url) / ![alt](url)
            if (c == '[' || (c == '!' && i + 1 < n && text[i + 1] == '['))
            {
                if (const auto link = parseBracketLink(text, i))
                {
                    if (link->isImage)
                    {
                        out.append(R"(<span style="background-color:#1F6FEB;color:#FFFFFF;font-weight:bold;">&nbsp;)");
                        out.append(escapeInlineText(link->label.empty() ? std::string_view("image") : link->label));
                        out.append("&nbsp;</span>");
                    }
                    else
                    {
                        out.append("<a href=\"");
                        out.append(escapeHrefAttr(link->url));
                        out.append(R"(" style="color:#58A6FF;text-decoration:none;">)");
                        out.append(renderInlineHtml(link->label));
                        out.append("</a>");
                    }
                    i = link->endPos;
                    continue;
                }
            }

            // Bold **...** or __...__
            if ((c == '*' || c == '_') && i + 1 < n && text[i + 1] == c && i + 2 < n && !mdIsSpace(text[i + 2]))
            {
                const char delim[3] = {c, c, '\0'};
                if (const auto closePos = text.find(delim, i + 2);
                    closePos != std::string_view::npos && closePos > i + 2 && !mdIsSpace(text[closePos - 1]))
                {
                    out.append(R"(<b style="color:#F0F6FC;">)");
                    out.append(renderInlineHtml(text.substr(i + 2, closePos - (i + 2))));
                    out.append("</b>");
                    i = closePos + 2;
                    continue;
                }
            }

            // Italic *...*
            if (c == '*' && !(i + 1 < n && text[i + 1] == '*') && i + 1 < n && !mdIsSpace(text[i + 1]))
            {
                if (const auto closePos = detail::findClosingItalicAsterisk(text, i);
                    closePos != std::string_view::npos)
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
                if (const auto closePos = text.find("~~", i + 2);
                    closePos != std::string_view::npos && closePos > i + 2)
                {
                    out.append(R"(<s style="color:#8B949E;">)");
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

    namespace
    {
        [[nodiscard]] std::string_view stripFourSpaceIndent(const std::string_view line) noexcept
        {
            if (line.starts_with('\t')) return line.substr(1);
            std::size_t s = 0;
            while (s < 4 && s < line.size() && line[s] == ' ') ++s;
            return line.substr(s);
        }

        [[nodiscard]] MarkdownBlock parseFencedCodeBlock(const std::vector<LineSlice>& lines,
                                                         std::size_t& idx,
                                                         const FenceOpenInfo& fence) noexcept
        {
            const std::size_t n = lines.size();
            const int startLine = lines[idx].lineNumber;
            std::string rawCode;
            ++idx;
            while (idx < n && !isFenceClose(lines[idx].text, fence.fenceChar, fence.fenceLen))
            {
                if (!rawCode.empty()) rawCode.push_back('\n');
                rawCode.append(lines[idx].text);
                ++idx;
            }
            const int endLine = idx < n ? lines[idx].lineNumber : lines.back().lineNumber;
            if (idx < n) ++idx;

            const Language detectedLang = LanguageDetector::fromCodeFenceInfo(fence.langTag);
            const std::string lowerTag = toLowerCopy(fence.langTag);

            MarkdownBlock b;
            b.kind = MarkdownBlockKind::CodeBlock;
            b.centered = false;
            b.isDiagram = (lowerTag == "mermaid" || lowerTag == "plantuml" || lowerTag == "dot");
            b.languageTag = fence.langTag.empty() ? "text" : fence.langTag;
            b.detectedLanguage = detectedLang;
            b.rawText = rawCode;
            b.html = SyntaxHighlighter::highlightCodeBlockHtml(rawCode, detectedLang);
            b.startLine = startLine;
            b.endLine = endLine;
            return b;
        }

        [[nodiscard]] MarkdownBlock parseIndentedCodeBlock(const std::vector<LineSlice>& lines,
                                                           std::size_t& idx) noexcept
        {
            const std::size_t n = lines.size();
            const int startLine = lines[idx].lineNumber;
            int endLine = startLine;
            std::string rawCode;

            while (idx < n)
            {
                const std::string_view rawLine = lines[idx].text;
                if (trimSpaces(rawLine).empty())
                {
                    std::size_t peek = idx + 1;
                    while (peek < n && trimSpaces(lines[peek].text).empty()) ++peek;
                    const bool peekIsNestedList = peek < n && tryExtractListItem(lines[peek].text).has_value();
                    if (peek < n && !peekIsNestedList &&
                        (lines[peek].text.starts_with("    ") || lines[peek].text.starts_with('\t')))
                    {
                        rawCode.push_back('\n');
                        ++idx;
                        continue;
                    }
                    break;
                }
                if (!rawLine.starts_with("    ") && !rawLine.starts_with('\t')) break;
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
            return b;
        }

        using InlineHtmlFn = std::string (*)(std::string_view) noexcept;

        void appendHeadingBlock(const LineSlice& line,
                                const HeadingInfo& heading,
                                const bool centeredBlock,
                                const InlineHtmlFn renderInline,
                                std::unordered_map<std::string, int>& slugCounts,
                                MarkdownDocument& doc) noexcept
        {
            auto plainTitle = std::string(trimSpaces(stripMarkupToPlainText(heading.title)));
            std::string baseSlug = MarkdownParser::toGithubSlug(heading.title);
            if (baseSlug.empty()) baseSlug = "section";

            int& count = slugCounts[baseSlug];
            std::string uniqueSlug = count == 0 ? baseSlug : baseSlug + "-" + std::to_string(count);
            ++count;

            MarkdownBlock b;
            b.kind = MarkdownBlockKind::Heading;
            b.level = heading.level;
            b.centered = centeredBlock;
            b.rawText = plainTitle;
            b.html = renderInline(heading.title);
            b.anchor = uniqueSlug;
            b.startLine = line.lineNumber;
            b.endLine = line.lineNumber;

            const auto blockIndex = static_cast<int>(doc.blocks.size());
            doc.blocks.push_back(std::move(b));
            doc.toc.push_back(MarkdownTocEntry{
                .level = heading.level,
                .title = std::move(plainTitle),
                .anchor = std::move(uniqueSlug),
                .blockIndex = blockIndex,
                .startLine = line.lineNumber
            });
        }

        [[nodiscard]] MarkdownBlock parseBadgeRowBlock(const std::vector<LineSlice>& lines,
                                                       std::size_t& idx,
                                                       std::vector<MarkdownBadge> initialBadges,
                                                       const bool centeredBlock) noexcept
        {
            const std::size_t n = lines.size();
            const int startLine = lines[idx].lineNumber;
            int endLine = startLine;
            ++idx;
            while (idx < n)
            {
                const std::string_view nextTrim = trimSpaces(lines[idx].text);
                if (nextTrim.empty()) break;
                auto nextBadges = tryParseBadgeLine(nextTrim);
                if (!nextBadges) break;
                initialBadges.insert(initialBadges.end(),
                                     std::make_move_iterator(nextBadges->begin()),
                                     std::make_move_iterator(nextBadges->end()));
                endLine = lines[idx].lineNumber;
                ++idx;
            }
            MarkdownBlock b;
            b.kind = MarkdownBlockKind::BadgeRow;
            b.centered = centeredBlock;
            b.badges = std::move(initialBadges);
            b.startLine = startLine;
            b.endLine = endLine;
            return b;
        }

        [[nodiscard]] MarkdownAlertType matchBlockquoteAlertPrefix(std::string_view& inner, bool& matched) noexcept
        {
            struct AlertDef
            {
                std::string_view tag;
                MarkdownAlertType type;
            };
            constexpr AlertDef kAlerts[] = {
                {.tag = "[!NOTE]", .type = MarkdownAlertType::Note},
                {.tag = "[!TIP]", .type = MarkdownAlertType::Tip},
                {.tag = "[!IMPORTANT]", .type = MarkdownAlertType::Important},
                {.tag = "[!WARNING]", .type = MarkdownAlertType::Warning},
                {.tag = "[!CAUTION]", .type = MarkdownAlertType::Caution}
            };
            matched = false;
            for (const auto& [tag, type] : kAlerts)
            {
                if (inner.starts_with(tag))
                {
                    inner = trimSpaces(inner.substr(tag.size()));
                    matched = true;
                    return type;
                }
            }
            return MarkdownAlertType::None;
        }

        [[nodiscard]] MarkdownBlock parseBlockquoteBlock(const std::vector<LineSlice>& lines,
                                                         std::size_t& idx,
                                                         const bool centeredBlock,
                                                         const InlineHtmlFn renderInline) noexcept
        {
            const std::size_t n = lines.size();
            const int startLine = lines[idx].lineNumber;
            int endLine = startLine;
            MarkdownAlertType alertType = MarkdownAlertType::None;
            std::string bodyText;

            bool firstLine = true;
            while (idx < n)
            {
                const std::string_view qt = trimSpaces(lines[idx].text);
                if (qt.empty() || qt[0] != '>') break;
                endLine = lines[idx].lineNumber;

                std::size_t skip = 1;
                if (skip < qt.size() && qt[skip] == ' ') ++skip;
                std::string_view inner = trimSpaces(qt.substr(skip));

                if (firstLine)
                {
                    firstLine = false;
                    bool matchedAlert = false;
                    alertType = matchBlockquoteAlertPrefix(inner, matchedAlert);
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
            b.html = renderInline(bodyText);
            b.startLine = startLine;
            b.endLine = endLine;
            return b;
        }

        [[nodiscard]] MarkdownBlock parseTableBlock(const std::vector<LineSlice>& lines,
                                                    std::size_t& idx,
                                                    std::vector<std::string> alignments,
                                                    const InlineHtmlFn renderInline) noexcept
        {
            const std::size_t n = lines.size();
            const int startLine = lines[idx].lineNumber;
            int endLine = lines[idx + 1].lineNumber;

            const auto rawHeaders = splitTableCells(lines[idx].text);
            const std::size_t colCount = std::max(rawHeaders.size(), alignments.size());
            alignments.resize(colCount, "left");

            std::vector<std::string> headersHtml;
            headersHtml.reserve(colCount);
            for (std::size_t c = 0; c < colCount; ++c)
            {
                headersHtml.push_back(c < rawHeaders.size() ? renderInline(rawHeaders[c]) : "");
            }

            idx += 2;
            std::vector<std::vector<std::string>> rowsHtml;
            while (idx < n)
            {
                const std::string_view rowTrim = trimSpaces(lines[idx].text);
                if (rowTrim.empty() || rowTrim.find('|') == std::string_view::npos) break;
                if (rowTrim[0] == '>' || isThematicBreak(rowTrim)) break;

                const auto cells = splitTableCells(rowTrim);
                std::vector<std::string> rowOut;
                rowOut.reserve(colCount);
                for (std::size_t c = 0; c < colCount; ++c)
                {
                    rowOut.push_back(c < cells.size() ? renderInline(cells[c]) : "");
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
            return b;
        }

        [[nodiscard]] MarkdownBlock parseListItemBlock(const std::vector<LineSlice>& lines,
                                                       std::size_t& idx,
                                                       ListItemInfo listItem,
                                                       const InlineHtmlFn renderInline) noexcept
        {
            const std::size_t n = lines.size();
            const int startLine = lines[idx].lineNumber;
            int endLine = startLine;
            std::string combinedText(listItem.text);
            ++idx;

            while (idx < n)
            {
                const std::string_view nextRaw = lines[idx].text;
                const std::string_view nextTrim = trimSpaces(nextRaw);
                if (nextTrim.empty()) break;
                if (tryExtractListItem(nextRaw) || tryExtractHeading(nextRaw) ||
                    tryExtractFenceOpen(nextRaw) || isThematicBreak(nextRaw) || nextTrim[0] == '>')
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
            b.indentLevel = listItem.indentLevel;
            b.ordered = listItem.ordered;
            b.listMarker = std::move(listItem.marker);
            b.isTask = listItem.isTask;
            b.taskChecked = listItem.taskChecked;
            b.rawText = combinedText;
            b.html = renderInline(combinedText);
            b.startLine = startLine;
            b.endLine = endLine;
            return b;
        }

        [[nodiscard]] MarkdownBlock parseParagraphBlock(const std::vector<LineSlice>& lines,
                                                        std::size_t& idx,
                                                        const bool centeredBlock,
                                                        const InlineHtmlFn renderInline) noexcept
        {
            const std::size_t n = lines.size();
            const int startLine = lines[idx].lineNumber;
            int endLine = startLine;
            std::string paraText(trimSpaces(lines[idx].text));
            ++idx;

            while (idx < n)
            {
                const std::string_view nextRaw = lines[idx].text;
                const std::string_view nextTrim = trimSpaces(nextRaw);
                if (nextTrim.empty()) break;

                bool dummyCenter = centeredBlock;
                if (std::string tmpSum; isHtmlWrapperOnlyLine(nextTrim, dummyCenter, tmpSum) || !tmpSum.empty())
                {
                    break;
                }
                if (tryExtractHeading(nextRaw) || tryExtractFenceOpen(nextRaw) ||
                    isThematicBreak(nextRaw) || nextTrim[0] == '>' ||
                    tryParseBadgeLine(nextTrim) || tryExtractListItem(nextRaw))
                {
                    break;
                }
                if (idx + 1 < n && nextTrim.find('|') != std::string_view::npos &&
                    tryParseTableSeparator(lines[idx + 1].text))
                {
                    break;
                }

                if (!paraText.empty())
                {
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
            b.html = renderInline(paraText);
            b.startLine = startLine;
            b.endLine = endLine;
            return b;
        }

        [[nodiscard]] MarkdownBlock makeDeletedSnippetBlock(std::vector<std::string> deletedLines,
                                                            const int lineNumber) noexcept
        {
            MarkdownBlock delSnippet;
            delSnippet.kind = MarkdownBlockKind::DeletedSnippet;
            delSnippet.changeStatus = MarkdownChangeStatus::Deleted;
            delSnippet.deletedLineCount = static_cast<int>(deletedLines.size());
            delSnippet.deletedLines = std::move(deletedLines);
            delSnippet.startLine = lineNumber;
            delSnippet.endLine = lineNumber;
            return delSnippet;
        }

        void collectDiffChanges(const git::bo::FileDiff& diff,
                                std::set<int>& addedLines,
                                std::map<int, std::vector<std::string>>& deletionsBeforeNewLine,
                                std::vector<std::string>& trailingDeletions) noexcept
        {
            for (const auto& hunk : diff.hunks)
            {
                std::vector<std::string> pendingDels;
                for (const auto& line : hunk.lines)
                {
                    if (line.kind == git::bo::DiffLineKind::Deletion)
                    {
                        pendingDels.push_back(line.content);
                    }
                    else if (line.kind == git::bo::DiffLineKind::Addition)
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
                    else if (line.kind == git::bo::DiffLineKind::Context &&
                             !pendingDels.empty() && line.newLineNumber.has_value())
                    {
                        auto& vec = deletionsBeforeNewLine[*line.newLineNumber];
                        vec.insert(vec.end(), pendingDels.begin(), pendingDels.end());
                        pendingDels.clear();
                    }
                }
                if (!pendingDels.empty())
                {
                    trailingDeletions.insert(trailingDeletions.end(), pendingDels.begin(), pendingDels.end());
                }
            }
        }
    } // namespace

    MarkdownDocument MarkdownParser::parse(const std::string_view markdownText) noexcept
    {
        MarkdownDocument doc;
        const std::string normalized = normalizeLineEndings(markdownText);
        const auto lines = splitNormalizedLines(normalized);
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
            const auto& [text, lineNumber] = lines[idx];
            const std::string_view trimmed = trimSpaces(text);

            if (trimmed.empty())
            {
                ++idx;
                continue;
            }

            if (trimmed.starts_with("<!--") && trimmed.find("-->") == std::string_view::npos)
            {
                ++idx;
                while (idx < n && lines[idx].text.find("-->") == std::string_view::npos) ++idx;
                if (idx < n) ++idx;
                continue;
            }

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
                b.html = R"(<b style="color:#79C0FF;">&#9654; )" + renderInlineHtml(summaryInner) + "</b>";
                b.startLine = lineNumber;
                b.endLine = lineNumber;
                doc.blocks.push_back(std::move(b));
                ++idx;
                continue;
            }

            if (const auto fence = tryExtractFenceOpen(text))
            {
                doc.blocks.push_back(parseFencedCodeBlock(lines, idx, *fence));
                continue;
            }

            if (!centeredBlock && (text.starts_with("    ") || text.starts_with('\t')) && !tryExtractListItem(text))
            {
                doc.blocks.push_back(parseIndentedCodeBlock(lines, idx));
                continue;
            }

            if (const auto heading = tryExtractHeading(text))
            {
                appendHeadingBlock(lines[idx], *heading, centeredBlock, &MarkdownParser::renderInlineHtml, slugCounts, doc);
                ++idx;
                continue;
            }

            if (isThematicBreak(text))
            {
                MarkdownBlock b;
                b.kind = MarkdownBlockKind::ThematicBreak;
                b.startLine = lineNumber;
                b.endLine = lineNumber;
                doc.blocks.push_back(std::move(b));
                ++idx;
                continue;
            }

            if (auto badges = tryParseBadgeLine(trimmed))
            {
                doc.blocks.push_back(parseBadgeRowBlock(lines, idx, std::move(*badges), centeredBlock));
                continue;
            }

            if (trimmed[0] == '>')
            {
                doc.blocks.push_back(parseBlockquoteBlock(lines, idx, centeredBlock, &MarkdownParser::renderInlineHtml));
                continue;
            }

            if (idx + 1 < n && trimmed.find('|') != std::string_view::npos)
            {
                if (auto alignments = tryParseTableSeparator(lines[idx + 1].text))
                {
                    doc.blocks.push_back(parseTableBlock(lines, idx, std::move(*alignments), &MarkdownParser::renderInlineHtml));
                    continue;
                }
            }

            if (auto listItem = tryExtractListItem(text))
            {
                doc.blocks.push_back(parseListItemBlock(lines, idx, std::move(*listItem), &MarkdownParser::renderInlineHtml));
                continue;
            }

            doc.blocks.push_back(parseParagraphBlock(lines, idx, centeredBlock, &MarkdownParser::renderInlineHtml));
        }

        return doc;
    }

    MarkdownDocument MarkdownParser::parseWithDiff(const std::string_view markdownText,
                                                   const git::bo::FileDiff* diff) noexcept
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
        collectDiffChanges(*diff, addedLines, deletionsBeforeNewLine, trailingDeletions);

        std::vector<MarkdownBlock> finalBlocks;
        finalBlocks.reserve(doc.blocks.size() + 4);
        std::unordered_map<int, int> oldToNewBlockIndex;
        auto delIt = deletionsBeforeNewLine.begin();

        for (std::size_t bIdx = 0; bIdx < doc.blocks.size(); ++bIdx)
        {
            MarkdownBlock block = doc.blocks[bIdx];

            std::vector<std::string> pureBeforeDels;
            while (delIt != deletionsBeforeNewLine.end() && delIt->first < block.startLine)
            {
                pureBeforeDels.insert(pureBeforeDels.end(), delIt->second.begin(), delIt->second.end());
                ++delIt;
            }
            if (!pureBeforeDels.empty())
            {
                finalBlocks.push_back(makeDeletedSnippetBlock(std::move(pureBeforeDels), block.startLine));
            }

            int addedInBlock = 0;
            for (int lineNum = block.startLine; lineNum <= block.endLine; ++lineNum)
            {
                if (addedLines.contains(lineNum)) ++addedInBlock;
            }

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
                finalBlocks.push_back(makeDeletedSnippetBlock(std::move(blockDels), block.startLine));
            }

            oldToNewBlockIndex[static_cast<int>(bIdx)] = static_cast<int>(finalBlocks.size());
            finalBlocks.push_back(std::move(block));
        }

        std::vector<std::string> eofDels;
        while (delIt != deletionsBeforeNewLine.end())
        {
            eofDels.insert(eofDels.end(), delIt->second.begin(), delIt->second.end());
            ++delIt;
        }
        eofDels.insert(eofDels.end(), trailingDeletions.begin(), trailingDeletions.end());
        if (!eofDels.empty())
        {
            const int trailingLine = doc.blocks.empty() ? 1 : doc.blocks.back().endLine + 1;
            finalBlocks.push_back(makeDeletedSnippetBlock(std::move(eofDels), trailingLine));
        }

        for (auto& tocEntry : doc.toc)
        {
            if (const auto it = oldToNewBlockIndex.find(tocEntry.blockIndex); it != oldToNewBlockIndex.end())
            {
                tocEntry.blockIndex = it->second;
            }
        }

        doc.blocks = std::move(finalBlocks);
        return doc;
    }
} // namespace gf::core::syntax
