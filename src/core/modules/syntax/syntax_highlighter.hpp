#pragma once

#include "core/modules/syntax/language_detector.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace gf::core::syntax
{
    enum class TokenType
    {
        PlainText,
        Whitespace,
        Keyword,
        Type,
        Function,
        Variable,
        Property,
        Constant,
        String,
        Number,
        Comment,
        Preprocessor,
        Operator,
        Punctuation,
        MarkdownHeading,
        MarkdownMarker,
        MarkdownBold,
        MarkdownItalic,
        MarkdownCode,
        MarkdownLink,
        MarkdownQuote
    };

    struct Token
    {
        TokenType type = TokenType::PlainText;
        std::string text;
    };

    struct MarkdownLineState
    {
        bool inCodeFence = false;
        char fenceChar = '`';
        std::size_t fenceLength = 0;
        Language fenceLanguage = Language::Unknown;

        bool operator==(const MarkdownLineState&) const = default;
    };

    class SyntaxHighlighter
    {
    public:
        [[nodiscard]] static std::vector<Token> tokenize(std::string_view line, Language lang) noexcept;
        [[nodiscard]] static std::vector<Token> tokenizeMarkdownLine(std::string_view line,
                                                                     MarkdownLineState& state) noexcept;

        [[nodiscard]] static std::string highlightLine(std::string_view line, Language lang) noexcept;
        [[nodiscard]] static std::string highlightLine(std::string_view line,
                                                       Language lang,
                                                       MarkdownLineState& mdState) noexcept;

        [[nodiscard]] static std::vector<MarkdownLineState>
        computeMarkdownLineStates(std::string_view content) noexcept;

        [[nodiscard]] static std::string highlightCodeBlockHtml(std::string_view codeText, Language lang) noexcept;

        [[nodiscard]] static std::string escapeHtml(std::string_view text) noexcept;
        [[nodiscard]] static std::string escapeHtmlWrapFriendly(std::string_view text, bool atLineStart) noexcept;
        [[nodiscard]] static std::string escapeHtmlForPre(std::string_view text) noexcept;

        [[nodiscard]] static std::string_view tokenColor(TokenType type) noexcept;
    };
} // namespace gf::core::syntax
