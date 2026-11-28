#include "core/modules/syntax/language_detector.hpp"
#include "core/shared/string_utils.hpp"

#include <array>

namespace gf::core::syntax
{
    namespace
    {
        struct LangEntry
        {
            std::string_view key;
            Language lang;
        };

        constexpr auto kCommonKeyTable = std::to_array<LangEntry>({
            {"cpp", Language::Cpp}, {"cxx", Language::Cpp}, {"cc", Language::Cpp},
            {"hpp", Language::Cpp}, {"hxx", Language::Cpp},
            {"c", Language::C}, {"h", Language::C},
            {"cs", Language::CSharp},
            {"java", Language::Java},
            {"kt", Language::Kotlin}, {"kts", Language::Kotlin},
            {"swift", Language::Swift},
            {"py", Language::Python},
            {"ts", Language::TypeScript}, {"mts", Language::TypeScript}, {"tsx", Language::TypeScript},
            {"js", Language::JavaScript}, {"mjs", Language::JavaScript},
            {"cjs", Language::JavaScript}, {"jsx", Language::JavaScript},
            {"rs", Language::Rust},
            {"go", Language::Go},
            {"rb", Language::Ruby},
            {"php", Language::Php},
            {"html", Language::Html}, {"htm", Language::Html},
            {"css", Language::Css}, {"scss", Language::Css}, {"sass", Language::Css}, {"less", Language::Css},
            {"json", Language::Json}, {"jsonc", Language::Json},
            {"yaml", Language::Yaml}, {"yml", Language::Yaml},
            {"xml", Language::Xml}, {"svg", Language::Xml}, {"plist", Language::Xml},
            {"xaml", Language::Xaml}, {"axaml", Language::Xaml},
            {"sql", Language::Sql},
            {"sh", Language::Shell}, {"bash", Language::Shell}, {"zsh", Language::Shell},
            {"ps1", Language::PowerShell},
            {"qml", Language::Qml},
            {"md", Language::Markdown}, {"markdown", Language::Markdown},
            {"cmake", Language::CMake}
        });

        constexpr auto kExtOnlyTable = std::to_array<LangEntry>({
            {"hh", Language::Cpp}, {"inl", Language::Cpp}, {"tpp", Language::Cpp},
            {"csx", Language::CSharp},
            {"pyw", Language::Python}, {"pyi", Language::Python},
            {"cts", Language::TypeScript},
            {"config", Language::Xml}, {"csproj", Language::Xml}, {"vcxproj", Language::Xml},
            {"psm1", Language::PowerShell},
            {"qmltypes", Language::Qml},
            {"mdown", Language::Markdown}, {"mkd", Language::Markdown}, {"mdx", Language::Markdown}
        });

        constexpr auto kFenceOnlyTable = std::to_array<LangEntry>({
            {"c++", Language::Cpp},
            {"csharp", Language::CSharp}, {"c#", Language::CSharp},
            {"kotlin", Language::Kotlin},
            {"python", Language::Python}, {"python3", Language::Python},
            {"javascript", Language::JavaScript},
            {"typescript", Language::TypeScript},
            {"rust", Language::Rust},
            {"golang", Language::Go},
            {"ruby", Language::Ruby},
            {"postgres", Language::Sql}, {"postgresql", Language::Sql},
            {"mysql", Language::Sql}, {"sqlite", Language::Sql},
            {"shell", Language::Shell}, {"shellsession", Language::Shell},
            {"pwsh", Language::PowerShell}, {"powershell", Language::PowerShell},
            {"make", Language::Makefile}, {"makefile", Language::Makefile},
            {"docker", Language::Docker}, {"dockerfile", Language::Docker}
        });

        constexpr auto kPlainTextTags = std::to_array<std::string_view>({
            "text", "txt", "plain", "plaintext", "ascii", "tree", "console", "output", "log", "diff"
        });

        struct LangMeta
        {
            Language lang;
            std::string_view name;
            std::string_view icon;
        };

