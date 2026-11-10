#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace gf::core
{
    /// Converts a std::filesystem::path to a UTF-8 std::string without going
/// through the local ANSI code page (essential on Windows with Unicode characters).
    [[nodiscard]] inline std::string pathToUtf8(const std::filesystem::path& p)
    {
        const auto u8 = p.u8string();
        return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
    }

    /// Constructs a std::filesystem::path from a valid UTF-8 string view.
    [[nodiscard]] inline std::filesystem::path utf8ToPath(std::string_view utf8)
    {
        const std::u8string_view u8View(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size());
        return std::filesystem::path(u8View);
    }
} // namespace gf::core
