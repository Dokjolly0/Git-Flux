#include "core/modules/syntax/syntax_highlighter.hpp"

#include <cctype>
#include <string_view>
#include <unordered_set>

namespace gf::core::syntax {

namespace {

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

const std::unordered_set<std::string_view> kPythonBuiltins = {
    "int", "float", "str", "bool", "list", "dict", "set", "tuple", "bytes", "object",
    "print", "len", "range", "enumerate", "zip", "isinstance", "issubclass", "type",
    "open", "super", "sum", "min", "max", "abs", "round", "all", "any"
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
    // C# specifics
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
        if ((c >= 'a' && c <= 'z')) return false;
    }
    return (sv[0] >= 'A' && sv[0] <= 'Z');
}

} // namespace

std::string_view SyntaxHighlighter::tokenColor(TokenType type) noexcept {
    switch (type) {
    case TokenType::Keyword:      return "#C678DD"; // Purple / Pink
    case TokenType::Type:         return "#E5C07B"; // Warm Gold / Yellow (Classes, Structs, Enums, Types)
    case TokenType::Function:     return "#61AFEF"; // Sky Blue (Methods & Functions)
    case TokenType::Variable:     return "#E06C75"; // Coral / Salmon (Variables)
    case TokenType::String:       return "#98C379"; // Sage Green
    case TokenType::Number:       return "#D19A66"; // Orange / Peach
    case TokenType::Comment:      return "#7F848E"; // Slate Gray
    case TokenType::Preprocessor: return "#E06C75"; // Coral / Rose
    case TokenType::Property:     return "#E5C07B"; // Light Amber / Property
    case TokenType::Constant:     return "#D19A66"; // Orange / Enum item
    case TokenType::Operator:     return "#56B6C2"; // Cyan / Muted Operator
    case TokenType::Punctuation:  return "#ABB2BF"; // Light Slate
    case TokenType::PlainText:
    case TokenType::Whitespace:
    default:                      return "#ABB2BF"; // Default code text
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

std::vector<Token> SyntaxHighlighter::tokenize(std::string_view line, Language lang) noexcept {
    std::vector<Token> tokens;
    const std::size_t n = line.size();
    std::size_t i = 0;

    char prevNonWhitespaceChar = '\0';
    std::string prevIdentifier;
    TokenType prevTokenType = TokenType::PlainText;
    const bool isMarkup = (lang == Language::Xml || lang == Language::Xaml || lang == Language::Html);

    while (i < n) {
        char c = line[i];

        // 1. Whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            std::size_t start = i;
            while (i < n && std::isspace(static_cast<unsigned char>(line[i]))) {
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

        // 4. Preprocessor / Decorator / Directive (#include, @decorator, [Attribute], <?xml ... ?>, <!DOCTYPE ...>)
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
                    i += 2; // skip escape sequence
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
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            std::size_t start = i;
            if (c == '0' && i + 1 < n && (line[i + 1] == 'x' || line[i + 1] == 'X' ||
                                         line[i + 1] == 'b' || line[i + 1] == 'B')) {
                i += 2;
                while (i < n && (std::isxdigit(static_cast<unsigned char>(line[i])) || line[i] == '_')) {
                    ++i;
                }
            } else {
                bool hasDot = (c == '.');
                while (i < n && (std::isdigit(static_cast<unsigned char>(line[i])) ||
                                (!hasDot && line[i] == '.') || line[i] == '_')) {
                    if (line[i] == '.') hasDot = true;
                    ++i;
                }
                if (i < n && (line[i] == 'e' || line[i] == 'E')) {
                    ++i;
                    if (i < n && (line[i] == '+' || line[i] == '-')) ++i;
                    while (i < n && std::isdigit(static_cast<unsigned char>(line[i]))) ++i;
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

            // Peek next non-whitespace character to see if it's a function call "(" or attribute "="
            std::size_t nextIdx = i;
            while (nextIdx < n && std::isspace(static_cast<unsigned char>(line[nextIdx]))) {
                ++nextIdx;
            }
            char nextChar = (nextIdx < n) ? line[nextIdx] : '\0';

            TokenType type = TokenType::PlainText;

            if (isMarkup) {
                if (prevNonWhitespaceChar == '<' || prevNonWhitespaceChar == '/') {
                    type = TokenType::Type; // Tag Name (e.g. Button, Grid, x:Key)
                } else if (nextChar == '=') {
                    type = TokenType::Property; // Attribute Name (e.g. Margin, x:Name, Click)
                } else if (isPascalCase(ident)) {
                    type = TokenType::Type;
                } else {
                    type = TokenType::Property;
                }
            } else {
                // Check if member access (e.g. obj.member, ptr->member)
                bool isProperty = (prevNonWhitespaceChar == '.' ||
                                  (prevNonWhitespaceChar == '>' && start >= 2 && line.substr(start - 2, 2) == "->"));

                // Check keywords based on language
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
                    for (char ch : ident) lowerIdent.push_back(static_cast<char>(std::tolower(ch)));
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
                    // Generic fallback keyword check
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
                    type = TokenType::Type; // Named class, struct, enum or interface declaration
                } else if (isProperty) {
                    type = (nextChar == '(') ? TokenType::Function : TokenType::Property;
                } else if (nextChar == '(') {
                    type = TokenType::Function; // Function / Method call
                } else if (isPascalCase(ident)) {
                    type = TokenType::Type; // Class, Struct, Type, or Enum name by naming convention
                } else if (isAllCapsConstant(ident)) {
                    type = TokenType::Constant; // ALL_CAPS constant or enum value
                } else if (ident.starts_with('$') || ident.starts_with("m_") || ident.starts_with("s_") ||
                           (ident.starts_with('_') && ident.size() > 1)) {
                    type = TokenType::Variable; // Member variable, Shell variable, or private field
                } else if (prevTokenType == TokenType::Type || prevIdentifier == "let" ||
                           prevIdentifier == "const" || prevIdentifier == "var" ||
                           prevIdentifier == "auto" || prevIdentifier == "val") {
                    type = TokenType::Variable; // Variable declaration after type/specifier
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

        // 8. Operators and Punctuation
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
    if (line.empty()) {
        return "";
    }

    auto tokens = tokenize(line, lang);
    std::string html;
    html.reserve(line.size() * 3);

    for (const auto& token : tokens) {
        if (token.type == TokenType::Whitespace) {
            html.append(escapeHtml(token.text));
            continue;
        }

        std::string_view color = tokenColor(token.type);
        std::string escaped = escapeHtml(token.text);

        if (token.type == TokenType::PlainText) {
            html.append(escaped);
        } else if (token.type == TokenType::Keyword) {
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
        } else if (token.type == TokenType::Comment) {
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

} // namespace gf::core::syntax
