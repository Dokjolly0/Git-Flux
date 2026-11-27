#include <catch2/catch_test_macros.hpp>

#include "core/modules/terminal/bl/ansi_parser.hpp"
#include "core/modules/terminal/bl/command_line_tokenizer.hpp"
#include "core/modules/terminal/bl/git_catalog_service.hpp"
#include "core/modules/terminal/bl/git_completion_engine.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace gf::terminal::bl;
using namespace gf::terminal::bo;

namespace
{
    [[nodiscard]] bool containsItem(const CompletionResult& res, const std::string_view text)
    {
        return std::ranges::any_of(res.items, [&](const CompletionItem& item)
        {
            return item.displayText == text || item.insertText == text;
        });
    }

    [[nodiscard]] int indexOfItem(const CompletionResult& res, const std::string_view text)
    {
        for (int i = 0; i < static_cast<int>(res.items.size()); ++i)
        {
            if (res.items[static_cast<std::size_t>(i)].displayText == text ||
                res.items[static_cast<std::size_t>(i)].insertText == text)
            {
                return i;
            }
        }
        return -1;
    }
} // namespace

TEST_CASE("AnsiParser parses standard, bright, 256-color and TrueColor SGR codes", "[terminal][ansi]")
{
    SECTION("Basic red/green and reset")
    {
        const auto line = AnsiParser::parseLine("\x1b[31m-old line\x1b[0m \x1b[1;32m+new line\x1b[0m");
        REQUIRE(line.plainText == "-old line +new line");
        REQUIRE(line.spans.size() == 3);
        REQUIRE(line.spans[0].text == "-old line");
        REQUIRE(line.spans[0].style.fgColor == "#F87171");
        REQUIRE_FALSE(line.spans[0].style.bold);
        REQUIRE(line.spans[1].text == " ");
        REQUIRE(line.spans[1].style.fgColor.empty());
        REQUIRE(line.spans[2].text == "+new line");
        REQUIRE(line.spans[2].style.fgColor == "#34D399");
        REQUIRE(line.spans[2].style.bold);
        REQUIRE(line.richHtml.find("color:#F87171;") != std::string::npos);
        REQUIRE(line.richHtml.find("font-weight:700;") != std::string::npos);
    }

    SECTION("256-color and 24-bit TrueColor")
    {
        const auto line = AnsiParser::parseLine("\x1b[38;5;196mred256\x1b[38;2;16;32;48mrgb\x1b[0m");
        REQUIRE(line.plainText == "red256rgb");
        REQUIRE(line.spans.size() == 2);
        REQUIRE(line.spans[0].style.fgColor == "#FF0000");
        REQUIRE(line.spans[1].style.fgColor == "#102030");
    }

    SECTION("Background SGR codes use muted dark palette instead of bright foreground colors")
    {
        const auto line = AnsiParser::parseLine("\x1b[44;1massets\x1b[0m");
        REQUIRE(line.plainText == "assets");
        REQUIRE(line.spans.size() == 1);
        REQUIRE(line.spans[0].style.bold);
        REQUIRE(line.spans[0].style.bgColor == "#122B4A");
    }

    SECTION("Carriage return overwrite for progress lines")
    {
        const auto line = AnsiParser::parseLine("Receiving objects: 10%\rReceiving objects: 65%\rReceiving objects: 100%");
        REQUIRE(line.plainText == "Receiving objects: 100%");
    }

    SECTION("HTML escaping preserves indentation and special chars")
    {
        const auto line = AnsiParser::parseLine("  <tag & \"attr\">");
        REQUIRE(line.plainText == "  <tag & \"attr\">");
        REQUIRE(line.richHtml.find("&nbsp;&nbsp;&lt;tag &amp; &quot;attr&quot;&gt;") != std::string::npos);
    }
}

