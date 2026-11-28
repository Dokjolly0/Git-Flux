#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace gf::core
{
    /// Curated 8-color accent palette shared across repository groups and terminal tabs.
    inline constexpr std::array<std::string_view, 8> kAccentPalette = {
        "#38BDF8", // Sky Cyan
        "#A78BFA", // Violet
        "#34D399", // Emerald
        "#FBBF24", // Amber
        "#FB7185", // Rose
        "#818CF8", // Indigo
        "#2DD4BF", // Teal
        "#FB923C"  // Orange
    };

    /// Default accent color used for terminal tabs, prompt repository name and branch chip.
    inline constexpr std::string_view kDefaultAccentColor = "#38BDF8";

    /// Returns true if `color` is a valid `#RRGGBB` hex string (or empty when `allowEmpty` is true).
    [[nodiscard]] constexpr bool isValidHexColor(const std::string_view color,
                                                 const bool allowEmpty = true) noexcept
    {
        if (color.empty())
        {
            return allowEmpty;
        }
        if (color.size() != 7 || color[0] != '#')
        {
            return false;
        }
        for (std::size_t i = 1; i < 7; ++i)
        {
            const char c = color[i];
            const bool isHex = (c >= '0' && c <= '9') ||
                               (c >= 'a' && c <= 'f') ||
                               (c >= 'A' && c <= 'F');
            if (!isHex)
            {
                return false;
            }
        }
        return true;
    }
} // namespace gf::core
