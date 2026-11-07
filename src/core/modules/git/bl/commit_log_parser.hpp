#pragma once

#include "core/modules/git/bo/history_models.hpp"

#include <string_view>
#include <vector>

namespace gf::git::bl {

class CommitLogParser {
public:
    /// High-performance deterministic parsing of git log output formatted with NUL (\0) and 0x1E record separators.
    [[nodiscard]] static std::vector<bo::GitCommitLogItem> parse(std::string_view rawOutput);
};

} // namespace gf::git::bl
