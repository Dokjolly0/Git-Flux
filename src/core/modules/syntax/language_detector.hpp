#pragma once

#include <string>
#include <string_view>

namespace gf::core::syntax
{
    enum class Language
    {
        Unknown,
        Cpp,
        C,
        CSharp,
        Java,
        Kotlin,
        Swift,
        Python,
        JavaScript,
        TypeScript,
        Rust,
        Go,
        Ruby,
        Php,
        Html,
        Css,
        Json,
        Yaml,
        Xml,
        Xaml,
        Sql,
        Shell,
        PowerShell,
        Qml,
        Markdown,
        CMake,
        Makefile,
        Docker,
        Git
    };

    struct LanguageInfo
    {
        Language language = Language::Unknown;
        std::string name = "Plain Text";
        std::string icon = "📄";
    };

    class LanguageDetector
    {
    public:
        [[nodiscard]] static LanguageInfo detect(std::string_view filePath) noexcept;
        [[nodiscard]] static Language fromExtension(std::string_view ext) noexcept;
        [[nodiscard]] static Language fromFileName(std::string_view fileName) noexcept;
        [[nodiscard]] static Language fromCodeFenceInfo(std::string_view infoString) noexcept;
    };
} // namespace gf::core::syntax