        constexpr auto kLanguageMetaTable = std::to_array<LangMeta>({
            {Language::Cpp, "C++", "⚡"},
            {Language::C, "C", "🅲"},
            {Language::CSharp, "C#", "♯"},
            {Language::Java, "Java", "☕"},
            {Language::Kotlin, "Kotlin", "🅺"},
            {Language::Swift, "Swift", "🦅"},
            {Language::Python, "Python", "🐍"},
            {Language::TypeScript, "TypeScript", "🟦"},
            {Language::JavaScript, "JavaScript", "🟨"},
            {Language::Rust, "Rust", "🦀"},
            {Language::Go, "Go", "🐹"},
            {Language::Ruby, "Ruby", "💎"},
            {Language::Php, "PHP", "🐘"},
            {Language::Html, "HTML", "🌐"},
            {Language::Css, "CSS", "🎨"},
            {Language::Json, "JSON", "📋"},
            {Language::Yaml, "YAML", "📜"},
            {Language::Xml, "XML", "📑"},
            {Language::Xaml, "XAML", "📐"},
            {Language::Sql, "SQL", "🗄️"},
            {Language::Shell, "Bash", "🐚"},
            {Language::PowerShell, "PowerShell", "💻"},
            {Language::Qml, "QML", "🔮"},
            {Language::Markdown, "Markdown", "📝"},
            {Language::CMake, "CMake", "🛠️"},
            {Language::Makefile, "Makefile", "⚙️"},
            {Language::Docker, "Docker", "🐳"},
            {Language::Git, "Git", "🌿"}
        });

        template <std::size_t N>
        [[nodiscard]] constexpr Language findInTable(const std::array<LangEntry, N>& table,
                                                     const std::string_view key) noexcept
        {
            for (const auto& entry : table)
            {
                if (entry.key == key) return entry.lang;
            }
            return Language::Unknown;
        }

        std::string_view extractFileName(const std::string_view path) noexcept
        {
            const auto pos = path.find_last_of("/\\");
            return pos == std::string_view::npos ? path : path.substr(pos + 1);
        }

        std::string_view extractExtension(const std::string_view fileName) noexcept
        {
            const auto pos = fileName.rfind('.');
            return (pos == std::string_view::npos || pos == 0) ? std::string_view{} : fileName.substr(pos);
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
        std::string lower = toLowerAscii(ext);
        if (!lower.starts_with('.'))
        {
            return Language::Unknown;
        }
        lower.erase(0, 1);

        if (const auto lang = findInTable(kCommonKeyTable, lower); lang != Language::Unknown)
        {
            return lang;
        }
        return findInTable(kExtOnlyTable, lower);
    }

    Language LanguageDetector::fromCodeFenceInfo(std::string_view infoString) noexcept
    {
        while (!infoString.empty() && (infoString.front() == ' ' || infoString.front() == '\t'))
        {
            infoString.remove_prefix(1);
        }
        if (infoString.empty()) return Language::Unknown;

        std::size_t endPos = 0;
        while (endPos < infoString.size())
        {
            if (const char c = infoString[endPos];
                c == ' ' || c == '\t' || c == '{' || c == ',' || c == ';' || c == ':')
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
        if (tag.empty()) return Language::Unknown;
        for (const auto plain : kPlainTextTags)
        {
            if (tag == plain) return Language::Unknown;
        }

        if (const auto lang = findInTable(kCommonKeyTable, tag); lang != Language::Unknown)
        {
            return lang;
        }
        return findInTable(kFenceOnlyTable, tag);
    }

    LanguageInfo LanguageDetector::detect(const std::string_view filePath) noexcept
    {
        if (filePath.empty())
        {
            return {.language = Language::Unknown, .name = "Plain Text", .icon = "📄"};
        }

        const std::string_view fileName = extractFileName(filePath);
        Language lang = fromFileName(fileName);
        if (lang == Language::Unknown)
        {
            lang = fromExtension(extractExtension(fileName));
        }

        for (const auto& meta : kLanguageMetaTable)
        {
            if (meta.lang == lang)
            {
                return {.language = meta.lang, .name = std::string(meta.name), .icon = std::string(meta.icon)};
            }
        }
        return {.language = Language::Unknown, .name = "Plain Text", .icon = "📄"};
    }
} // namespace gf::core::syntax
