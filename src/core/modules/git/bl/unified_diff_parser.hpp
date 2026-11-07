#pragma once

#include "core/modules/git/bo/diff_models.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace gf::git::bl {

class UnifiedDiffParser {
public:
    /// Parsea l'output di un comando `git diff` contenente uno o più file diff unificati.
    [[nodiscard]] static std::vector<bo::FileDiff> parse(std::string_view diffOutput);

    /// Parsea l'output di un diff relativo ad un singolo file. Ritorna nullopt se l'output è vuoto.
    [[nodiscard]] static std::optional<bo::FileDiff> parseSingle(std::string_view diffOutput);
};

} // namespace gf::git::bl
