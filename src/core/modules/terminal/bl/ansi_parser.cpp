#include "core/modules/terminal/bl/ansi_parser.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <vector>

namespace gf::terminal::bl
{
    namespace
    {
        // Modern high-contrast dark terminal ANSI 16-color palette (harmonized with FluxTheme)
        constexpr std::array<const char*, 16> kAnsi16Palette = {
            "#1E293B", // 0: Black (Slate 800)
            "#F87171", // 1: Red (Red 400)
            "#34D399", // 2: Green (Emerald 400)
            "#FBBF24", // 3: Yellow (Amber 400)
            "#60A5FA", // 4: Blue (Blue 400)
            "#C084FC", // 5: Magenta (Purple 400)
            "#22D3EE", // 6: Cyan (Cyan 400)
            "#E2E8F0", // 7: White (Slate 200)
            "#64748B", // 8: Bright Black (Slate 500)
            "#FCA5A5", // 9: Bright Red
            "#6EE7B7", // 10: Bright Green
            "#FDE68A", // 11: Bright Yellow
            "#93C5FD", // 12: Bright Blue
            "#D8B4FE", // 13: Bright Magenta
            "#67E8F9", // 14: Bright Cyan
            "#F8FAFC"  // 15: Bright White
        };

        // Dedicated muted dark-theme ANSI background palette (prevents glaring neon highlighter blocks)
        constexpr std::array<const char*, 16> kAnsi16BgPalette = {
            "#0F172A", // 40: Black bg
            "#45141C", // 41: Red bg
            "#0B3B2B", // 42: Green bg
            "#422A07", // 43: Yellow bg
            "#122B4A", // 44: Blue bg (subtle deep navy)
            "#31184A", // 45: Magenta bg
            "#0C3342", // 46: Cyan bg
            "#334155", // 47: White bg
            "#1E293B", // 100: Bright Black bg
            "#5C1B26", // 101: Bright Red bg
            "#0F4F3A", // 102: Bright Green bg
            "#57380A", // 103: Bright Yellow bg
            "#1A3B63", // 104: Bright Blue bg
            "#422063", // 105: Bright Magenta bg
            "#114559", // 106: Bright Cyan bg
            "#475569"  // 107: Bright White bg
        };

        [[nodiscard]] std::string rgbToHex(const int r, const int g, const int b)
        {
            const int cr = std::clamp(r, 0, 255);
            const int cg = std::clamp(g, 0, 255);
            const int cb = std::clamp(b, 0, 255);
            char buf[8];
            std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", cr, cg, cb);
            return std::string(buf, 7);
        }

        [[nodiscard]] std::string xterm256ToHex(const int index)
        {
            if (index < 0 || index > 255)
            {
                return "#E2E8F0";
            }
            if (index < 16)
            {
                return kAnsi16Palette[static_cast<std::size_t>(index)];
            }
            if (index <= 231)
            {
                const int cube = index - 16;
                const int rIdx = cube / 36;
                const int gIdx = (cube / 6) % 6;
                const int bIdx = cube % 6;
                constexpr std::array<int, 6> levels = {0, 95, 135, 175, 215, 255};
                return rgbToHex(levels[static_cast<std::size_t>(rIdx)],
                                levels[static_cast<std::size_t>(gIdx)],
                                levels[static_cast<std::size_t>(bIdx)]);
            }
            const int gray = 8 + (index - 232) * 10;
            return rgbToHex(gray, gray, gray);
        }

        [[nodiscard]] std::string escapeHtmlPreservingSpaces(const std::string_view text)
        {
            std::string out;
            out.reserve(text.size() + 16);
            bool prevWasSpace = true; // preserve leading spaces as &nbsp;
            for (const char ch : text)
            {
                switch (ch)
                {
                case '&':
                    out.append("&amp;");
                    prevWasSpace = false;
                    break;
                case '<':
                    out.append("&lt;");
                    prevWasSpace = false;
                    break;
                case '>':
                    out.append("&gt;");
                    prevWasSpace = false;
                    break;
                case '"':
                    out.append("&quot;");
                    prevWasSpace = false;
                    break;
                case '\t':
                    out.append("&nbsp;&nbsp;&nbsp;&nbsp;");
                    prevWasSpace = true;
                    break;
                case ' ':
                    if (prevWasSpace)
                    {
                        out.append("&nbsp;");
                    }
                    else
                    {
                        out.push_back(' ');
                    }
                    prevWasSpace = true;
                    break;
                default:
                    out.push_back(ch);
                    prevWasSpace = false;
                    break;
                }
            }
            return out;
        }