TEST_CASE("CommandLineTokenizer handles quotes, cursor position and chained commands", "[terminal][tokenizer]")
{
    SECTION("Simple prefix at end")
    {
        const auto seg = CommandLineTokenizer::parseActiveSegment("git c", 5);
        REQUIRE(seg.tokens.size() == 2);
        REQUIRE(seg.tokens[0].value == "git");
        REQUIRE(seg.tokens[1].value == "c");
        REQUIRE(seg.activeTokenIndex == 1);
        REQUIRE(seg.activeReplaceStart == 4);
        REQUIRE(seg.activeReplaceLength == 1);
        REQUIRE(seg.activePrefix == "c");
    }

    SECTION("Trailing space starts new empty token")
    {
        const auto seg = CommandLineTokenizer::parseActiveSegment("git checkout ", 13);
        REQUIRE(seg.tokens.size() == 2);
        REQUIRE(seg.activeTokenIndex == 2);
        REQUIRE(seg.activeReplaceStart == 13);
        REQUIRE(seg.activeReplaceLength == 0);
        REQUIRE(seg.activePrefix.empty());
        REQUIRE(seg.endsWithSpace);
    }

    SECTION("Chained command after && isolates active git segment")
    {
        const std::string line = "git add . && git co";
        const auto seg = CommandLineTokenizer::parseActiveSegment(line, line.size());
        REQUIRE(seg.tokens.size() == 2);
        REQUIRE(seg.tokens[0].value == "git");
        REQUIRE(seg.tokens[1].value == "co");
        REQUIRE(seg.activeTokenIndex == 1);
        REQUIRE(seg.activeReplaceStart == 17);
        REQUIRE(seg.activeReplaceLength == 2);
    }

    SECTION("trySplitSimpleArgs rejects shell metacharacters")
    {
        std::vector<std::string> args;
        REQUIRE(CommandLineTokenizer::trySplitSimpleArgs("git commit -m \"hello world\"", args));
        REQUIRE(args == std::vector<std::string>{"git", "commit", "-m", "hello world"});

        REQUIRE_FALSE(CommandLineTokenizer::trySplitSimpleArgs("git log | grep fix", args));
        REQUIRE_FALSE(CommandLineTokenizer::trySplitSimpleArgs("git add . && git status", args));
    }
}

TEST_CASE("GitCatalogService parses git help -a, aliases, and subcommand options", "[terminal][catalog]")
{
    SECTION("parseHelpAllOutput extracts all categories and ignores guides")
    {
        const std::string sample = R"(See 'git help <command>' to read about a specific subcommand

Main Porcelain Commands
   checkout                Switch branches or restore working tree files
   commit                  Record changes to the repository

Ancillary Commands / Manipulators
   config                  Get and set repository or global options

Low-level Commands / Interrogators
   cat-file                Provide contents or details of repository objects

User-facing repository, command and file interfaces
   attributes              Defining attributes per path

External commands
   lfs
)";
        const auto cmds = GitCatalogService::parseHelpAllOutput(sample);
        REQUIRE(cmds.size() == 5);
        REQUIRE(cmds[0].name == "checkout");
        REQUIRE(cmds[0].category == GitCommandCategory::MainPorcelain);
        REQUIRE(cmds[2].name == "config");
        REQUIRE(cmds[2].category == GitCommandCategory::AncillaryManipulator);
        REQUIRE(cmds[3].name == "cat-file");
        REQUIRE(cmds[3].category == GitCommandCategory::LowLevelInterrogator);
        REQUIRE(cmds[4].name == "lfs");
        REQUIRE(cmds[4].category == GitCommandCategory::External);
    }

    SECTION("parseAliasesOutput extracts user aliases and expansions")
    {
        const std::string sample =
            "alias.st status -sb\n"
            "alias.co checkout\n"
            "alias.lg log --oneline --graph --decorate\n";
        const auto aliases = GitCatalogService::parseAliasesOutput(sample);
        REQUIRE(aliases.size() == 3);
        REQUIRE(aliases[0].name == "st");
        REQUIRE(aliases[0].category == GitCommandCategory::Alias);
        REQUIRE(aliases[0].description == "Alias: status -sb");
        REQUIRE(aliases[2].name == "lg");
    }

    SECTION("parseSubcommandHelpOptions parses short, long, negatable and wrapped descriptions")
    {
        const std::string sample = R"(usage: git commit [<options>]

    -q, --[no-]quiet      suppress summary after successful commit
    -m, --[no-]message <message>
                          commit message
    --amend               amend previous commit
)";
        const auto opts = GitCatalogService::parseSubcommandHelpOptions(sample);
        REQUIRE(opts.size() == 3);
        REQUIRE(opts[0].shortFlag == "-q");
        REQUIRE(opts[0].longFlag == "--quiet");
        REQUIRE(opts[0].isNegatable);
        REQUIRE(opts[1].shortFlag == "-m");
        REQUIRE(opts[1].longFlag == "--message");
        REQUIRE(opts[1].takesValue);
        REQUIRE(opts[1].description == "commit message");
        REQUIRE(opts[2].longFlag == "--amend");
        REQUIRE_FALSE(opts[2].isNegatable);
    }

    SECTION("parseCompletionHelperOptions extracts flags and value flags")
    {
        const auto opts = GitCatalogService::parseCompletionHelperOptions("--amend --message= --no-verify --");
        REQUIRE(opts.size() == 3);
        REQUIRE(opts[0].longFlag == "--amend");
        REQUIRE_FALSE(opts[0].takesValue);
        REQUIRE(opts[1].longFlag == "--message");
        REQUIRE(opts[1].takesValue);
        REQUIRE(opts[2].longFlag == "--no-verify");
    }
}

