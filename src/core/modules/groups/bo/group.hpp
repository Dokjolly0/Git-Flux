#pragma once

#include "core/modules/groups/bo/group_id.hpp"
#include "core/modules/repositories/bo/repository_id.hpp"
#include "core/shared/accent_palette.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace gf::groups::bo
{
    /// Curated dark-theme accent palette for repository groups (Linear / Arc inspired).
    inline constexpr auto& kDefaultGroupPalette = gf::core::kAccentPalette;

    [[nodiscard]] constexpr bool isValidGroupColor(const std::string_view color) noexcept
    {
        return gf::core::isValidHexColor(color, true);
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
