#pragma once

#include "core/modules/git/bo/conflict_models.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace gf::git::bl
{
    class ConflictMarkerParser
    {
    public:
        /// Parses a string containing git conflict markers (<<<<<<<, |||||||, =======, >>>>>>>).
        [[nodiscard]] static std::vector<bo::ConflictHunk>
        parseHunks(std::string_view content);

        /// Reconstructs the complete file replacing each hunk with its chosen resolution.
        [[nodiscard]] static std::expected<std::string, std::string>
        reconstruct(std::string_view originalContent, const std::vector<bo::ConflictHunk>& hunks);

        /// Fast check to verify whether text contains valid Git conflict markers.
        [[nodiscard]] static bool
        hasConflictMarkers(std::string_view content) noexcept;
    };
} // namespace gf::git::bl
