#pragma once

#include <string>
#include <string_view>

namespace gf::core
{
    [[nodiscard]] inline bool appendEscapedHtmlChar(std::string& out, const char c) noexcept
    {
        switch (c)
        {
        case '&': out.append("&amp;");
            return true;
        case '<': out.append("&lt;");
            return true;
        case '>': out.append("&gt;");
            return true;
        case '"': out.append("&quot;");
            return true;
        default:
            return false;
        }
    }

    [[nodiscard]] inline std::string escapeHtmlBasic(const std::string_view text) noexcept
    {
        std::string out;
        out.reserve(text.size() + 8);
        for (const char c : text)
        {
            if (!appendEscapedHtmlChar(out, c))
            {
                out.push_back(c);
            }
        }
        return out;
    }

    [[nodiscard]] inline std::string escapeHtmlStrictSpaces(const std::string_view text) noexcept
    {
        std::string out;
        out.reserve(text.size() * 11 / 10);
        for (const char c : text)
        {
            if (appendEscapedHtmlChar(out, c)) continue;
            if (c == ' ') out.append("&nbsp;");
            else if (c == '\t') out.append("&nbsp;&nbsp;&nbsp;&nbsp;");
            else out.push_back(c);
        }
        return out;
    }

    [[nodiscard]] inline std::string escapeHtmlForPre(const std::string_view text) noexcept
    {
        std::string out;
        out.reserve(text.size() * 11 / 10);
        for (const char c : text)
        {
            if (appendEscapedHtmlChar(out, c)) continue;
            if (c == '\t') out.append("    ");
            else out.push_back(c);
        }
        return out;
    }

    [[nodiscard]] inline std::string escapeHtmlWrapFriendly(const std::string_view text,
                                                              const bool atLineStart) noexcept
    {
        std::string out;
        out.reserve(text.size() * 11 / 10);
        std::size_t i = 0;
        const std::size_t n = text.size();
        while (i < n)
        {
            const char c = text[i];
            if (appendEscapedHtmlChar(out, c))
            {
                ++i;
                continue;
            }
            if (c == ' ' || c == '\t')
            {
                const bool isLeading = atLineStart && i == 0;
                std::size_t runEnd = i;
                std::size_t visualSpaces = 0;
                while (runEnd < n && (text[runEnd] == ' ' || text[runEnd] == '\t'))
                {
                    visualSpaces += text[runEnd] == '\t' ? 4 : 1;
                    ++runEnd;
                }
                const std::size_t nbspCount = isLeading ? visualSpaces : (visualSpaces > 1 ? visualSpaces - 1 : 0);
                for (std::size_t k = 0; k < nbspCount; ++k)
                {
                    out.append("&nbsp;");
                }
                if (!isLeading)
                {
                    out.push_back(' ');
                }
                i = runEnd;
                continue;
            }
            out.push_back(c);
            ++i;
        }
        return out;
    }

    [[nodiscard]] inline std::string escapeHtmlPreservingSpaces(const std::string_view text)
    {
        std::string out;
        out.reserve(text.size() + 16);
        bool prevWasSpace = true; // preserve leading spaces as &nbsp;
        for (const char ch : text)
        {
            if (appendEscapedHtmlChar(out, ch))
            {
                prevWasSpace = false;
                continue;
            }
            if (ch == '\t')
            {
                out.append("&nbsp;&nbsp;&nbsp;&nbsp;");
                prevWasSpace = true;
            }
            else if (ch == ' ')
            {
                if (prevWasSpace)
                {
                    out.append("&nbsp;");
                }
                else
                {
                    out.push_back(' ');
                }
                prevWasSpace = true;
            }
            else
            {
                out.push_back(ch);
                prevWasSpace = false;
            }
        }
        return out;
    }
} // namespace gf::core