TEST_CASE("GitCompletionEngine progressively filters all Git commands, flags and repo context", "[terminal][completion]")
{
    GitCatalogService catalog;
    catalog.setCommandsForTesting({
        {"checkout", "Switch branches or restore working tree files", GitCommandCategory::MainPorcelain},
        {"cherry-pick", "Apply the changes introduced by some existing commits", GitCommandCategory::MainPorcelain},
        {"clean", "Remove untracked files from the working tree", GitCommandCategory::MainPorcelain},
        {"clone", "Clone a repository into a new directory", GitCommandCategory::MainPorcelain},
        {"commit", "Record changes to the repository", GitCommandCategory::MainPorcelain},
        {"co", "Alias: checkout", GitCommandCategory::Alias},
        {"config", "Get and set repository or global options", GitCommandCategory::AncillaryManipulator},
        {"cat-file", "Provide contents or details of repository objects", GitCommandCategory::LowLevelInterrogator},
        {"check-ignore", "Debug gitignore / exclude files", GitCommandCategory::LowLevelInterrogator},
        {"checkout-index", "Copy files from the index to the working tree", GitCommandCategory::LowLevelManipulator},
        {"status", "Show the working tree status", GitCommandCategory::MainPorcelain}
    });
    catalog.setCachedOptionsFor("commit", {
        {"-a", "--all", "", "commit all changed files", false, true},
        {"-m", "--message", "<message>", "commit message", true, false},
        {"", "--amend", "", "amend previous commit", false, false},
        {"", "--verify", "", "run pre-commit hooks", false, true}
    });

    RepoCompletionSnapshot snap;
    snap.currentBranch = "main";
    snap.localBranches = {"main", "feature/cli-terminal", "develop"};
    snap.remoteBranches = {"origin/main", "origin/develop"};
    snap.remotes = {"origin", "upstream"};
    snap.tags = {"v1.0.0"};
    snap.stashes = {{"stash@{0}", "WIP on main: terminal work"}};
    snap.modifiedFiles = {"src/main.cpp", "README.md"};
    snap.untrackedFiles = {"notes.txt"};

    SECTION("`git c` returns ALL c* commands (porcelain, alias, ancillary, plumbing) ranked by category")
    {
        const auto res = GitCompletionEngine::complete("git c", 5, {}, catalog, snap);
        REQUIRE(res.items.size() == 10); // all 10 commands starting with 'c', NOT 'status'
        REQUIRE_FALSE(containsItem(res, "status"));
        REQUIRE(containsItem(res, "checkout"));
        REQUIRE(containsItem(res, "cherry-pick"));
        REQUIRE(containsItem(res, "commit"));
        REQUIRE(containsItem(res, "co"));
        REQUIRE(containsItem(res, "config"));
        REQUIRE(containsItem(res, "cat-file"));
        REQUIRE(containsItem(res, "check-ignore"));
        REQUIRE(containsItem(res, "checkout-index"));

        // Porcelain ranked before Alias, Alias before Ancillary, Ancillary before Plumbing
        REQUIRE(indexOfItem(res, "checkout") < indexOfItem(res, "co"));
        REQUIRE(indexOfItem(res, "co") < indexOfItem(res, "config"));
        REQUIRE(indexOfItem(res, "config") < indexOfItem(res, "cat-file"));
    }

    SECTION("Progressive letter-by-letter narrowing: git c -> git ch -> git checko")
    {
        const auto r1 = GitCompletionEngine::complete("git ch", 6, {}, catalog, snap);
        REQUIRE(containsItem(r1, "checkout"));
        REQUIRE(containsItem(r1, "cherry-pick"));
        REQUIRE(containsItem(r1, "check-ignore"));
        REQUIRE(containsItem(r1, "checkout-index"));
        REQUIRE_FALSE(containsItem(r1, "commit"));
        REQUIRE_FALSE(containsItem(r1, "cat-file"));

        const auto r2 = GitCompletionEngine::complete("git checko", 10, {}, catalog, snap);
        REQUIRE(r2.items.size() == 2);
        REQUIRE(r2.items[0].displayText == "checkout");
        REQUIRE(r2.items[1].displayText == "checkout-index");
    }

    SECTION("Substring fallback activates when zero prefix matches exist")
    {
        const auto res = GitCompletionEngine::complete("git pick", 8, {}, catalog, snap);
        REQUIRE(res.items.size() == 1);
        REQUIRE(res.items[0].displayText == "cherry-pick");
        REQUIRE(res.items[0].matchStart == 7);
        REQUIRE(res.items[0].matchLength == 4);
    }

    SECTION("Subcommand flags including negated --no-* flags")
    {
        const auto r1 = GitCompletionEngine::complete("git commit --a", 14, {}, catalog, snap);
        REQUIRE(containsItem(r1, "--all"));
        REQUIRE(containsItem(r1, "--amend"));
        REQUIRE_FALSE(containsItem(r1, "--message"));

        const auto r2 = GitCompletionEngine::complete("git commit --no-v", 17, {}, catalog, snap);
        REQUIRE(containsItem(r2, "--no-verify"));
    }

    SECTION("Sub-subcommands and stashes")
    {
        const auto r1 = GitCompletionEngine::complete("git stash p", 11, {}, catalog, snap);
        REQUIRE(containsItem(r1, "pop"));
        REQUIRE(containsItem(r1, "push"));

        const auto r2 = GitCompletionEngine::complete("git stash pop ", 14, {}, catalog, snap);
        REQUIRE(containsItem(r2, "stash@{0}"));
    }

    SECTION("Context-aware branches, remotes, and working tree files")
    {
        const auto brRes = GitCompletionEngine::complete("git checkout feat", 17, {}, catalog, snap);
        REQUIRE(containsItem(brRes, "feature/cli-terminal"));

        const auto remRes = GitCompletionEngine::complete("git push ", 9, {}, catalog, snap);
        REQUIRE(containsItem(remRes, "origin"));
        REQUIRE(containsItem(remRes, "upstream"));

        const auto pushBranchRes = GitCompletionEngine::complete("git push origin f", 17, {}, catalog, snap);
        REQUIRE(containsItem(pushBranchRes, "feature/cli-terminal"));

        const auto addRes = GitCompletionEngine::complete("git add ", 8, {}, catalog, snap);
        REQUIRE(containsItem(addRes, "."));
        REQUIRE(containsItem(addRes, "src/main.cpp"));
        REQUIRE(containsItem(addRes, "notes.txt"));
    }

    SECTION("Chained command autocompletes after &&")
    {
        const std::string line = "git add . && git co";
        const auto res = GitCompletionEngine::complete(line, line.size(), {}, catalog, snap);
        REQUIRE(containsItem(res, "commit"));
        REQUIRE(containsItem(res, "config"));
        REQUIRE(containsItem(res, "co"));
        REQUIRE(res.replaceStart == 17);
        REQUIRE(res.replaceLength == 2);
    }

    SECTION("Filesystem directories and files autocomplete at token 0 and with Windows backslashes")
    {
        const auto cwd = std::filesystem::current_path();
        if (std::filesystem::exists(cwd / "scripts"))
        {
            // Bare `sc` at position 0 suggests `scripts/`
            const auto r1 = GitCompletionEngine::complete("sc", 2, cwd, catalog, snap);
            REQUIRE(containsItem(r1, "scripts/"));

            // Windows backslash prefix `.\sc` preserves separator and suggests `.\scripts\`
            const auto r2 = GitCompletionEngine::complete(".\\sc", 4, cwd, catalog, snap);
            REQUIRE(containsItem(r2, ".\\scripts\\"));

            // `cd sc` suggests `scripts/`
            const auto r3 = GitCompletionEngine::complete("cd sc", 5, cwd, catalog, snap);
            REQUIRE(containsItem(r3, "scripts/"));
        }
    }
}