        [[nodiscard]] std::string resolveCarriageReturns(const std::string_view rawLine)
        {
            if (rawLine.find('\r') == std::string_view::npos)
            {
                return std::string(rawLine);
            }

            // Split on '\r' and keep the last non-empty overwrite (standard progress bar behavior)
            std::string_view result;
            std::size_t start = 0;
            while (start <= rawLine.size())
            {
                const std::size_t cr = rawLine.find('\r', start);
                const std::string_view seg = (cr == std::string_view::npos)
                                                 ? rawLine.substr(start)
                                                 : rawLine.substr(start, cr - start);
                if (!seg.empty())
                {
                    result = seg;
                }
                if (cr == std::string_view::npos)
                {
                    break;
                }
                start = cr + 1;
            }
            return std::string(result);
        }

        void applySgrParams(const std::vector<int>& params, bo::TerminalTextStyle& style)
        {
            if (params.empty())
            {
                style = bo::TerminalTextStyle{};
                return;
            }

            for (std::size_t i = 0; i < params.size(); ++i)
            {
                const int code = params[i];
                if (code == 0)
                {
                    style = bo::TerminalTextStyle{};
                }
                else if (code == 1)
                {
                    style.bold = true;
                }
                else if (code == 2)
                {
                    style.dim = true;
                }
                else if (code == 3)
                {
                    style.italic = true;
                }
                else if (code == 4)
                {
                    style.underline = true;
                }
                else if (code == 22)
                {
                    style.bold = false;
                    style.dim = false;
                }
                else if (code == 23)
                {
                    style.italic = false;
                }
                else if (code == 24)
                {
                    style.underline = false;
                }
                else if (code >= 30 && code <= 37)
                {
                    style.fgColor = kAnsi16Palette[static_cast<std::size_t>(code - 30)];
                }
                else if (code == 39)
                {
                    style.fgColor.clear();
                }
                else if (code >= 40 && code <= 47)
                {
                    style.bgColor = kAnsi16BgPalette[static_cast<std::size_t>(code - 40)];
                }
                else if (code == 49)
                {
                    style.bgColor.clear();
                }
                else if (code >= 90 && code <= 97)
                {
                    style.fgColor = kAnsi16Palette[static_cast<std::size_t>(8 + (code - 90))];
                }
                else if (code >= 100 && code <= 107)
                {
                    style.bgColor = kAnsi16BgPalette[static_cast<std::size_t>(8 + (code - 100))];
                }
                else if (code == 38 || code == 48)
                {
                    const bool isFg = (code == 38);
                    if (i + 2 < params.size() && params[i + 1] == 5)
                    {
                        const std::string hex = xterm256ToHex(params[i + 2]);
                        if (isFg) style.fgColor = hex;
                        else style.bgColor = hex;
                        i += 2;
                    }
                    else if (i + 4 < params.size() && params[i + 1] == 2)
                    {
                        const std::string hex = rgbToHex(params[i + 2], params[i + 3], params[i + 4]);
                        if (isFg) style.fgColor = hex;
                        else style.bgColor = hex;
                        i += 4;
                    }
                }
            }
        }

        [[nodiscard]] std::vector<int> parseSgrNumbers(const std::string_view seq)
        {
            std::vector<int> params;
            if (seq.empty())
            {
                return params;
            }
            std::size_t pos = 0;
            while (pos <= seq.size())
            {
                const std::size_t sep = seq.find(';', pos);
                const std::string_view part = (sep == std::string_view::npos)
                                                  ? seq.substr(pos)
                                                  : seq.substr(pos, sep - pos);
                if (part.empty())
                {
                    params.push_back(0);
                }
                else
                {
                    int val = 0;
                    const auto [ptr, ec] = std::from_chars(part.data(), part.data() + part.size(), val);
                    if (ec == std::errc{} && ptr == part.data() + part.size())
                    {
                        params.push_back(val);
                    }
                }
                if (sep == std::string_view::npos)
                {
                    break;
                }
                pos = sep + 1;
            }
            return params;
        }

