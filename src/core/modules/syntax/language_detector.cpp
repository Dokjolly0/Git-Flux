#include "core/modules/syntax/language_detector.hpp"
#include "core/shared/string_utils.hpp"

namespace gf::core::syntax
{
    namespace
    {
        std::string_view extractFileName(const std::string_view path) noexcept
        {
            const auto pos = path.find_last_of("/\\");
            if (pos == std::string_view::npos)
            {
                return path;
            }
            return path.substr(pos + 1);
        }

        std::string_view extractExtension(const std::string_view fileName) noexcept
        {
            const auto pos = fileName.rfind('.');
            if (pos == std::string_view::npos || pos == 0)
            {
                return "";
            }
            return fileName.substr(pos);
        }
    } // namespace

    Language LanguageDetector::fromFileName(const std::string_view fileName) noexcept
    {
        const std::string lower = toLowerAscii(fileName);
        if (lower == "cmakelists.txt") return Language::CMake;
        if (lower == "makefile" || lower == "gnumakefile") return Language::Makefile;
        if (lower.starts_with("dockerfile") || lower.starts_with("containerfile")) return Language::Docker;
        if (lower == ".gitignore" || lower == ".gitattributes" || lower == ".gitmodules") return Language::Git;
        return Language::Unknown;
    }

