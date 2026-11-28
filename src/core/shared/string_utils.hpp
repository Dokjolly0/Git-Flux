#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace gf::core
{
    [[nodiscard]] constexpr bool isAsciiWhitespace(const char ch) noexcept
    {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
    }

    [[nodiscard]] constexpr std::string_view trim(std::string_view sv) noexcept
    {
        while (!sv.empty() && isAsciiWhitespace(sv.front()))
        {
            sv.remove_prefix(1);
        }
        while (!sv.empty() && isAsciiWhitespace(sv.back()))
        {
            sv.remove_suffix(1);
        }
        return sv;
    }

    [[nodiscard]] constexpr std::string_view trimRight(std::string_view sv) noexcept
    {
        while (!sv.empty() && isAsciiWhitespace(sv.back()))
        {
            sv.remove_suffix(1);
        }
        return sv;
    }

    [[nodiscard]] constexpr std::string_view trimTrailingNewlines(std::string_view sv) noexcept
    {
        while (!sv.empty() && (sv.back() == '\r' || sv.back() == '\n'))
        {
            sv.remove_suffix(1);
        }
        return sv;
    }

    [[nodiscard]] constexpr char toLowerAsciiChar(const char ch) noexcept
    {
        return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
    }

    [[nodiscard]] inline std::string toLowerAscii(const std::string_view sv)
    {
        std::string out;
        out.reserve(sv.size());
        for (const char ch : sv)
        {
            out.push_back(toLowerAsciiChar(ch));
        }
        return out;
    }

    inline void stripUtf8Bom(std::string& text) noexcept
    {
        constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";
        if (text.starts_with(kUtf8Bom))
        {
            text.erase(0, kUtf8Bom.size());
        }
    }
} // namespace gf::core