        void appendSpanHtml(std::string& out, const bo::TerminalStyledSpan& span)
        {
            if (span.text.empty())
            {
                return;
            }
            const std::string escaped = escapeHtmlPreservingSpaces(span.text);
            const auto& st = span.style;
            const bool hasStyle = !st.fgColor.empty() || !st.bgColor.empty() ||
                                  st.bold || st.dim || st.italic || st.underline;
            if (!hasStyle)
            {
                out.append(escaped);
                return;
            }

            out.append("<span style=\"");
            if (!st.fgColor.empty())
            {
                out.append("color:");
                out.append(st.fgColor);
                out.push_back(';');
            }
            else if (st.dim)
            {
                out.append("color:#64748B;");
            }
            if (!st.bgColor.empty())
            {
                out.append("background-color:");
                out.append(st.bgColor);
                out.push_back(';');
            }
            if (st.bold)
            {
                out.append("font-weight:700;");
            }
            if (st.italic)
            {
                out.append("font-style:italic;");
            }
            if (st.underline)
            {
                out.append("text-decoration:underline;");
            }
            out.append("\">");
            out.append(escaped);
            out.append("</span>");
        }
    } // namespace

    bo::TerminalRenderedLine AnsiParser::parseLine(const std::string_view rawLine,
                                                   bo::TerminalTextStyle& carryStyle)
    {
        const std::string resolved = resolveCarriageReturns(rawLine);
        const std::string_view line(resolved);

        bo::TerminalRenderedLine rendered;
        rendered.plainText.reserve(line.size());

        std::string currentText;
        currentText.reserve(line.size());

        auto flushSpan = [&]()
        {
            if (currentText.empty())
            {
                return;
            }
            rendered.plainText.append(currentText);
            if (!rendered.spans.empty() && rendered.spans.back().style == carryStyle)
            {
                rendered.spans.back().text.append(currentText);
            }
            else
            {
                rendered.spans.push_back(bo::TerminalStyledSpan{
                    .text = std::move(currentText),
                    .style = carryStyle
                });
            }
            currentText.clear();
        };

        std::size_t i = 0;
        while (i < line.size())
        {
            if (line[i] == '\x1b')
            {
                if (i + 1 < line.size() && line[i + 1] == '[')
                {
                    // CSI sequence: ESC [ <params> <final byte in 0x40..0x7E>
                    std::size_t j = i + 2;
                    while (j < line.size())
                    {
                        const unsigned char c = static_cast<unsigned char>(line[j]);
                        if (c >= 0x40 && c <= 0x7E)
                        {
                            break;
                        }
                        ++j;
                    }
                    if (j < line.size())
                    {
                        if (line[j] == 'm')
                        {
                            flushSpan();
                            const auto params = parseSgrNumbers(line.substr(i + 2, j - (i + 2)));
                            applySgrParams(params, carryStyle);
                        }
                        // Non-SGR CSI sequences (cursor moves, erase line, etc.) are silently stripped
                        i = j + 1;
                        continue;
                    }
                    // Truncated CSI sequence: stop consuming
                    break;
                }
                if (i + 1 < line.size() && line[i + 1] == ']')
                {
                    // OSC sequence: ESC ] ... (BEL or ESC \)
                    std::size_t j = i + 2;
                    while (j < line.size())
                    {
                        if (line[j] == '\x07')
                        {
                            ++j;
                            break;
                        }
                        if (line[j] == '\x1b' && j + 1 < line.size() && line[j + 1] == '\\')
                        {
                            j += 2;
                            break;
                        }
                        ++j;
                    }
                    i = j;
                    continue;
                }
                // Unknown 2-byte ESC sequence
                i += (i + 1 < line.size()) ? 2 : 1;
                continue;
            }

            // Skip raw control chars except tab
            const unsigned char ch = static_cast<unsigned char>(line[i]);
            if (ch < 0x20 && ch != '\t')
            {
                ++i;
                continue;
            }

            currentText.push_back(line[i]);
            ++i;
        }

        flushSpan();

        rendered.richHtml.reserve(rendered.plainText.size() + rendered.spans.size() * 32);
        for (const auto& span : rendered.spans)
        {
            appendSpanHtml(rendered.richHtml, span);
        }

        return rendered;
    }

    bo::TerminalRenderedLine AnsiParser::parseLine(const std::string_view rawLine)
    {
        bo::TerminalTextStyle style{};
        return parseLine(rawLine, style);
    }
} // namespace gf::terminal::bl
