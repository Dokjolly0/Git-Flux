#pragma once

#include "core/modules/git/bo/history_models.hpp"

#include <string_view>
#include <vector>

namespace gf::git::bl {

class CommitLogParser {
public:
    /// Parsing deterministico ad alte prestazioni dell'output di git log formattato con separatori NUL (\0) e record 0x1E.
    [[nodiscard]] static std::vector<bo::GitCommitLogItem> parse(std::string_view rawOutput);
};

} // namespace gf::git::bl
