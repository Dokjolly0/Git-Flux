#pragma once

#include "core/shared/utf8_path.hpp"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace gf::core
{
    /// Reads an environment variable portably. MSVC flags std::getenv as
/// unsafe (C4996) because it is not thread-safe on Windows; this uses
/// _dupenv_s there and std::getenv on other platforms.
    [[nodiscard]] inline std::optional<std::string> getEnvVar(const char* name)
    {
#if defined(_MSC_VER)
        char* buffer = nullptr;
        size_t size = 0;
        if (_dupenv_s(&buffer, &size, name) != 0 || !buffer)
        {
            return std::nullopt;
        }
        std::string value(buffer);
        free(buffer);
        return value;
#else
        const char* value = std::getenv(name);
        if (!value)
        {
            return std::nullopt;
        }
        return std::string(value);
#endif
    }

    /// Resolves the current user's home directory (`HOME`, falling back to `USERPROFILE`).
    [[nodiscard]] inline std::optional<std::filesystem::path> getUserHomeDir()
    {
        auto home = getEnvVar("HOME");
        if (!home)
        {
            home = getEnvVar("USERPROFILE");
        }
        if (home && !home->empty())
        {
            return utf8ToPath(*home);
        }
        return std::nullopt;
    }
} // namespace gf::core
