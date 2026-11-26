#pragma once

#include "core/modules/git/bo/git_status.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace gf::git::bl
{
    class PorcelainStatusParser
    {
    public:
        /// Parses the output of `git status --porcelain=v2 -z --branch`.
        [[nodiscard]] static std::expected<bo::GitRepositoryStatus, std::string>
        parse(std::string_view rawOutput);
    };
} // namespace gf::git::bl
