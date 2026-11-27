#pragma once

#include "core/modules/terminal/bl/git_catalog_service.hpp"
#include "core/modules/terminal/bo/git_completion_models.hpp"
#include "core/modules/terminal/bo/script_completion_models.hpp"

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace gf::terminal::bl
{
    class GitCompletionEngine
    {
    public:
        /// Computes ranked completions for `inputLine` at `cursorPos`.
        /// Pure, fast, and non-blocking: uses cached Git catalog/options, cached static script
        /// parameter analysis (OS-aware), and `repoSnapshot`.
        /// If subcommand options are not yet cached, sets `result.needsSubcommandOptionsLoad = true`.
        [[nodiscard]] static bo::CompletionResult complete(std::string_view inputLine,
                                                           std::size_t cursorPos,
                                                           const std::filesystem::path& workingDir,
                                                           const GitCatalogService& catalog,
                                                           const bo::RepoCompletionSnapshot& repoSnapshot,
                                                           bool explicitTrigger = false,
                                                           bo::TargetOs targetOs = bo::currentTargetOs());
    };
} // namespace gf::terminal::bl
