#include "core/modules/syntax/language_detector.hpp"

#include <algorithm>
#include <cctype>

namespace gf::core::syntax {

namespace {

std::string toLower(std::string_view sv) {
    std::string s;
    s.reserve(sv.size());
    for (char c : sv) {
        s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return s;
}

std::string_view extractFileName(std::string_view path) noexcept {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string_view::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string_view extractExtension(std::string_view fileName) noexcept {
    auto pos = fileName.rfind('.');
    if (pos == std::string_view::npos || pos == 0) {
        return "";
    }
    return fileName.substr(pos);
}

} // namespace

Language LanguageDetector::fromFileName(std::string_view fileName) noexcept {
    std::string lower = toLower(fileName);
    if (lower == "cmakelists.txt") return Language::CMake;
    if (lower == "makefile" || lower == "gnumakefile") return Language::Makefile;
    if (lower.starts_with("dockerfile") || lower.starts_with("containerfile")) return Language::Docker;
    if (lower == ".gitignore" || lower == ".gitattributes" || lower == ".gitmodules") return Language::Git;
    return Language::Unknown;
}

Language LanguageDetector::fromExtension(std::string_view ext) noexcept {
    std::string lower = toLower(ext);

    // C / C++
    if (lower == ".cpp" || lower == ".cxx" || lower == ".cc" || lower == ".hpp" ||
        lower == ".hxx" || lower == ".hh" || lower == ".inl" || lower == ".tpp") {
        return Language::Cpp;
    }
    if (lower == ".c" || lower == ".h") {
        return Language::C;
    }

    // C#
    if (lower == ".cs" || lower == ".csx") return Language::CSharp;

    // Java / Kotlin
    if (lower == ".java") return Language::Java;
    if (lower == ".kt" || lower == ".kts") return Language::Kotlin;

    // Swift
    if (lower == ".swift") return Language::Swift;

    // Python
    if (lower == ".py" || lower == ".pyw" || lower == ".pyi") return Language::Python;

    // TypeScript / JavaScript
    if (lower == ".ts" || lower == ".mts" || lower == ".cts" || lower == ".tsx") return Language::TypeScript;
    if (lower == ".js" || lower == ".mjs" || lower == ".cjs" || lower == ".jsx") return Language::JavaScript;

    // Rust
    if (lower == ".rs") return Language::Rust;

    // Go
    if (lower == ".go") return Language::Go;

    // Ruby / PHP
    if (lower == ".rb") return Language::Ruby;
    if (lower == ".php") return Language::Php;

    // Web / Styles
    if (lower == ".html" || lower == ".htm") return Language::Html;
    if (lower == ".css" || lower == ".scss" || lower == ".sass" || lower == ".less") return Language::Css;

    // Data / Config
    if (lower == ".json" || lower == ".jsonc") return Language::Json;
    if (lower == ".yaml" || lower == ".yml") return Language::Yaml;
    if (lower == ".xml" || lower == ".svg" || lower == ".xaml") return Language::Xml;

    // SQL
    if (lower == ".sql") return Language::Sql;

    // Shell / Scripting
    if (lower == ".sh" || lower == ".bash" || lower == ".zsh") return Language::Shell;
    if (lower == ".ps1" || lower == ".psm1") return Language::PowerShell;

    // QML / Qt
    if (lower == ".qml" || lower == ".qmltypes") return Language::Qml;

    // Markdown / Docs
    if (lower == ".md" || lower == ".markdown") return Language::Markdown;

    // Build tools
    if (lower == ".cmake") return Language::CMake;

    return Language::Unknown;
}

LanguageInfo LanguageDetector::detect(std::string_view filePath) noexcept {
    if (filePath.empty()) {
        return { Language::Unknown, "Plain Text", "📄" };
    }

    std::string_view fileName = extractFileName(filePath);

    // 1. Check exact filename
    Language lang = fromFileName(fileName);

    // 2. If unknown, check extension
    if (lang == Language::Unknown) {
        std::string_view ext = extractExtension(fileName);
        lang = fromExtension(ext);
    }

    switch (lang) {
    case Language::Cpp:        return { Language::Cpp, "C++", "⚡" };
    case Language::C:          return { Language::C, "C", "🅲" };
    case Language::CSharp:     return { Language::CSharp, "C#", "♯" };
    case Language::Java:       return { Language::Java, "Java", "☕" };
    case Language::Kotlin:     return { Language::Kotlin, "Kotlin", "🅺" };
    case Language::Swift:      return { Language::Swift, "Swift", "🦅" };
    case Language::Python:     return { Language::Python, "Python", "🐍" };
    case Language::TypeScript: return { Language::TypeScript, "TypeScript", "🟦" };
    case Language::JavaScript: return { Language::JavaScript, "JavaScript", "🟨" };
    case Language::Rust:       return { Language::Rust, "Rust", "🦀" };
    case Language::Go:         return { Language::Go, "Go", "🐹" };
    case Language::Ruby:       return { Language::Ruby, "Ruby", "💎" };
    case Language::Php:        return { Language::Php, "PHP", "🐘" };
    case Language::Html:       return { Language::Html, "HTML", "🌐" };
    case Language::Css:        return { Language::Css, "CSS", "🎨" };
    case Language::Json:       return { Language::Json, "JSON", "📋" };
    case Language::Yaml:       return { Language::Yaml, "YAML", "📜" };
    case Language::Xml:        return { Language::Xml, "XML", "📑" };
    case Language::Sql:        return { Language::Sql, "SQL", "🗄️" };
    case Language::Shell:      return { Language::Shell, "Bash", "🐚" };
    case Language::PowerShell: return { Language::PowerShell, "PowerShell", "💻" };
    case Language::Qml:        return { Language::Qml, "QML", "🔮" };
    case Language::Markdown:   return { Language::Markdown, "Markdown", "📝" };
    case Language::CMake:      return { Language::CMake, "CMake", "🛠️" };
    case Language::Makefile:   return { Language::Makefile, "Makefile", "⚙️" };
    case Language::Docker:     return { Language::Docker, "Docker", "🐳" };
    case Language::Git:        return { Language::Git, "Git", "🌿" };
    default:                   return { Language::Unknown, "Plain Text", "📄" };
    }
}

} // namespace gf::core::syntax
