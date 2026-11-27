#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace gf::terminal::bo
{
    enum class GitCommandCategory
    {
        MainPorcelain,
        Alias,
        AncillaryManipulator,
        AncillaryInterrogator,
        InteractingWithOthers,
        External,
        LowLevelManipulator,
        LowLevelInterrogator,
        LowLevelSync,
        LowLevelHelper
    };

    [[nodiscard]] inline int categoryPriority(const GitCommandCategory category) noexcept
    {
        switch (category)
        {
        case GitCommandCategory::MainPorcelain: return 0;
        case GitCommandCategory::Alias: return 1;
        case GitCommandCategory::AncillaryManipulator: return 2;
        case GitCommandCategory::AncillaryInterrogator: return 3;
        case GitCommandCategory::InteractingWithOthers: return 4;
        case GitCommandCategory::External: return 5;
        case GitCommandCategory::LowLevelManipulator: return 6;
        case GitCommandCategory::LowLevelInterrogator: return 7;
        case GitCommandCategory::LowLevelSync: return 8;
        case GitCommandCategory::LowLevelHelper: return 9;
        }
        return 10;
    }

    [[nodiscard]] inline std::string_view categoryBadgeLabel(const GitCommandCategory category) noexcept
    {
        switch (category)
        {
        case GitCommandCategory::MainPorcelain: return "porcelain";
        case GitCommandCategory::Alias: return "alias";
        case GitCommandCategory::AncillaryManipulator:
        case GitCommandCategory::AncillaryInterrogator: return "ancillary";
        case GitCommandCategory::InteractingWithOthers: return "interop";
        case GitCommandCategory::External: return "external";
        case GitCommandCategory::LowLevelManipulator:
        case GitCommandCategory::LowLevelInterrogator:
        case GitCommandCategory::LowLevelSync:
        case GitCommandCategory::LowLevelHelper: return "plumbing";
        }
        return "git";
    }

    struct GitCommandSpec
    {
        std::string name;
        std::string description;
        GitCommandCategory category = GitCommandCategory::MainPorcelain;
    };

    struct GitOptionSpec
    {
        std::string shortFlag;   // e.g. "-m"
        std::string longFlag;    // e.g. "--message"
        std::string valueHint;   // e.g. "<message>"
        std::string description; // e.g. "commit message"
        bool takesValue = false;
        bool isNegatable = false; // e.g. --[no-]verify
    };

    enum class CompletionItemKind
    {
        GitCommand,
        GitSubcommand,
        GitAlias,
        GitFlag,
        Branch,
        Remote,
        Tag,
        Stash,
        Commit,
        FilePath,
        ShellBuiltin,
        ExecuteAction
    };

    [[nodiscard]] inline std::string_view completionKindToString(const CompletionItemKind kind) noexcept
    {
        switch (kind)
        {
        case CompletionItemKind::GitCommand: return "command";
        case CompletionItemKind::GitSubcommand: return "subcommand";
        case CompletionItemKind::GitAlias: return "alias";
        case CompletionItemKind::GitFlag: return "flag";
        case CompletionItemKind::Branch: return "branch";
        case CompletionItemKind::Remote: return "remote";
        case CompletionItemKind::Tag: return "tag";
        case CompletionItemKind::Stash: return "stash";
        case CompletionItemKind::Commit: return "commit";
        case CompletionItemKind::FilePath: return "file";
        case CompletionItemKind::ShellBuiltin: return "builtin";
        case CompletionItemKind::ExecuteAction: return "execute";
        }
        return "command";
    }

    struct CompletionItem
    {
        std::string insertText;
        std::string displayText;
        std::string detail;
        std::string categoryLabel;
        CompletionItemKind kind = CompletionItemKind::GitCommand;
        std::size_t matchStart = 0;
        std::size_t matchLength = 0;
        bool appendSpace = true;
        int score = 0;
    };

    struct CompletionResult
    {
        std::vector<CompletionItem> items;
        std::size_t replaceStart = 0;
        std::size_t replaceLength = 0;
        std::string activePrefix;
        std::string activeSubcommand; // non-empty if we are inside `git <subcommand> ...`
        bool needsSubcommandOptionsLoad = false;
    };

    struct RepoCommitHint
    {
        std::string shortSha;
        std::string summary;
    };

    struct RepoStashHint
    {
        std::string selector; // e.g. "stash@{0}"
        std::string message;
    };

    struct RepoCompletionSnapshot
    {
        std::string currentBranch;
        std::vector<std::string> localBranches;
        std::vector<std::string> remoteBranches;
        std::vector<std::string> remotes;
        std::vector<std::string> tags;
        std::vector<RepoStashHint> stashes;
        std::vector<std::string> modifiedFiles;
        std::vector<std::string> untrackedFiles;
        std::vector<RepoCommitHint> recentCommits;
    };
} // namespace gf::terminal::bo