    Language LanguageDetector::fromExtension(const std::string_view ext) noexcept
    {
        const std::string lower = toLowerAscii(ext);

        // C / C++
        if (lower == ".cpp" || lower == ".cxx" || lower == ".cc" || lower == ".hpp" ||
            lower == ".hxx" || lower == ".hh" || lower == ".inl" || lower == ".tpp")
        {
            return Language::Cpp;
        }
        if (lower == ".c" || lower == ".h")
        {
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
        if (lower == ".xml" || lower == ".svg" || lower == ".plist" || lower == ".config" || lower == ".csproj" || lower
            == ".vcxproj") return Language::Xml;
        if (lower == ".xaml" || lower == ".axaml") return Language::Xaml;

        // SQL
        if (lower == ".sql") return Language::Sql;

        // Shell / Scripting
        if (lower == ".sh" || lower == ".bash" || lower == ".zsh") return Language::Shell;
        if (lower == ".ps1" || lower == ".psm1") return Language::PowerShell;

        // QML / Qt
        if (lower == ".qml" || lower == ".qmltypes") return Language::Qml;

        // Markdown / Docs
        if (lower == ".md" || lower == ".markdown" || lower == ".mdown" ||
            lower == ".mkd" || lower == ".mdx")
        {
            return Language::Markdown;
        }

        // Build tools
        if (lower == ".cmake") return Language::CMake;

        return Language::Unknown;
    }

    Language LanguageDetector::fromCodeFenceInfo(std::string_view infoString) noexcept
    {
        while (!infoString.empty() &&
            (infoString.front() == ' ' || infoString.front() == '\t'))
        {
            infoString.remove_prefix(1);
        }
        if (infoString.empty())
        {
            return Language::Unknown;
        }

        std::size_t endPos = 0;
        while (endPos < infoString.size())
        {
            if (const char c = infoString[endPos]; c == ' ' || c == '\t' || c == '{' || c == ',' || c == ';' || c == ':')
            {
                break;
            }
            ++endPos;
        }

        std::string tag = toLowerAscii(infoString.substr(0, endPos));
        if (tag.starts_with('.'))
        {
            tag.erase(0, 1);
        }

        if (tag.empty() || tag == "text" || tag == "txt" || tag == "plain" ||
            tag == "plaintext" || tag == "ascii" || tag == "tree" ||
            tag == "console" || tag == "output" || tag == "log" || tag == "diff")
        {
            return Language::Unknown;
        }

        if (tag == "cpp" || tag == "c++" || tag == "cc" || tag == "cxx" || tag == "hpp" || tag == "hxx") return
            Language::Cpp;
        if (tag == "c" || tag == "h") return Language::C;
        if (tag == "cs" || tag == "csharp" || tag == "c#") return Language::CSharp;
        if (tag == "java") return Language::Java;
        if (tag == "kt" || tag == "kts" || tag == "kotlin") return Language::Kotlin;
        if (tag == "swift") return Language::Swift;
        if (tag == "py" || tag == "python" || tag == "python3") return Language::Python;
        if (tag == "js" || tag == "javascript" || tag == "jsx" || tag == "mjs" || tag == "cjs") return
            Language::JavaScript;
        if (tag == "ts" || tag == "typescript" || tag == "tsx" || tag == "mts") return Language::TypeScript;
        if (tag == "rs" || tag == "rust") return Language::Rust;
        if (tag == "go" || tag == "golang") return Language::Go;
        if (tag == "rb" || tag == "ruby") return Language::Ruby;
        if (tag == "php") return Language::Php;
        if (tag == "html" || tag == "htm") return Language::Html;
        if (tag == "css" || tag == "scss" || tag == "sass" || tag == "less") return Language::Css;
        if (tag == "json" || tag == "jsonc") return Language::Json;
        if (tag == "yaml" || tag == "yml") return Language::Yaml;
        if (tag == "xml" || tag == "svg" || tag == "plist") return Language::Xml;
        if (tag == "xaml" || tag == "axaml") return Language::Xaml;
        if (tag == "sql" || tag == "postgres" || tag == "postgresql" || tag == "mysql" || tag == "sqlite") return
            Language::Sql;
        if (tag == "sh" || tag == "bash" || tag == "zsh" || tag == "shell" || tag == "shellsession") return
            Language::Shell;
        if (tag == "ps1" || tag == "pwsh" || tag == "powershell") return Language::PowerShell;
        if (tag == "qml") return Language::Qml;
        if (tag == "md" || tag == "markdown") return Language::Markdown;
        if (tag == "cmake") return Language::CMake;
        if (tag == "make" || tag == "makefile") return Language::Makefile;
        if (tag == "docker" || tag == "dockerfile") return Language::Docker;

        return Language::Unknown;
    }

    LanguageInfo LanguageDetector::detect(const std::string_view filePath) noexcept
    {
        if (filePath.empty())
        {
            return {.language = Language::Unknown, .name = "Plain Text", .icon = "📄"};
        }

        const std::string_view fileName = extractFileName(filePath);

        // 1. Check exact filename
        Language lang = fromFileName(fileName);

        // 2. If unknown, check extension
        if (lang == Language::Unknown)
        {
            const std::string_view ext = extractExtension(fileName);
            lang = fromExtension(ext);
        }

        switch (lang)
        {
        case Language::Cpp: return {.language = Language::Cpp, .name = "C++", .icon = "⚡"};
        case Language::C: return {.language = Language::C, .name = "C", .icon = "🅲"};
        case Language::CSharp: return {.language = Language::CSharp, .name = "C#", .icon = "♯"};
        case Language::Java: return {.language = Language::Java, .name = "Java", .icon = "☕"};
        case Language::Kotlin: return {.language = Language::Kotlin, .name = "Kotlin", .icon = "🅺"};
        case Language::Swift: return {.language = Language::Swift, .name = "Swift", .icon = "🦅"};
        case Language::Python: return {.language = Language::Python, .name = "Python", .icon = "🐍"};
        case Language::TypeScript: return {.language = Language::TypeScript, .name = "TypeScript", .icon = "🟦"};
        case Language::JavaScript: return {.language = Language::JavaScript, .name = "JavaScript", .icon = "🟨"};
        case Language::Rust: return {.language = Language::Rust, .name = "Rust", .icon = "🦀"};
        case Language::Go: return {.language = Language::Go, .name = "Go", .icon = "🐹"};
        case Language::Ruby: return {.language = Language::Ruby, .name = "Ruby", .icon = "💎"};
        case Language::Php: return {.language = Language::Php, .name = "PHP", .icon = "🐘"};
        case Language::Html: return {.language = Language::Html, .name = "HTML", .icon = "🌐"};
        case Language::Css: return {.language = Language::Css, .name = "CSS", .icon = "🎨"};
        case Language::Json: return {.language = Language::Json, .name = "JSON", .icon = "📋"};
        case Language::Yaml: return {.language = Language::Yaml, .name = "YAML", .icon = "📜"};
        case Language::Xml: return {.language = Language::Xml, .name = "XML", .icon = "📑"};
        case Language::Xaml: return {.language = Language::Xaml, .name = "XAML", .icon = "📐"};
        case Language::Sql: return {.language = Language::Sql, .name = "SQL", .icon = "🗄️"};
        case Language::Shell: return {.language = Language::Shell, .name = "Bash", .icon = "🐚"};
        case Language::PowerShell: return {.language = Language::PowerShell, .name = "PowerShell", .icon = "💻"};
        case Language::Qml: return {.language = Language::Qml, .name = "QML", .icon = "🔮"};
        case Language::Markdown: return {.language = Language::Markdown, .name = "Markdown", .icon = "📝"};
        case Language::CMake: return {.language = Language::CMake, .name = "CMake", .icon = "🛠️"};
        case Language::Makefile: return {.language = Language::Makefile, .name = "Makefile", .icon = "⚙️"};
        case Language::Docker: return {.language = Language::Docker, .name = "Docker", .icon = "🐳"};
        case Language::Git: return {.language = Language::Git, .name = "Git", .icon = "🌿"};
        default: return {.language = Language::Unknown, .name = "Plain Text", .icon = "📄"};
        }
    }
} // namespace gf::core::syntax
