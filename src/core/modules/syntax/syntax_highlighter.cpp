#include "core/modules/syntax/syntax_highlighter.hpp"

#include <algorithm>
#include <string_view>
#include <unordered_set>

namespace gf::core::syntax {

namespace {

// ==============================================================================
// ASCII & UTF-8 SAFE HELPERS
// ==============================================================================

[[nodiscard]] constexpr bool asciiIsSpace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

[[nodiscard]] constexpr bool asciiIsDigit(char c) noexcept {
    return c >= '0' && c <= '9';
}

[[nodiscard]] constexpr bool asciiIsXDigit(char c) noexcept {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

[[nodiscard]] constexpr char asciiToLower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] constexpr std::size_t utf8SequenceLength(unsigned char leadByte) noexcept {
    if (leadByte < 0x80) return 1;
    if ((leadByte & 0xE0) == 0xC0) return 2;
    if ((leadByte & 0xF0) == 0xE0) return 3;
    if ((leadByte & 0xF8) == 0xF0) return 4;
    return 1;
}

[[nodiscard]] std::size_t clampUtf8Boundary(std::string_view text, std::size_t maxBytes) noexcept {
    if (text.size() <= maxBytes) {
        return text.size();
    }
    std::size_t end = maxBytes;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
        --end;
    }
    return end;
}

// ==============================================================================
// KEYWORD & TYPE DICTIONARIES
// ==============================================================================

const std::unordered_set<std::string_view> kCppKeywords = {
    "alignas", "alignof", "asm", "auto", "bool", "break", "case", "catch", "char",
    "char8_t", "char16_t", "char32_t", "class", "concept", "const", "consteval",
    "constexpr", "constinit", "const_cast", "continue", "co_await", "co_return",
    "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast",
    "else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend",
    "goto", "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
    "nullptr", "operator", "private", "protected", "public", "register", "reinterpret_cast",
    "requires", "return", "short", "signed", "sizeof", "static", "static_assert",
    "static_cast", "struct", "switch", "template", "this", "thread_local", "throw",
    "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
    "virtual", "void", "volatile", "wchar_t", "while"
};

const std::unordered_set<std::string_view> kCppBuiltinTypes = {
    "void", "bool", "char", "char8_t", "char16_t", "char32_t", "short", "int", "long",
    "float", "double", "signed", "unsigned", "wchar_t",
    "size_t", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
    "uint32_t", "uint64_t", "intptr_t", "uintptr_t", "ptrdiff_t", "string", "string_view",
    "vector", "map", "unordered_map", "set", "unordered_set", "array", "deque",
    "list", "pair", "tuple", "optional", "variant", "any", "unique_ptr", "shared_ptr",
    "weak_ptr", "function", "span"
};

const std::unordered_set<std::string_view> kJsTsKeywords = {
    "abstract", "any", "as", "asserts", "async", "await", "boolean", "break", "case",
    "catch", "class", "const", "constructor", "continue", "debugger", "declare",
    "default", "delete", "do", "else", "enum", "export", "extends", "false", "finally",
    "for", "from", "function", "get", "if", "implements", "import", "in", "infer",
    "instanceof", "interface", "is", "keyof", "let", "module", "namespace", "never",
    "new", "null", "number", "object", "of", "package", "private", "protected",
    "public", "readonly", "require", "return", "set", "static", "string", "super",
    "switch", "symbol", "this", "throw", "true", "try", "type", "typeof", "undefined",
    "unknown", "var", "void", "while", "with", "yield"
};

const std::unordered_set<std::string_view> kPythonKeywords = {
    "False", "None", "True", "and", "as", "assert", "async", "await", "break",
    "class", "continue", "def", "del", "elif", "else", "except", "finally", "for",
    "from", "global", "if", "import", "in", "is", "lambda", "nonlocal", "not", "or",
    "pass", "raise", "return", "try", "while", "with", "yield", "self", "cls"
};

const std::unordered_set<std::string_view> kRustKeywords = {
    "as", "async", "await", "break", "const", "continue", "crate", "dyn", "else",
    "enum", "extern", "false", "fn", "for", "if", "impl", "in", "let", "loop",
    "match", "mod", "move", "mut", "pub", "ref", "return", "self", "Self", "static",
    "struct", "super", "trait", "true", "type", "unsafe", "use", "where", "while"
};

const std::unordered_set<std::string_view> kRustBuiltinTypes = {
    "bool", "char", "i8", "i16", "i32", "i64", "i128", "isize", "u8", "u16",
    "u32", "u64", "u128", "usize", "f32", "f64", "str", "String", "Option",
    "Result", "Some", "None", "Ok", "Err", "Vec", "Box", "Rc", "Arc"
};

const std::unordered_set<std::string_view> kGoKeywords = {
    "break", "default", "func", "interface", "select", "case", "defer", "go",
    "map", "struct", "chan", "else", "goto", "package", "switch", "const",
    "fallthrough", "if", "range", "type", "continue", "for", "import", "return",
    "var", "true", "false", "nil", "iota"
};

const std::unordered_set<std::string_view> kGoBuiltinTypes = {
    "bool", "byte", "complex64", "complex128", "error", "float32", "float64",
    "int", "int8", "int16", "int32", "int64", "rune", "string", "uint",
    "uint8", "uint16", "uint32", "uint64", "uintptr", "any"
};

const std::unordered_set<std::string_view> kJavaCSharpKeywords = {
    "abstract", "assert", "boolean", "break", "byte", "case", "catch", "char",
    "class", "const", "continue", "default", "do", "double", "else", "enum",
    "extends", "final", "finally", "float", "for", "goto", "if", "implements",
    "import", "instanceof", "int", "interface", "long", "native", "new", "package",
    "private", "protected", "public", "return", "short", "static", "strictfp",
    "super", "switch", "synchronized", "this", "throw", "throws", "transient",
    "try", "void", "volatile", "while", "true", "false", "null",
    "async", "await", "decimal", "delegate", "event", "explicit", "extern", "fixed",
    "foreach", "implicit", "in", "internal", "is", "lock", "namespace", "object",
    "operator", "out", "override", "params", "readonly", "ref", "sbyte", "sealed",
    "sizeof", "stackalloc", "string", "struct", "typeof", "uint", "ulong", "unchecked",
    "unsafe", "ushort", "using", "virtual", "yield", "var", "record", "init"
};

const std::unordered_set<std::string_view> kQmlKeywords = {
    "import", "property", "signal", "function", "readonly", "required", "alias",
    "id", "var", "as", "default", "onClicked", "onPressed", "onReleased", "onChanged"
};

const std::unordered_set<std::string_view> kQmlElements = {
    "Item", "Rectangle", "Text", "Image", "Row", "Column", "RowLayout", "ColumnLayout",
    "GridLayout", "ListView", "GridView", "Repeater", "Button", "MouseArea", "DropArea",
    "ScrollView", "Flickable", "Menu", "MenuItem", "MenuSeparator", "Dialog", "Popup",
    "Timer", "Connections", "Binding", "Component", "Loader", "Window", "ApplicationWindow"
};

const std::unordered_set<std::string_view> kSqlKeywords = {
    "select", "from", "where", "insert", "into", "values", "update", "set", "delete",
    "create", "table", "drop", "alter", "join", "left", "right", "inner", "outer",
    "on", "group", "by", "order", "having", "limit", "offset", "and", "or", "not",
    "in", "is", "null", "primary", "key", "foreign", "references", "index", "unique",
    "constraint", "as", "distinct", "union", "all", "case", "when", "then", "else",
    "end", "like", "between", "exists", "count", "avg", "min", "max", "sum"
};

const std::unordered_set<std::string_view> kShellKeywords = {
    "if", "then", "else", "elif", "fi", "for", "in", "do", "done", "while", "until",
    "case", "esac", "function", "return", "exit", "echo", "export", "source", "local",
    "read", "set", "unset", "shift", "trap", "cd", "pwd", "mkdir", "rm", "cp", "mv"
};

const std::unordered_set<std::string_view> kCMakeKeywords = {
    "cmake_minimum_required", "project", "add_executable", "add_library",
    "target_link_libraries", "target_include_directories", "target_compile_definitions",
    "target_compile_options", "target_sources", "find_package", "find_path",
    "find_library", "find_program", "set", "unset", "option", "if", "else",
    "elseif", "endif", "foreach", "endforeach", "while", "endwhile", "function",
    "endfunction", "macro", "endmacro", "include", "return", "message"
};

bool isIdentStart(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

bool isIdentChar(char c) noexcept {
    return isIdentStart(c) || (c >= '0' && c <= '9');
}

bool isPascalCase(std::string_view sv) noexcept {
    if (sv.empty() || sv[0] < 'A' || sv[0] > 'Z') return false;
    bool hasLower = false;
    for (char c : sv) {
        if (c >= 'a' && c <= 'z') {
            hasLower = true;
            break;
        }
    }
    return hasLower;
}

bool isAllCapsConstant(std::string_view sv) noexcept {
    if (sv.size() < 2) return false;
    for (char c : sv) {
        if (c >= 'a' && c <= 'z') return false;
    }
    return (sv[0] >= 'A' && sv[0] <= 'Z');
}

// ==============================================================================
// MARKDOWN INLINE TOKENIZER
// ==============================================================================

void pushMergedToken(std::vector<Token>& tokens, TokenType type, std::string_view slice) {
    if (slice.empty()) return;
    if (!tokens.empty() && tokens.back().type == type && type != TokenType::Whitespace) {
        tokens.back().text.append(slice);
    } else {
        tokens.push_back({ type, std::string(slice) });
    }
}

void appendMarkdownInlineTokens(std::vector<Token>& tokens,
                                std::string_view text,
                                TokenType defaultType,
                                bool highlightTablePipes) {
    const std::size_t n = text.size();
    std::size_t i = 0;

    while (i < n) {
        const unsigned char uc = static_cast<unsigned char>(text[i]);
        const char c = text[i];

        // 1. Whitespace run
        if (uc < 0x80 && (c == ' ' || c == '\t')) {
            std::size_t start = i;
            while (i < n && (text[i] == ' ' || text[i] == '\t')) {
                ++i;
            }
            tokens.push_back({ TokenType::Whitespace, std::string(text.substr(start, i - start)) });
            continue;
        }

        // 2. Non-ASCII UTF-8 characters (coalesced cleanly as one token)
        if (uc >= 0x80) {
            std::size_t start = i;
            while (i < n && static_cast<unsigned char>(text[i]) >= 0x80) {
                std::size_t len = utf8SequenceLength(static_cast<unsigned char>(text[i]));
                i = std::min(n, i + len);
            }
            pushMergedToken(tokens, defaultType, text.substr(start, i - start));
            continue;
        }

        // 3. Backslash escape (\*, \`, \\, etc.)
        if (c == '\\' && i + 1 < n) {
            pushMergedToken(tokens, defaultType, text.substr(i, 2));
            i += 2;
            continue;
        }

        // 4. Table pipe delimiter
        if (highlightTablePipes && c == '|') {
            tokens.push_back({ TokenType::MarkdownMarker, "|" });
            ++i;
            continue;
        }

        // 5. Inline code span (`code` or ``code``)
        if (c == '`') {
            std::size_t tickStart = i;
            while (i < n && text[i] == '`') ++i;
            std::size_t tickCount = i - tickStart;

            // Find matching closing tick run of exact length
            std::size_t search = i;
            bool foundClose = false;
            std::size_t closeEnd = i;
            while (search < n) {
                if (text[search] == '`') {
                    std::size_t runEnd = search;
                    while (runEnd < n && text[runEnd] == '`') ++runEnd;
                    if (runEnd - search == tickCount) {
                        foundClose = true;
                        closeEnd = runEnd;
                        break;
                    }
                    search = runEnd;
                } else {
                    ++search;
                }
            }
            if (foundClose) {
                tokens.push_back({ TokenType::MarkdownCode, std::string(text.substr(tickStart, closeEnd - tickStart)) });
                i = closeEnd;
            } else {
                pushMergedToken(tokens, defaultType, text.substr(tickStart, tickCount));
            }
            continue;
        }

        // 6. HTML comment <!-- ... -->
        if (c == '<' && text.substr(i).starts_with("<!--")) {
            auto endPos = text.find("-->", i + 4);
            if (endPos != std::string_view::npos) {
                endPos += 3;
                tokens.push_back({ TokenType::Comment, std::string(text.substr(i, endPos - i)) });
                i = endPos;
            } else {
                tokens.push_back({ TokenType::Comment, std::string(text.substr(i)) });
                break;
            }
            continue;
        }

        // 7. URL autolink (<https://...> or bare http(s)://...)
        if (c == '<' && (text.substr(i + 1).starts_with("http://") || text.substr(i + 1).starts_with("https://"))) {
            auto gt = text.find('>', i + 1);
            if (gt != std::string_view::npos) {
                tokens.push_back({ TokenType::Punctuation, "<" });
                tokens.push_back({ TokenType::MarkdownLink, std::string(text.substr(i + 1, gt - (i + 1))) });
                tokens.push_back({ TokenType::Punctuation, ">" });
                i = gt + 1;
                continue;
            }
        }
        if ((c == 'h') &&
            (text.substr(i).starts_with("https://") || text.substr(i).starts_with("http://")) &&
            (i == 0 || !isIdentChar(text[i - 1]))) {
            std::size_t start = i;
            int parenDepth = 0;
            while (i < n && !asciiIsSpace(text[i]) && text[i] != '"' && text[i] != '<' && text[i] != '>') {
                if (text[i] == '(') {
                    ++parenDepth;
                } else if (text[i] == ')') {
                    if (parenDepth == 0) break;
                    --parenDepth;
                } else if (text[i] == ']') {
                    break;
                }
                ++i;
            }
            while (i > start && (text[i - 1] == '.' || text[i - 1] == ',' || text[i - 1] == ';')) {
                --i;
            }
            tokens.push_back({ TokenType::MarkdownLink, std::string(text.substr(start, i - start)) });
            continue;
        }

        // 8. Links & Images: [text](url) or ![alt](url) or nested badge [![alt](img)](url)
        if ((c == '!' && i + 1 < n && text[i + 1] == '[') || c == '[') {
            const bool isImage = (c == '!');
            const std::size_t bracketOpen = isImage ? i + 1 : i;

            // Find balanced closing ']' skipping inline code
            int bracketDepth = 1;
            std::size_t k = bracketOpen + 1;
            std::size_t bracketClose = std::string_view::npos;
            while (k < n) {
                if (text[k] == '\\' && k + 1 < n) {
                    k += 2;
                    continue;
                }
                if (text[k] == '`') {
                    ++k;
                    while (k < n && text[k] != '`') ++k;
                    if (k < n) ++k;
                    continue;
                }
                if (text[k] == '[') {
                    ++bracketDepth;
                } else if (text[k] == ']') {
                    --bracketDepth;
                    if (bracketDepth == 0) {
                        bracketClose = k;
                        break;
                    }
                }
                ++k;
            }

            if (bracketClose != std::string_view::npos && bracketClose + 1 < n && text[bracketClose + 1] == '(') {
                std::size_t urlStart = bracketClose + 2;
                int parenDepth = 1;
                std::size_t p = urlStart;
                std::size_t parenClose = std::string_view::npos;
                while (p < n) {
                    if (text[p] == '\\' && p + 1 < n) {
                        p += 2;
                        continue;
                    }
                    if (text[p] == '(') ++parenDepth;
                    else if (text[p] == ')') {
                        --parenDepth;
                        if (parenDepth == 0) {
                            parenClose = p;
                            break;
                        }
                    }
                    ++p;
                }

                if (parenClose != std::string_view::npos) {
                    tokens.push_back({ TokenType::Punctuation, isImage ? "![" : "[" });
                    std::string_view labelSlice = text.substr(bracketOpen + 1, bracketClose - bracketOpen - 1);
                    if (labelSlice.find("![") != std::string_view::npos) {
                        appendMarkdownInlineTokens(tokens, labelSlice, TokenType::MarkdownLink, false);
                    } else {
                        tokens.push_back({ TokenType::MarkdownLink, std::string(labelSlice) });
                    }
                    tokens.push_back({ TokenType::Punctuation, "](" });
                    tokens.push_back({ TokenType::Comment, std::string(text.substr(urlStart, parenClose - urlStart)) });
                    tokens.push_back({ TokenType::Punctuation, ")" });
                    i = parenClose + 1;
                    continue;
                }
            }
        }

        // 9. Bold (**...**)
        if (c == '*' && i + 1 < n && text[i + 1] == '*' && i + 2 < n && !asciiIsSpace(text[i + 2])) {
            auto endPos = text.find("**", i + 2);
            if (endPos != std::string_view::npos && endPos > i + 2 && !asciiIsSpace(text[endPos - 1])) {
                tokens.push_back({ TokenType::MarkdownBold, std::string(text.substr(i, endPos + 2 - i)) });
                i = endPos + 2;
                continue;
            }
        }

        // 10. Italic (*...*)
        if (c == '*' && !(i + 1 < n && text[i + 1] == '*') && i + 1 < n && !asciiIsSpace(text[i + 1])) {
            std::size_t search = i + 1;
            std::size_t endPos = std::string_view::npos;
            while (search < n) {
                if (text[search] == '\\' && search + 1 < n) {
                    search += 2;
                    continue;
                }
                if (text[search] == '*') {
                    if (search + 1 < n && text[search + 1] == '*') {
                        search += 2;
                        continue;
                    }
                    if (!asciiIsSpace(text[search - 1])) {
                        endPos = search;
                    }
                    break;
                }
                ++search;
            }
            if (endPos != std::string_view::npos) {
                tokens.push_back({ TokenType::MarkdownItalic, std::string(text.substr(i, endPos + 1 - i)) });
                i = endPos + 1;
                continue;
            }
        }

        // 11. Strikethrough (~~...~~)
        if (c == '~' && i + 1 < n && text[i + 1] == '~' && i + 2 < n && !asciiIsSpace(text[i + 2])) {
            auto endPos = text.find("~~", i + 2);
            if (endPos != std::string_view::npos && endPos > i + 2) {
                tokens.push_back({ TokenType::Comment, std::string(text.substr(i, endPos + 2 - i)) });
                i = endPos + 2;
                continue;
            }
        }

        // 12. Simple inline HTML tags like <br/>, <div align="center">, </div>, <kbd>
        if (c == '<' && i + 1 < n &&
            ((text[i + 1] >= 'a' && text[i + 1] <= 'z') || (text[i + 1] >= 'A' && text[i + 1] <= 'Z') || text[i + 1] == '/')) {
            auto gt = text.find('>', i + 1);
            if (gt != std::string_view::npos && gt - i < 80) {
                tokens.push_back({ TokenType::Comment, std::string(text.substr(i, gt + 1 - i)) });
                i = gt + 1;
                continue;
            }
        }

        // 13. Consume contiguous plain prose run cleanly (including quotes, apostrophes, dots, prose words)
        std::size_t plainStart = i;
        ++i;
        while (i < n) {
            const unsigned char nextU = static_cast<unsigned char>(text[i]);
            if (nextU >= 0x80) {
                std::size_t seqLen = utf8SequenceLength(nextU);
                i = std::min(n, i + seqLen);
                continue;
            }
            const char nc = text[i];
            if (nc == ' ' || nc == '\t' || nc == '\\' || nc == '`' || nc == '*' ||
                nc == '~' || nc == '<' || nc == '[' || (nc == '!' && i + 1 < n && text[i + 1] == '[') ||
                (highlightTablePipes && nc == '|') ||
                (nc == 'h' && (text.substr(i).starts_with("https://") || text.substr(i).starts_with("http://")))) {
                break;
            }
            ++i;
        }
        pushMergedToken(tokens, defaultType, text.substr(plainStart, i - plainStart));
    }
}

bool isThematicBreakLine(std::string_view s) noexcept {
    char marker = '\0';
    int count = 0;
    for (char c : s) {
        if (c == ' ' || c == '\t') continue;
        if (c != '-' && c != '*' && c != '_') return false;
        if (marker == '\0') marker = c;
        else if (c != marker) return false;
        ++count;
    }
    return count >= 3;
}

std::string renderTokensToHtml(const std::vector<Token>& tokens, bool wrapFriendly) noexcept {
    std::string html;
    std::size_t estSize = 0;
    for (const auto& t : tokens) estSize += t.text.size() * 2;
    html.reserve(estSize + 16);

    bool isFirstToken = true;
    for (const auto& token : tokens) {
        const bool atLineStart = isFirstToken;
        isFirstToken = false;

        std::string escaped = wrapFriendly
                                  ? SyntaxHighlighter::escapeHtmlWrapFriendly(token.text, atLineStart)
                                  : SyntaxHighlighter::escapeHtml(token.text);

        if (token.type == TokenType::Whitespace || token.type == TokenType::PlainText) {
            html.append(escaped);
            continue;
        }

        std::string_view color = SyntaxHighlighter::tokenColor(token.type);

        if (token.type == TokenType::Keyword ||
            token.type == TokenType::MarkdownHeading ||
            token.type == TokenType::MarkdownMarker ||
            token.type == TokenType::MarkdownBold) {
            html.append("<span style=\"color:");
            html.append(color);
            html.append(";font-weight:bold;\">");
            html.append(escaped);
            html.append("</span>");
        } else if (token.type == TokenType::Type) {
            html.append("<span style=\"color:");
            html.append(color);
            html.append(";font-weight:600;\">");
            html.append(escaped);
            html.append("</span>");
        } else if (token.type == TokenType::Comment ||
                   token.type == TokenType::MarkdownItalic ||
                   token.type == TokenType::MarkdownQuote) {
            html.append("<span style=\"color:");
            html.append(color);
            html.append(";font-style:italic;\">");
            html.append(escaped);
            html.append("</span>");
        } else {
            html.append("<span style=\"color:");
            html.append(color);
            html.append(";\">");
            html.append(escaped);
            html.append("</span>");
        }
    }

    return html;
}

} // namespace

std::string_view SyntaxHighlighter::tokenColor(TokenType type) noexcept {
    switch (type) {
    case TokenType::Keyword:         return "#C678DD";
    case TokenType::Type:            return "#E5C07B";
    case TokenType::Function:        return "#61AFEF";
    case TokenType::Variable:        return "#E06C75";
    case TokenType::String:          return "#98C379";
    case TokenType::Number:          return "#D19A66";
    case TokenType::Comment:         return "#7F848E";
    case TokenType::Preprocessor:    return "#E06C75";
    case TokenType::Property:        return "#E5C07B";
    case TokenType::Constant:        return "#D19A66";
    case TokenType::Operator:        return "#56B6C2";
    case TokenType::Punctuation:     return "#ABB2BF";
    case TokenType::MarkdownHeading: return "#79C0FF";
    case TokenType::MarkdownMarker:  return "#FF7B72";
    case TokenType::MarkdownBold:    return "#F0F6FC";
    case TokenType::MarkdownItalic:  return "#D2A8FF";
    case TokenType::MarkdownCode:    return "#A5D6FF";
    case TokenType::MarkdownLink:    return "#58A6FF";
    case TokenType::MarkdownQuote:   return "#8B949E";
    case TokenType::PlainText:
    case TokenType::Whitespace:
    default:                         return "#ABB2BF";
    }
}

std::string SyntaxHighlighter::escapeHtml(std::string_view text) noexcept {
    std::string out;
    out.reserve(text.size() * 11 / 10);
    for (char c : text) {
        switch (c) {
        case '&':  out.append("&amp;"); break;
        case '<':  out.append("&lt;"); break;
        case '>':  out.append("&gt;"); break;
        case '"':  out.append("&quot;"); break;
        case ' ':  out.append("&nbsp;"); break;
        case '\t': out.append("&nbsp;&nbsp;&nbsp;&nbsp;"); break;
        default:   out.push_back(c); break;
        }
    }
    return out;
}

std::string SyntaxHighlighter::escapeHtmlWrapFriendly(std::string_view text, bool atLineStart) noexcept {
    std::string out;
    out.reserve(text.size() * 11 / 10);
    std::size_t i = 0;
    const std::size_t n = text.size();
    while (i < n) {
        const char c = text[i];
        switch (c) {
        case '&':  out.append("&amp;"); ++i; break;
        case '<':  out.append("&lt;"); ++i; break;
        case '>':  out.append("&gt;"); ++i; break;
        case '"':  out.append("&quot;"); ++i; break;
        case ' ':
        case '\t': {
            const bool isLeading = (atLineStart && i == 0);
            std::size_t runEnd = i;
            std::size_t visualSpaces = 0;
            while (runEnd < n && (text[runEnd] == ' ' || text[runEnd] == '\t')) {
                visualSpaces += (text[runEnd] == '\t') ? 4 : 1;
                ++runEnd;
            }
            if (isLeading) {
                for (std::size_t k = 0; k < visualSpaces; ++k) {
                    out.append("&nbsp;");
                }
            } else if (visualSpaces == 1) {
                out.push_back(' ');
            } else {
                for (std::size_t k = 0; k + 1 < visualSpaces; ++k) {
                    out.append("&nbsp;");
                }
                out.push_back(' ');
            }
            i = runEnd;
            break;
        }
        default:
            out.push_back(c);
            ++i;
            break;
        }
    }
    return out;
}

std::string SyntaxHighlighter::escapeHtmlForPre(std::string_view text) noexcept {
    std::string out;
    out.reserve(text.size() * 11 / 10);
    for (char c : text) {
        switch (c) {
        case '&':  out.append("&amp;"); break;
        case '<':  out.append("&lt;"); break;
        case '>':  out.append("&gt;"); break;
        case '"':  out.append("&quot;"); break;
        case '\t': out.append("    "); break;
        default:   out.push_back(c); break;
        }
    }
    return out;
}

std::vector<Token> SyntaxHighlighter::tokenizeMarkdownLine(std::string_view line, MarkdownLineState& state) noexcept {
    std::vector<Token> tokens;
    if (line.empty()) {
        return tokens;
    }

    // Count leading indentation
    std::size_t wsEnd = 0;
    std::size_t wsColumns = 0;
    while (wsEnd < line.size() && (line[wsEnd] == ' ' || line[wsEnd] == '\t')) {
        wsColumns += (line[wsEnd] == '\t') ? 4 : 1;
        ++wsEnd;
    }

    std::string_view trimmed = line.substr(wsEnd);

    // 1. Inside an open fenced code block
    if (state.inCodeFence) {
        if (wsColumns <= 3 && !trimmed.empty() && trimmed[0] == state.fenceChar) {
            std::size_t fenceEnd = 0;
            while (fenceEnd < trimmed.size() && trimmed[fenceEnd] == state.fenceChar) {
                ++fenceEnd;
            }
            bool onlyTrailingWs = true;
            for (std::size_t k = fenceEnd; k < trimmed.size(); ++k) {
                if (trimmed[k] != ' ' && trimmed[k] != '\t' && trimmed[k] != '\r') {
                    onlyTrailingWs = false;
                    break;
                }
            }
            if (fenceEnd >= state.fenceLength && onlyTrailingWs) {
                if (wsEnd > 0) {
                    tokens.push_back({ TokenType::Whitespace, std::string(line.substr(0, wsEnd)) });
                }
                tokens.push_back({ TokenType::MarkdownMarker, std::string(trimmed.substr(0, fenceEnd)) });
                if (fenceEnd < trimmed.size()) {
                    tokens.push_back({ TokenType::Whitespace, std::string(trimmed.substr(fenceEnd)) });
                }
                state.inCodeFence = false;
                state.fenceLength = 0;
                state.fenceLanguage = Language::Unknown;
                return tokens;
            }
        }

        if (state.fenceLanguage != Language::Unknown) {
            return tokenize(line, state.fenceLanguage);
        }

        // Plain code fence / tree diagram / ASCII art -> keep UTF-8 plain text completely intact
        if (wsEnd > 0) {
            tokens.push_back({ TokenType::Whitespace, std::string(line.substr(0, wsEnd)) });
        }
        if (!trimmed.empty()) {
            tokens.push_back({ TokenType::PlainText, std::string(trimmed) });
        }
        return tokens;
    }

    // 2. Check opening fenced code block (``` or ~~~ with 0-3 leading spaces)
    if (wsColumns <= 3 && trimmed.size() >= 3 && (trimmed[0] == '`' || trimmed[0] == '~')) {
        const char fChar = trimmed[0];
        std::size_t fLen = 0;
        while (fLen < trimmed.size() && trimmed[fLen] == fChar) {
            ++fLen;
        }
        std::string_view infoRaw = trimmed.substr(fLen);
        const bool invalidBacktickInfo = (fChar == '`' && infoRaw.find('`') != std::string_view::npos);
        if (fLen >= 3 && !invalidBacktickInfo) {
            if (wsEnd > 0) {
                tokens.push_back({ TokenType::Whitespace, std::string(line.substr(0, wsEnd)) });
            }
            tokens.push_back({ TokenType::MarkdownMarker, std::string(trimmed.substr(0, fLen)) });
            if (!infoRaw.empty()) {
                tokens.push_back({ TokenType::Type, std::string(infoRaw) });
            }
            state.inCodeFence = true;
            state.fenceChar = fChar;
            state.fenceLength = fLen;
            state.fenceLanguage = LanguageDetector::fromCodeFenceInfo(infoRaw);
            return tokens;
        }
    }

    // Emit leading whitespace if any
    if (wsEnd > 0) {
        tokens.push_back({ TokenType::Whitespace, std::string(line.substr(0, wsEnd)) });
    }
    if (trimmed.empty()) {
        return tokens;
    }

    // 3. ATX Heading (# Title)
    if (wsColumns <= 3 && trimmed[0] == '#') {
        std::size_t hashCount = 0;
        while (hashCount < trimmed.size() && hashCount < 7 && trimmed[hashCount] == '#') {
            ++hashCount;
        }
        if (hashCount <= 6 && (hashCount == trimmed.size() || trimmed[hashCount] == ' ' || trimmed[hashCount] == '\t')) {
            tokens.push_back({ TokenType::MarkdownMarker, std::string(trimmed.substr(0, hashCount)) });
            appendMarkdownInlineTokens(tokens, trimmed.substr(hashCount), TokenType::MarkdownHeading, false);
            return tokens;
        }
    }

    // 4. Thematic Break (---, ***, ___)
    if (wsColumns <= 3 && isThematicBreakLine(trimmed)) {
        tokens.push_back({ TokenType::Comment, std::string(trimmed) });
        return tokens;
    }

    // 5. Blockquote (> ...)
    if (wsColumns <= 3 && trimmed[0] == '>') {
        std::size_t qIdx = 0;
        while (qIdx < trimmed.size() && (trimmed[qIdx] == '>' || trimmed[qIdx] == ' ')) {
            if (trimmed[qIdx] == '>') {
                tokens.push_back({ TokenType::MarkdownMarker, ">" });
                ++qIdx;
                if (qIdx < trimmed.size() && trimmed[qIdx] == ' ') {
                    tokens.push_back({ TokenType::Whitespace, " " });
                    ++qIdx;
                }
            } else {
                break;
            }
        }
        std::string_view qRest = trimmed.substr(qIdx);
        for (std::string_view alert : { "[!NOTE]", "[!TIP]", "[!IMPORTANT]", "[!WARNING]", "[!CAUTION]" }) {
            if (qRest.starts_with(alert)) {
                tokens.push_back({ TokenType::Preprocessor, std::string(alert) });
                qRest.remove_prefix(alert.size());
                break;
            }
        }
        appendMarkdownInlineTokens(tokens, qRest, TokenType::MarkdownQuote, false);
        return tokens;
    }

    // 6. Unordered or Ordered List Item (- ..., * ..., + ..., 1. ...)
    if ((trimmed.size() >= 2 && (trimmed[0] == '-' || trimmed[0] == '*' || trimmed[0] == '+') &&
         (trimmed[1] == ' ' || trimmed[1] == '\t'))) {
        tokens.push_back({ TokenType::MarkdownMarker, std::string(trimmed.substr(0, 1)) });
        std::size_t afterMarker = 1;
        while (afterMarker < trimmed.size() && (trimmed[afterMarker] == ' ' || trimmed[afterMarker] == '\t')) {
            ++afterMarker;
        }
        tokens.push_back({ TokenType::Whitespace, std::string(trimmed.substr(1, afterMarker - 1)) });
        std::string_view itemRest = trimmed.substr(afterMarker);
        if (itemRest.size() >= 4 && itemRest[0] == '[' && itemRest[2] == ']' &&
            (itemRest[1] == ' ' || itemRest[1] == 'x' || itemRest[1] == 'X') &&
            (itemRest[3] == ' ' || itemRest[3] == '\t')) {
            tokens.push_back({ TokenType::Preprocessor, std::string(itemRest.substr(0, 3)) });
            itemRest.remove_prefix(3);
        }
        appendMarkdownInlineTokens(tokens, itemRest, TokenType::PlainText, false);
        return tokens;
    }

    if (asciiIsDigit(trimmed[0])) {
        std::size_t d = 0;
        while (d < trimmed.size() && d < 9 && asciiIsDigit(trimmed[d])) ++d;
        if (d < trimmed.size() && (trimmed[d] == '.' || trimmed[d] == ')') &&
            d + 1 < trimmed.size() && (trimmed[d + 1] == ' ' || trimmed[d + 1] == '\t')) {
            tokens.push_back({ TokenType::MarkdownMarker, std::string(trimmed.substr(0, d + 1)) });
            std::size_t afterMarker = d + 1;
            while (afterMarker < trimmed.size() && (trimmed[afterMarker] == ' ' || trimmed[afterMarker] == '\t')) {
                ++afterMarker;
            }
            tokens.push_back({ TokenType::Whitespace, std::string(trimmed.substr(d + 1, afterMarker - (d + 1))) });
            appendMarkdownInlineTokens(tokens, trimmed.substr(afterMarker), TokenType::PlainText, false);
            return tokens;
        }
    }

    // 7. Table row or regular Markdown prose
    const bool isTableRow = (!trimmed.empty() && trimmed[0] == '|');
    appendMarkdownInlineTokens(tokens, trimmed, TokenType::PlainText, isTableRow);
    return tokens;
}

std::vector<Token> SyntaxHighlighter::tokenize(std::string_view line, Language lang) noexcept {
    if (lang == Language::Markdown) {
        MarkdownLineState localState{};
        return tokenizeMarkdownLine(line, localState);
    }

    std::vector<Token> tokens;
    const std::size_t n = line.size();
    std::size_t i = 0;

    // Plain Text (Unknown language, e.g. .txt or plain code blocks): preserve UTF-8 cleanly without C++ highlighting
    if (lang == Language::Unknown) {
        while (i < n) {
            if (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n') {
                std::size_t start = i;
                while (i < n && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r' || line[i] == '\n')) {
                    ++i;
                }
                tokens.push_back({ TokenType::Whitespace, std::string(line.substr(start, i - start)) });
            } else {
                std::size_t start = i;
                while (i < n && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' && line[i] != '\n') {
                    const unsigned char uc = static_cast<unsigned char>(line[i]);
                    if (uc >= 0x80) {
                        i = std::min(n, i + utf8SequenceLength(uc));
                    } else {
                        ++i;
                    }
                }
                tokens.push_back({ TokenType::PlainText, std::string(line.substr(start, i - start)) });
            }
        }
        return tokens;
    }

    char prevNonWhitespaceChar = '\0';
    std::string prevIdentifier;
    TokenType prevTokenType = TokenType::PlainText;
    const bool isMarkup = (lang == Language::Xml || lang == Language::Xaml || lang == Language::Html);

    while (i < n) {
        char c = line[i];

        // 1. Whitespace
        if (asciiIsSpace(c)) {
            std::size_t start = i;
            while (i < n && asciiIsSpace(line[i])) {
                ++i;
            }
            tokens.push_back({ TokenType::Whitespace, std::string(line.substr(start, i - start)) });
            continue;
        }

        // Text content between tags for markup (e.g. <Label>Hello World</Label>)
        if (isMarkup && prevNonWhitespaceChar == '>' && c != '<') {
            std::size_t endPos = line.find('<', i);
            if (endPos == std::string_view::npos) endPos = n;
            tokens.push_back({ TokenType::PlainText, std::string(line.substr(i, endPos - i)) });
            i = endPos;
            prevNonWhitespaceChar = '\0';
            continue;
        }

        // 2. Line Comments
        bool isLineComment = false;
        if (c == '/' && i + 1 < n && line[i + 1] == '/') {
            isLineComment = true; // // ...
        } else if (c == '#' && (lang == Language::Python || lang == Language::Shell ||
                               lang == Language::Ruby || lang == Language::Yaml ||
                               lang == Language::Docker || lang == Language::CMake ||
                               lang == Language::Makefile || lang == Language::Git)) {
            isLineComment = true; // # ...
        } else if (c == '-' && i + 1 < n && line[i + 1] == '-' && lang == Language::Sql) {
            isLineComment = true; // -- ...
        }

        if (isLineComment) {
            tokens.push_back({ TokenType::Comment, std::string(line.substr(i)) });
            break;
        }

        // 3. Block Comments on same line /* ... */ or <!-- ... -->
        if (c == '/' && i + 1 < n && line[i + 1] == '*') {
            auto endPos = line.find("*/", i + 2);
            if (endPos != std::string_view::npos) {
                endPos += 2;
                tokens.push_back({ TokenType::Comment, std::string(line.substr(i, endPos - i)) });
                i = endPos;
                continue;
            } else {
                tokens.push_back({ TokenType::Comment, std::string(line.substr(i)) });
                break;
            }
        }
        if (c == '<' && line.substr(i).starts_with("<!--")) {
            auto endPos = line.find("-->", i + 4);
            if (endPos != std::string_view::npos) {
                endPos += 3;
                tokens.push_back({ TokenType::Comment, std::string(line.substr(i, endPos - i)) });
                i = endPos;
                continue;
            } else {
                tokens.push_back({ TokenType::Comment, std::string(line.substr(i)) });
                break;
            }
        }

        // 4. Preprocessor / Decorator / Directive (#include, @decorator, <?xml ... ?>, <!DOCTYPE ...>)
        if (isMarkup && c == '<' && (line.substr(i).starts_with("<?") || line.substr(i).starts_with("<!")) &&
            !line.substr(i).starts_with("<!--")) {
            auto endPos = line.find('>', i);
            if (endPos != std::string_view::npos) {
                ++endPos;
                tokens.push_back({ TokenType::Preprocessor, std::string(line.substr(i, endPos - i)) });
                i = endPos;
                prevNonWhitespaceChar = '>';
                continue;
            }
        }
        if (isMarkup) {
            if (c == '<' && i + 1 < n && line[i + 1] == '/') {
                tokens.push_back({ TokenType::Punctuation, "</" });
                i += 2;
                prevNonWhitespaceChar = '/';
                continue;
            }
            if (c == '/' && i + 1 < n && line[i + 1] == '>') {
                tokens.push_back({ TokenType::Punctuation, "/>" });
                i += 2;
                prevNonWhitespaceChar = '>';
                continue;
            }
        }
        if (c == '#' && (lang == Language::Cpp || lang == Language::C || lang == Language::CSharp)) {
            std::size_t start = i++;
            while (i < n && isIdentChar(line[i])) {
                ++i;
            }
            tokens.push_back({ TokenType::Preprocessor, std::string(line.substr(start, i - start)) });
            prevNonWhitespaceChar = '#';
            continue;
        }
        if (c == '@' && (lang == Language::Python || lang == Language::Java ||
                         lang == Language::TypeScript || lang == Language::Kotlin)) {
            std::size_t start = i++;
            while (i < n && isIdentChar(line[i])) {
                ++i;
            }
            tokens.push_back({ TokenType::Preprocessor, std::string(line.substr(start, i - start)) });
            prevNonWhitespaceChar = '@';
            continue;
        }

        // 5. Strings: "...", '...', `...`
        if (c == '"' || c == '\'' || c == '`') {
            char quote = c;
            std::size_t start = i++;
            while (i < n) {
                if (line[i] == '\\' && i + 1 < n) {
                    i += 2;
                } else if (line[i] == quote) {
                    ++i;
                    break;
                } else {
                    ++i;
                }
            }
            tokens.push_back({ TokenType::String, std::string(line.substr(start, i - start)) });
            prevNonWhitespaceChar = quote;
            continue;
        }

        // 6. Numbers: 123, 0xFF, 0b1010, 3.14159f
        if (asciiIsDigit(c) || (c == '.' && i + 1 < n && asciiIsDigit(line[i + 1]))) {
            std::size_t start = i;
            if (c == '0' && i + 1 < n && (line[i + 1] == 'x' || line[i + 1] == 'X' ||
                                          line[i + 1] == 'b' || line[i + 1] == 'B')) {
                i += 2;
                while (i < n && (asciiIsXDigit(line[i]) || line[i] == '_')) {
                    ++i;
                }
            } else {
                bool hasDot = (c == '.');
                while (i < n && (asciiIsDigit(line[i]) || (!hasDot && line[i] == '.') || line[i] == '_')) {
                    if (line[i] == '.') hasDot = true;
                    ++i;
                }
                if (i < n && (line[i] == 'e' || line[i] == 'E')) {
                    ++i;
                    if (i < n && (line[i] == '+' || line[i] == '-')) ++i;
                    while (i < n && asciiIsDigit(line[i])) ++i;
                }
                while (i < n && (line[i] == 'f' || line[i] == 'F' || line[i] == 'u' ||
                                 line[i] == 'U' || line[i] == 'l' || line[i] == 'L')) {
                    ++i;
                }
            }
            tokens.push_back({ TokenType::Number, std::string(line.substr(start, i - start)) });
            prevNonWhitespaceChar = '0';
            continue;
        }

        // 7. Identifiers (Keywords, Types, Functions, Properties, Classes, Enums)
        if (isIdentStart(c) || (isMarkup && (c == ':' || isIdentChar(c)))) {
            std::size_t start = i;
            while (i < n && (isIdentChar(line[i]) || (isMarkup && (line[i] == ':' || line[i] == '-' || line[i] == '.')))) {
                ++i;
            }
            std::string ident = std::string(line.substr(start, i - start));

            std::size_t nextIdx = i;
            while (nextIdx < n && asciiIsSpace(line[nextIdx])) {
                ++nextIdx;
            }
            char nextChar = (nextIdx < n) ? line[nextIdx] : '\0';

            TokenType type = TokenType::PlainText;

            if (isMarkup) {
                if (prevNonWhitespaceChar == '<' || prevNonWhitespaceChar == '/') {
                    type = TokenType::Type;
                } else if (nextChar == '=') {
                    type = TokenType::Property;
                } else if (isPascalCase(ident)) {
                    type = TokenType::Type;
                } else {
                    type = TokenType::Property;
                }
            } else {
                bool isProperty = (prevNonWhitespaceChar == '.' ||
                                   (prevNonWhitespaceChar == '>' && start >= 2 && line.substr(start - 2, 2) == "->"));

                bool isKeyword = false;
                bool isBuiltinType = false;

                switch (lang) {
                case Language::Cpp:
                case Language::C:
                    isKeyword = kCppKeywords.contains(ident);
                    isBuiltinType = kCppBuiltinTypes.contains(ident);
                    break;
                case Language::JavaScript:
                case Language::TypeScript:
                    isKeyword = kJsTsKeywords.contains(ident);
                    break;
                case Language::Python:
                    isKeyword = kPythonKeywords.contains(ident);
                    break;
                case Language::Rust:
                    isKeyword = kRustKeywords.contains(ident);
                    isBuiltinType = kRustBuiltinTypes.contains(ident);
                    break;
                case Language::Go:
                    isKeyword = kGoKeywords.contains(ident);
                    isBuiltinType = kGoBuiltinTypes.contains(ident);
                    break;
                case Language::Java:
                case Language::CSharp:
                case Language::Kotlin:
                case Language::Swift:
                    isKeyword = kJavaCSharpKeywords.contains(ident);
                    break;
                case Language::Qml:
                    isKeyword = kQmlKeywords.contains(ident) || kJsTsKeywords.contains(ident);
                    if (kQmlElements.contains(ident)) {
                        isBuiltinType = true;
                    }
                    break;
                case Language::Sql: {
                    std::string lowerIdent;
                    lowerIdent.reserve(ident.size());
                    for (char ch : ident) lowerIdent.push_back(asciiToLower(ch));
                    isKeyword = kSqlKeywords.contains(lowerIdent);
                    break;
                }
                case Language::Shell:
                case Language::PowerShell:
                    isKeyword = kShellKeywords.contains(ident);
                    break;
                case Language::CMake:
                    isKeyword = kCMakeKeywords.contains(ident);
                    break;
                default:
                    isKeyword = kCppKeywords.contains(ident) || kJsTsKeywords.contains(ident) || kPythonKeywords.contains(ident);
                    break;
                }

                if (isBuiltinType) {
                    type = TokenType::Type;
                } else if (isKeyword) {
                    type = TokenType::Keyword;
                } else if (prevIdentifier == "class" || prevIdentifier == "struct" ||
                           prevIdentifier == "enum" || prevIdentifier == "interface" ||
                           prevIdentifier == "type" || prevIdentifier == "extends" ||
                           prevIdentifier == "implements") {
                    type = TokenType::Type;
                } else if (isProperty) {
                    type = (nextChar == '(') ? TokenType::Function : TokenType::Property;
                } else if (nextChar == '(') {
                    type = TokenType::Function;
                } else if (isPascalCase(ident)) {
                    type = TokenType::Type;
                } else if (isAllCapsConstant(ident)) {
                    type = TokenType::Constant;
                } else if (ident.starts_with('$') || ident.starts_with("m_") || ident.starts_with("s_") ||
                           (ident.starts_with('_') && ident.size() > 1)) {
                    type = TokenType::Variable;
                } else if (prevTokenType == TokenType::Type || prevIdentifier == "let" ||
                           prevIdentifier == "const" || prevIdentifier == "var" ||
                           prevIdentifier == "auto" || prevIdentifier == "val") {
                    type = TokenType::Variable;
                } else {
                    type = TokenType::PlainText;
                }
            }

            tokens.push_back({ type, ident });
            prevTokenType = type;
            prevIdentifier = ident;
            prevNonWhitespaceChar = (i > 0) ? line[i - 1] : '\0';
            continue;
        }

        // 8. UTF-8 Multi-Byte Text (box-drawing characters, accented letters, emojis, etc.)
        // Keep complete UTF-8 sequences together in a single PlainText token so multi-byte
        // characters are never sliced across separate HTML <span> elements.
        if (static_cast<unsigned char>(c) >= 0x80) {
            std::size_t start = i;
            while (i < n && static_cast<unsigned char>(line[i]) >= 0x80) {
                std::size_t seqLen = utf8SequenceLength(static_cast<unsigned char>(line[i]));
                i = std::min(n, i + seqLen);
            }
            tokens.push_back({ TokenType::PlainText, std::string(line.substr(start, i - start)) });
            prevTokenType = TokenType::PlainText;
            prevNonWhitespaceChar = '\0';
            continue;
        }

        // 9. Operators and Punctuation (ASCII)
        TokenType opType = TokenType::Operator;
        if (c == ';' || c == ',' || c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' ||
            (isMarkup && (c == '<' || c == '>'))) {
            opType = TokenType::Punctuation;
        }

        tokens.push_back({ opType, std::string(1, c) });
        prevTokenType = opType;
        if (c == ';' || c == '{' || c == '}') {
            prevIdentifier.clear();
        }
        prevNonWhitespaceChar = c;
        ++i;
    }

    return tokens;
}

std::string SyntaxHighlighter::highlightLine(std::string_view line, Language lang) noexcept {
    MarkdownLineState localState{};
    return highlightLine(line, lang, localState);
}

std::string SyntaxHighlighter::highlightLine(std::string_view line,
                                             Language lang,
                                             MarkdownLineState& mdState) noexcept {
    constexpr std::size_t kMaxHighlightLineChars = 1024;
    constexpr std::size_t kMaxRenderLineChars = 4096;

    if (line.empty()) {
        if (lang == Language::Markdown) {
            (void)tokenizeMarkdownLine(line, mdState);
        }
        return "";
    }

    if (line.size() > kMaxHighlightLineChars) {
        if (lang == Language::Markdown) {
            (void)tokenizeMarkdownLine(line, mdState);
        }
        if (line.size() > kMaxRenderLineChars) {
            const std::size_t safeLen = clampUtf8Boundary(line, kMaxRenderLineChars);
            return escapeHtmlWrapFriendly(line.substr(0, safeLen), true) + " &hellip;";
        }
        return escapeHtmlWrapFriendly(line, true);
    }

    auto tokens = (lang == Language::Markdown)
                      ? tokenizeMarkdownLine(line, mdState)
                      : tokenize(line, lang);

    return renderTokensToHtml(tokens, true);
}

std::vector<MarkdownLineState> SyntaxHighlighter::computeMarkdownLineStates(std::string_view content) noexcept {
    std::vector<MarkdownLineState> preLineStates;
    if (content.empty()) {
        return preLineStates;
    }

    MarkdownLineState state{};
    std::size_t pos = 0;
    while (pos <= content.size()) {
        preLineStates.push_back(state);
        if (pos == content.size()) {
            break;
        }
        auto nl = content.find('\n', pos);
        std::string_view rawLine = (nl == std::string_view::npos)
                                       ? content.substr(pos)
                                       : content.substr(pos, nl - pos);
        if (!rawLine.empty() && rawLine.back() == '\r') {
            rawLine.remove_suffix(1);
        }
        (void)tokenizeMarkdownLine(rawLine, state);
        if (nl == std::string_view::npos) {
            break;
        }
        pos = nl + 1;
    }
    return preLineStates;
}

std::string SyntaxHighlighter::highlightCodeBlockHtml(std::string_view codeText, Language lang) noexcept {
    std::string html;
    html.reserve(codeText.size() * 2 + 32);
    html.append("<pre style=\"margin:0;white-space:pre;\">");

    std::size_t pos = 0;
    bool firstLine = true;
    while (pos < codeText.size()) {
        if (!firstLine) {
            html.push_back('\n');
        }
        firstLine = false;

        auto nl = codeText.find('\n', pos);
        std::string_view line = (nl == std::string_view::npos)
                                    ? codeText.substr(pos)
                                    : codeText.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (lang == Language::Unknown) {
            html.append(escapeHtmlForPre(line));
        } else {
            constexpr std::size_t kMaxPreLineHighlight = 1024;
            if (line.size() > kMaxPreLineHighlight) {
                html.append(escapeHtmlForPre(line));
            } else {
                auto tokens = tokenize(line, lang);
                for (const auto& token : tokens) {
                    std::string esc = escapeHtmlForPre(token.text);
                    if (token.type == TokenType::Whitespace || token.type == TokenType::PlainText) {
                        html.append(esc);
                        continue;
                    }
                    std::string_view color = tokenColor(token.type);
                    html.append("<span style=\"color:");
                    html.append(color);
                    if (token.type == TokenType::Keyword) {
                        html.append(";font-weight:bold");
                    } else if (token.type == TokenType::Type) {
                        html.append(";font-weight:600");
                    } else if (token.type == TokenType::Comment) {
                        html.append(";font-style:italic");
                    }
                    html.append(";\">");
                    html.append(esc);
                    html.append("</span>");
                }
            }
        }

        if (nl == std::string_view::npos) {
            break;
        }
        pos = nl + 1;
    }

    html.append("</pre>");
    return html;
}

} // namespace gf::core::syntax
