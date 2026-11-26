#pragma once

#include "core/modules/git/bo/diff_models.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace gf::git::bl
{
    class UnifiedDiffParser
    {
    public:
        /// Parses the output of a `git diff` command containing one or more unified file diffs.
        [[nodiscard]] static std::vector<bo::FileDiff> parse(std::string_view diffOutput);

        /// Parses the diff output for a single file. Returns nullopt if the output is empty.
        [[nodiscard]] static std::optional<bo::FileDiff> parseSingle(std::string_view diffOutput);
    };
} // namespace gf::git::bl
