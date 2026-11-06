#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace gf::core {

/// Converte un std::filesystem::path in std::string UTF-8 senza passare
/// dalla codepage ANSI locale (fondamentale su Windows con caratteri Unicode).
[[nodiscard]] inline std::string pathToUtf8(const std::filesystem::path& p) {
    const auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

/// Costruisce un std::filesystem::path a partire da una stringa UTF-8 valida.
[[nodiscard]] inline std::filesystem::path utf8ToPath(std::string_view utf8) {
    const std::u8string_view u8View(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size());
    return std::filesystem::path(u8View);
}

} // namespace gf::core
