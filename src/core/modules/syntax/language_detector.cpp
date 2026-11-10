#include "core/modules/syntax/language_detector.hpp"

#include <algorithm>
#include <cctype>

namespace gf::core::syntax
{
    namespace
    {
        std::string toLower(std::string_view sv)
        {
            std::string s;
            s.reserve(sv.size());
            for (char c : sv)
            {
                s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return s;
        }

        std::string_view extractFileName(std::string_view path) noexcept
        {
            auto pos = path.find_last_of("/\\");
            if (pos == std::string_view::npos)
            {
                return path;
            }
            return path.substr(pos + 1);
        }

        std::string_view extractExtension(std::string_view fileName) noexcept
        {
            auto pos = fileName.rfind('.');
            if (pos == std::string_view::npos || pos == 0)
            {
                return "";
            }
            return fileName.substr(pos);
        }
    } // namespace

    Language LanguageDetector::fromFileName(std::string_view fileName) noexcept
    {
        std::string lower = toLower(fileName);
        if (lower == "cmakelists.txt") return Language::CMake;
        if (lower == "makefile" || lower == "gnumakefile") return Language::Makefile;
        if (lower.starts_with("dockerfile") || lower.starts_with("containerfile")) return Language::Docker;
        if (lower == ".gitignore" || lower == ".gitattributes" || lower == ".gitmodules") return Language::Git;
        return Language::Unknown;
    }

    Language LanguageDetector::fromExtension(std::string_view ext) noexcept
    {
        std::string lower = toLower(ext);

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
            char c = infoString[endPos];
            if (c == ' ' || c == '\t' || c == '{' || c == ',' || c == ';' || c == ':')
            {
                break;
            }
            ++endPos;
        }

        std::string tag = toLower(infoString.substr(0, endPos));
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

    LanguageInfo LanguageDetector::detect(std::string_view filePath) noexcept
    {
        if (filePath.empty())
        {
            return {Language::Unknown, "Plain Text", "📄"};
        }

        std::string_view fileName = extractFileName(filePath);

        // 1. Check exact filename
        Language lang = fromFileName(fileName);

        // 2. If unknown, check extension
        if (lang == Language::Unknown)
        {
            std::string_view ext = extractExtension(fileName);
            lang = fromExtension(ext);
        }

        switch (lang)
        {
        case Language::Cpp: return {Language::Cpp, "C++", "⚡"};
        case Language::C: return {Language::C, "C", "🅲"};
        case Language::CSharp: return {Language::CSharp, "C#", "♯"};
        case Language::Java: return {Language::Java, "Java", "☕"};
        case Language::Kotlin: return {Language::Kotlin, "Kotlin", "🅺"};
        case Language::Swift: return {Language::Swift, "Swift", "🦅"};
        case Language::Python: return {Language::Python, "Python", "🐍"};
        case Language::TypeScript: return {Language::TypeScript, "TypeScript", "🟦"};
        case Language::JavaScript: return {Language::JavaScript, "JavaScript", "🟨"};
        case Language::Rust: return {Language::Rust, "Rust", "🦀"};
        case Language::Go: return {Language::Go, "Go", "🐹"};
        case Language::Ruby: return {Language::Ruby, "Ruby", "💎"};
        case Language::Php: return {Language::Php, "PHP", "🐘"};
        case Language::Html: return {Language::Html, "HTML", "🌐"};
        case Language::Css: return {Language::Css, "CSS", "🎨"};
        case Language::Json: return {Language::Json, "JSON", "📋"};
        case Language::Yaml: return {Language::Yaml, "YAML", "📜"};
        case Language::Xml: return {Language::Xml, "XML", "📑"};
        case Language::Xaml: return {Language::Xaml, "XAML", "📐"};
        case Language::Sql: return {Language::Sql, "SQL", "🗄️"};
        case Language::Shell: return {Language::Shell, "Bash", "🐚"};
        case Language::PowerShell: return {Language::PowerShell, "PowerShell", "💻"};
        case Language::Qml: return {Language::Qml, "QML", "🔮"};
        case Language::Markdown: return {Language::Markdown, "Markdown", "📝"};
        case Language::CMake: return {Language::CMake, "CMake", "🛠️"};
        case Language::Makefile: return {Language::Makefile, "Makefile", "⚙️"};
        case Language::Docker: return {Language::Docker, "Docker", "🐳"};
        case Language::Git: return {Language::Git, "Git", "🌿"};
        default: return {Language::Unknown, "Plain Text", "📄"};
        }
    }
} // namespace gf::core::syntax
