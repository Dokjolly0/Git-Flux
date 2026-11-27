#pragma once

#include "core/modules/terminal/bo/script_completion_models.hpp"

#include <filesystem>
#include <string_view>

namespace gf::terminal::bl
{
    class ScriptParameterAnalyzer
    {
    public:
        /// Detects the script language from file extension or optional first-line shebang.
        [[nodiscard]] static bo::ScriptKind detectScriptKind(const std::filesystem::path& path,
                                                             std::string_view content = {});

        /// Statically inspects a script file on disk (never executes it!) with mtime-validated caching.
        /// Reads at most 256 KB to keep completion instantaneous.
        [[nodiscard]] static bo::ScriptMetadata analyzeFileCached(const std::filesystem::path& path);

        /// Pure static parsers (unit-testable without filesystem access).
        [[nodiscard]] static bo::ScriptMetadata parsePowerShell(std::string_view content);
        [[nodiscard]] static bo::ScriptMetadata parsePosixShell(std::string_view content);
        [[nodiscard]] static bo::ScriptMetadata parseBatch(std::string_view content);
        [[nodiscard]] static bo::ScriptMetadata parsePython(std::string_view content);
    };
} // namespace gf::terminal::bl
