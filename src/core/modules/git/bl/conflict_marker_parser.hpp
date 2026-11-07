#pragma once

#include "core/modules/git/bo/conflict_models.hpp"

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace gf::git::bl {

class ConflictMarkerParser {
public:
    /// Esegue il parsing di una stringa con marker git (<<<<<<<, |||||||, =======, >>>>>>>).
    [[nodiscard]] static std::vector<bo::ConflictHunk>
    parseHunks(std::string_view content);

    /// Ricostruisce il file completo sostituendo ogni hunk con la relativa risoluzione.
    [[nodiscard]] static std::expected<std::string, std::string>
    reconstruct(std::string_view originalContent, const std::vector<bo::ConflictHunk>& hunks);

    /// Verifica rapida se il testo contiene marker di conflitto Git validi.
    [[nodiscard]] static bool
    hasConflictMarkers(std::string_view content) noexcept;
};

} // namespace gf::git::bl
