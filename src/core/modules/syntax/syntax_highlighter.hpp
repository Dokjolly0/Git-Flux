#pragma once

#include "core/modules/syntax/language_detector.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace gf::core::syntax {

enum class TokenType {
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
    Punctuation
};

struct Token {
    TokenType type = TokenType::PlainText;
    std::string text;
};

class SyntaxHighlighter {
public:
    [[nodiscard]] static std::vector<Token> tokenize(std::string_view line, Language lang) noexcept;
    [[nodiscard]] static std::string highlightLine(std::string_view line, Language lang) noexcept;
    [[nodiscard]] static std::string escapeHtml(std::string_view text) noexcept;
    [[nodiscard]] static std::string_view tokenColor(TokenType type) noexcept;
};

} // namespace gf::core::syntax
