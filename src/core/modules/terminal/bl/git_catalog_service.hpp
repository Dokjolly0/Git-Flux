#pragma once

#include "core/modules/git/ports/git_command_executor.hpp"
#include "core/modules/terminal/bo/git_completion_models.hpp"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gf::terminal::bl
{
    class GitCatalogService
    {
    public:
        explicit GitCatalogService(gf::git::ports::IGitCommandExecutor* executor = nullptr);

        /// Runs dynamic discovery (`git help -a`, `git --list-cmds`, `git config --get-regexp ^alias\.`)
        /// and merges results with the built-in fallback catalog. Thread-safe and idempotent.
        void discoverAllCommands(const std::filesystem::path& repoPath = {});

        /// Refreshes global + repository-local Git aliases for the given repository path.
        void refreshAliases(const std::filesystem::path& repoPath);

        /// Returns whether dynamic discovery has completed at least once.
        [[nodiscard]] bool isDiscovered() const noexcept { return m_discovered.load(std::memory_order_acquire); }

        /// Returns a snapshot of all known Git commands (Porcelain, Ancillary, External, Plumbing, Aliases).
        [[nodiscard]] std::vector<bo::GitCommandSpec> allCommands() const;

        /// Returns global options accepted directly after `git` (before the subcommand).
        [[nodiscard]] static const std::vector<bo::GitOptionSpec>& globalOptions();

        /// Returns curated sub-subcommands for multi-verb Git commands (e.g. `stash`, `remote`, `worktree`, `bisect`, `lfs`).
        [[nodiscard]] static std::vector<bo::GitCommandSpec> subSubcommandsFor(std::string_view subcommand);

        /// Instant cache lookup for a subcommand's flags/options (returns std::nullopt if not yet loaded).
        [[nodiscard]] std::optional<std::vector<bo::GitOptionSpec>> cachedOptionsFor(std::string_view subcommand) const;

        /// Discovers (via `git <cmd> -h` + `--git-completion-helper`), caches, and returns all options for `subcommand`.
        [[nodiscard]] std::vector<bo::GitOptionSpec> loadOptionsFor(std::string_view subcommand);

        /// Seeds options for a subcommand directly (useful for tests).
        void setCachedOptionsFor(std::string subcommand, std::vector<bo::GitOptionSpec> options);

        /// Seeds the full command catalog directly (useful for tests).
        void setCommandsForTesting(std::vector<bo::GitCommandSpec> commands);

        // --- Pure static parsers (unit-testable without spawning Git) ---

        [[nodiscard]] static std::vector<bo::GitCommandSpec> parseHelpAllOutput(std::string_view output);
        [[nodiscard]] static std::vector<std::string> parseListCmdsOutput(std::string_view output);
        [[nodiscard]] static std::vector<bo::GitCommandSpec> parseAliasesOutput(std::string_view output);
        [[nodiscard]] static std::vector<bo::GitOptionSpec> parseSubcommandHelpOptions(std::string_view output);
        [[nodiscard]] static std::vector<bo::GitOptionSpec> parseCompletionHelperOptions(std::string_view output);
        [[nodiscard]] static std::vector<bo::GitCommandSpec> builtinFallbackCommands();

    private:
        gf::git::ports::IGitCommandExecutor* m_executor{nullptr};
        std::atomic<bool> m_discovered{false};

        mutable std::mutex m_mutex;
        std::vector<bo::GitCommandSpec> m_baseCommands;
        std::vector<bo::GitCommandSpec> m_aliases;
        std::unordered_map<std::string, std::vector<bo::GitOptionSpec>> m_optionsCache;
    };
} // namespace gf::terminal::bl
