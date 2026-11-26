#pragma once

#include "core/modules/groups/bo/group_id.hpp"
#include "core/modules/repositories/bo/repository_id.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gf::groups::bo
{
    /// Curated dark-theme accent palette for repository groups (Linear / Arc inspired).
    inline constexpr std::array<std::string_view, 8> kDefaultGroupPalette = {
        "#38BDF8", // Sky Cyan
        "#A78BFA", // Violet
        "#34D399", // Emerald
        "#FBBF24", // Amber
        "#FB7185", // Rose
        "#818CF8", // Indigo
        "#2DD4BF", // Teal
        "#FB923C"  // Orange
    };

    [[nodiscard]] constexpr bool isValidGroupColor(const std::string_view color) noexcept
    {
        if (color.empty())
        {
            return true;
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

    [[nodiscard]] constexpr std::string_view defaultGroupColorForId(const std::string_view groupId) noexcept
    {
        std::uint64_t hash = 14695981039346656037ULL;
        for (const char c : groupId)
        {
            hash ^= static_cast<unsigned char>(c);
            hash *= 1099511628211ULL;
        }
        return kDefaultGroupPalette[hash % kDefaultGroupPalette.size()];
    }

    struct Group
    {
        GroupId id{};
        std::string name{};
        std::string color{}; // Optional "#RRGGBB" custom color; empty uses deterministic palette fallback
        std::vector<repositories::bo::RepositoryId> repositoryIds{}; // ordered list
        std::int64_t createdAtUnixEpochMs = 0;
        std::int64_t updatedAtUnixEpochMs = 0;

        [[nodiscard]] bool isValid() const noexcept
        {
            return !id.empty() && !name.empty();
        }

        [[nodiscard]] std::string_view resolvedColor() const noexcept
        {
            if (!color.empty() && isValidGroupColor(color))
            {
                return color;
            }
            return defaultGroupColorForId(id.value());
        }

        [[nodiscard]] bool contains(const repositories::bo::RepositoryId& repoId) const noexcept
        {
            return std::ranges::find(repositoryIds, repoId) != repositoryIds.end();
        }

        auto operator<=>(const Group&) const = default;
    };
} // namespace gf::groups::bo
