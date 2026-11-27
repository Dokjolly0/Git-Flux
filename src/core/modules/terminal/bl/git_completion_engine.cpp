#include "core/modules/terminal/bl/git_completion_engine.hpp"
#include "core/modules/terminal/bl/command_line_tokenizer.hpp"
#include "core/shared/utf8_path.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace gf::terminal::bl
{
    namespace
    {
        [[nodiscard]] std::string toLower(const std::string_view sv)
        {
            std::string out;
            out.reserve(sv.size());
            for (const char c : sv)
            {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        }

        [[nodiscard]] bool startsWithCaseInsensitive(const std::string_view text, const std::string_view prefix) noexcept
        {
            if (prefix.size() > text.size()) return false;
            for (std::size_t i = 0; i < prefix.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(text[i])) !=
                    std::tolower(static_cast<unsigned char>(prefix[i])))
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] std::size_t findCaseInsensitive(const std::string_view text, const std::string_view needle)
        {
            if (needle.empty()) return 0;
            const std::string lt = toLower(text);
            const std::string ln = toLower(needle);
            return lt.find(ln);
        }

        [[nodiscard]] bool globalOptionTakesSeparateArg(const std::string_view tok) noexcept
        {
            return tok == "-C" || tok == "-c" || tok == "--git-dir" ||
                   tok == "--work-tree" || tok == "--namespace" || tok == "--config-env";
        }

        [[nodiscard]] bool isRemoteCommand(const std::string_view cmd) noexcept
        {
            return cmd == "push" || cmd == "pull" || cmd == "fetch" || cmd == "ls-remote";
        }

        [[nodiscard]] bool isBranchRefCommand(const std::string_view cmd) noexcept
        {
            return cmd == "checkout" || cmd == "switch" || cmd == "merge" || cmd == "rebase" ||
                   cmd == "branch" || cmd == "reset" || cmd == "diff" || cmd == "log" ||
                   cmd == "show" || cmd == "cherry-pick" || cmd == "revert" || cmd == "tag" ||
                   cmd == "describe" || cmd == "shortlog" || cmd == "range-diff" ||
                   cmd == "rev-parse" || cmd == "rev-list" || cmd == "merge-base" ||
                   cmd == "cherry" || cmd == "archive";
        }

        [[nodiscard]] bool isFileCommand(const std::string_view cmd) noexcept
        {
            return cmd == "add" || cmd == "restore" || cmd == "rm" || cmd == "mv" ||
                   cmd == "blame" || cmd == "diff" || cmd == "checkout" || cmd == "reset" ||
                   cmd == "log" || cmd == "check-ignore" || cmd == "check-attr" || cmd == "grep";
        }

        void addFilesystemCompletions(const std::filesystem::path& workingDir,
                                      const std::string_view prefix,
                                      const bool directoriesOnly,
                                      std::vector<bo::CompletionItem>& out,
                                      std::unordered_set<std::string>& seen,
                                      const int baseScore)
        {
            if (workingDir.empty()) return;

            std::error_code ec;
            if (!std::filesystem::exists(workingDir, ec) || !std::filesystem::is_directory(workingDir, ec))
            {
                return;
            }

            // Split prefix into dir part and leaf prefix (preserve user's '/' or '\' separator style)
            const bool useBackslash = (prefix.find('\\') != std::string_view::npos);
            std::string normalized(prefix);
            std::ranges::replace(normalized, '\\', '/');

            std::string dirPrefix;
            std::string leafPrefix = normalized;
            if (const std::size_t slash = normalized.rfind('/'); slash != std::string::npos)
            {
                dirPrefix = normalized.substr(0, slash + 1);
                leafPrefix = normalized.substr(slash + 1);
            }

            std::filesystem::path searchDir = workingDir;
            if (!dirPrefix.empty())
            {
                const std::filesystem::path rel = gf::core::utf8ToPath(dirPrefix);
                searchDir = rel.is_absolute() ? rel : (workingDir / rel);
            }

            if (!std::filesystem::exists(searchDir, ec) || !std::filesystem::is_directory(searchDir, ec))
            {
                return;
            }

            constexpr std::size_t kMaxEntries = 80;
            std::size_t added = 0;

            for (const auto& entry : std::filesystem::directory_iterator(
                     searchDir, std::filesystem::directory_options::skip_permission_denied, ec))
            {
                if (added >= kMaxEntries) break;

                const std::string name = gf::core::pathToUtf8(entry.path().filename());
                if (name == ".git") continue;
                if (leafPrefix.empty() && !name.empty() && name.front() == '.') continue;

                const bool isDir = entry.is_directory(ec);
                if (directoriesOnly && !isDir) continue;

                if (!leafPrefix.empty() && !startsWithCaseInsensitive(name, leafPrefix))
                {
                    continue;
                }

                std::string candidate = dirPrefix + name;
                if (isDir)
                {
                    candidate.push_back('/');
                }
                if (useBackslash)
                {
                    std::ranges::replace(candidate, '/', '\\');
                }

                if (!seen.insert(candidate).second) continue;

                std::string insertText = candidate;
                if (insertText.find(' ') != std::string::npos)
                {
                    insertText = "\"" + insertText + "\"";
                }

                out.push_back(bo::CompletionItem{
                    .insertText = std::move(insertText),
                    .displayText = candidate,
                    .detail = isDir ? "Directory" : "File",
                    .categoryLabel = isDir ? "dir" : "file",
                    .kind = bo::CompletionItemKind::FilePath,
                    .matchStart = dirPrefix.size(),
                    .matchLength = leafPrefix.size(),
                    .appendSpace = !isDir,
                    .score = baseScore + (isDir ? 5 : 0)
                });
                ++added;
            }
        }

        void addOptionCompletions(const std::vector<bo::GitOptionSpec>& options,
                                  const std::string_view prefix,
                                  std::vector<bo::CompletionItem>& out)
        {
            const bool onlyLong = prefix.starts_with("--");
            const bool isSingleDash = (prefix == "-");

            std::unordered_set<std::string> seen;

            auto tryAdd = [&](const std::string& flag,
                              const std::string& valueHint,
                              const std::string& desc,
                              const bool takesValue,
                              const int bonusScore)
            {
                if (flag.empty()) return;
                if (!startsWithCaseInsensitive(flag, prefix)) return;
                if (!seen.insert(flag).second) return;

                std::string display = flag;
                if (!valueHint.empty())
                {
                    if (valueHint.front() != '=' && valueHint.front() != '[')
                    {
                        display.push_back(' ');
                    }
                    display.append(valueHint);
                }

                std::string insert = flag;
                bool appendSpace = true;
                if (takesValue && !valueHint.empty() && valueHint.front() == '=')
                {
                    insert.push_back('=');
                    appendSpace = false;
                }

                out.push_back(bo::CompletionItem{
                    .insertText = std::move(insert),
                    .displayText = std::move(display),
                    .detail = desc,
                    .categoryLabel = "flag",
                    .kind = bo::CompletionItemKind::GitFlag,
                    .matchStart = 0,
                    .matchLength = prefix.size(),
                    .appendSpace = appendSpace,
                    .score = 400 + bonusScore
                });
            };

            for (const auto& opt : options)
            {
                if (!onlyLong && !opt.shortFlag.empty())
                {
                    std::string desc = opt.description;
                    if (!opt.longFlag.empty())
                    {
                        desc = opt.longFlag + (desc.empty() ? "" : (" — " + desc));
                    }
                    tryAdd(opt.shortFlag, opt.valueHint, desc, opt.takesValue, 20);
                }

                if (!opt.longFlag.empty())
                {
                    // When typing just `-`, include long flags too so users discover all flags immediately
                    if (isSingleDash || onlyLong || startsWithCaseInsensitive(opt.longFlag, prefix))
                    {
                        tryAdd(opt.longFlag, opt.valueHint, opt.description, opt.takesValue, 10);
                    }
                    if (opt.isNegatable && opt.longFlag.size() > 2)
                    {
                        const std::string negFlag = "--no-" + opt.longFlag.substr(2);
                        if (startsWithCaseInsensitive(negFlag, prefix))
                        {
                            tryAdd(negFlag, "",
                                   opt.description.empty() ? "Negated option" : ("No: " + opt.description),
                                   false, 0);
                        }
                    }
                }
            }
        }

        void addCommandListCompletions(const std::vector<bo::GitCommandSpec>& commands,
                                       const std::string_view prefix,
                                       const bo::CompletionItemKind defaultKind,
                                       std::vector<bo::CompletionItem>& out)
        {
            std::vector<bo::CompletionItem> prefixMatches;
            std::vector<bo::CompletionItem> substringMatches;

            for (const auto& cmd : commands)
            {
                const int catPrio = bo::categoryPriority(cmd.category);
                const bo::CompletionItemKind kind =
                    (cmd.category == bo::GitCommandCategory::Alias) ? bo::CompletionItemKind::GitAlias : defaultKind;
                const std::string badge(bo::categoryBadgeLabel(cmd.category));

                if (prefix.empty() || startsWithCaseInsensitive(cmd.name, prefix))
                {
                    // Higher score = shown earlier. MainPorcelain (0) > Alias (1) > Ancillary (2-3) > External (5) > Plumbing (6-9)
                    const int exactBonus = (!prefix.empty() && cmd.name.size() == prefix.size()) ? 50 : 0;
                    prefixMatches.push_back(bo::CompletionItem{
                        .insertText = cmd.name,
                        .displayText = cmd.name,
                        .detail = cmd.description,
                        .categoryLabel = badge,
                        .kind = kind,
                        .matchStart = 0,
                        .matchLength = prefix.size(),
                        .appendSpace = true,
                        .score = 1000 - catPrio * 40 + exactBonus
                    });
                }
                else if (!prefix.empty())
                {
                    if (const std::size_t idx = findCaseInsensitive(cmd.name, prefix); idx != std::string::npos)
                    {
                        substringMatches.push_back(bo::CompletionItem{
                            .insertText = cmd.name,
                            .displayText = cmd.name,
                            .detail = cmd.description,
                            .categoryLabel = badge,
                            .kind = kind,
                            .matchStart = idx,
                            .matchLength = prefix.size(),
                            .appendSpace = true,
                            .score = 300 - catPrio * 20
                        });
                    }
                }
            }

            // Strict prefix matches whenever available (so `git c` shows ONLY commands starting with `c`);
            // fall back to substring matches only when zero commands match the prefix (e.g. `git pick` -> `cherry-pick`).
            auto& chosen = !prefixMatches.empty() ? prefixMatches : substringMatches;
            out.insert(out.end(), std::make_move_iterator(chosen.begin()), std::make_move_iterator(chosen.end()));
        }

        void addBranchAndRefCompletions(const bo::RepoCompletionSnapshot& snap,
                                        const std::string_view prefix,
                                        std::vector<bo::CompletionItem>& out,
                                        std::unordered_set<std::string>& seen,
                                        const bool includeRemoteBranches = true,
                                        const bool includeTagsAndCommits = true)
        {
            auto matchAndPush = [&](const std::string& val,
                                    const std::string& detail,
                                    const std::string& badge,
                                    const bo::CompletionItemKind kind,
                                    const int score)
            {
                if (val.empty()) return;
                if (!prefix.empty() && !startsWithCaseInsensitive(val, prefix)) return;
                if (!seen.insert(val).second) return;

                out.push_back(bo::CompletionItem{
                    .insertText = val,
                    .displayText = val,
                    .detail = detail,
                    .categoryLabel = badge,
                    .kind = kind,
                    .matchStart = 0,
                    .matchLength = prefix.size(),
                    .appendSpace = true,
                    .score = score
                });
            };

            for (const auto& br : snap.localBranches)
            {
                const bool isCurr = (br == snap.currentBranch);
                matchAndPush(br, isCurr ? "Branch corrente" : "Branch locale", "branch",
                             bo::CompletionItemKind::Branch, isCurr ? 620 : 600);
            }

            matchAndPush("HEAD", "Commit corrente (HEAD)", "ref", bo::CompletionItemKind::Branch, 560);

            if (includeRemoteBranches)
            {
                for (const auto& rbr : snap.remoteBranches)
                {
                    matchAndPush(rbr, "Branch remoto", "remote", bo::CompletionItemKind::Branch, 540);
                }
            }

            if (includeTagsAndCommits)
            {
                for (const auto& tag : snap.tags)
                {
                    matchAndPush(tag, "Tag", "tag", bo::CompletionItemKind::Tag, 500);
                }
                for (const auto& commit : snap.recentCommits)
                {
                    matchAndPush(commit.shortSha, commit.summary, "commit", bo::CompletionItemKind::Commit, 450);
                }
            }
        }

        void addRemoteCompletions(const bo::RepoCompletionSnapshot& snap,
                                  const std::string_view prefix,
                                  std::vector<bo::CompletionItem>& out,
                                  std::unordered_set<std::string>& seen)
        {
            for (const auto& remote : snap.remotes)
            {
                if (!prefix.empty() && !startsWithCaseInsensitive(remote, prefix)) continue;
                if (!seen.insert(remote).second) continue;

                out.push_back(bo::CompletionItem{
                    .insertText = remote,
                    .displayText = remote,
                    .detail = "Repository remoto",
                    .categoryLabel = "remote",
                    .kind = bo::CompletionItemKind::Remote,
                    .matchStart = 0,
                    .matchLength = prefix.size(),
                    .appendSpace = true,
                    .score = 650
                });
            }
        }

        void addStashCompletions(const bo::RepoCompletionSnapshot& snap,
                                 const std::string_view prefix,
                                 std::vector<bo::CompletionItem>& out,
                                 std::unordered_set<std::string>& seen)
        {
            for (const auto& st : snap.stashes)
            {
                if (!prefix.empty() && !startsWithCaseInsensitive(st.selector, prefix)) continue;
                if (!seen.insert(st.selector).second) continue;

                out.push_back(bo::CompletionItem{
                    .insertText = st.selector,
                    .displayText = st.selector,
                    .detail = st.message,
                    .categoryLabel = "stash",
                    .kind = bo::CompletionItemKind::Stash,
                    .matchStart = 0,
                    .matchLength = prefix.size(),
                    .appendSpace = true,
                    .score = 660
                });
            }
        }

        void addWorkingTreeFileCompletions(const bo::RepoCompletionSnapshot& snap,
                                           const std::string_view prefix,
                                           const bool includeUntracked,
                                           std::vector<bo::CompletionItem>& out,
                                           std::unordered_set<std::string>& seen)
        {
            auto pushFile = [&](const std::string& path, const std::string& detail, const int score)
            {
                if (!prefix.empty() && !startsWithCaseInsensitive(path, prefix)) return;
                if (!seen.insert(path).second) return;

                std::string insert = path;
                if (insert.find(' ') != std::string::npos)
                {
                    insert = "\"" + insert + "\"";
                }
                out.push_back(bo::CompletionItem{
                    .insertText = std::move(insert),
                    .displayText = path,
                    .detail = detail,
                    .categoryLabel = "file",
                    .kind = bo::CompletionItemKind::FilePath,
                    .matchStart = 0,
                    .matchLength = prefix.size(),
                    .appendSpace = true,
                    .score = score
                });
            };

            for (const auto& f : snap.modifiedFiles)
            {
                pushFile(f, "File modificato", 580);
            }
            if (includeUntracked)
            {
                for (const auto& f : snap.untrackedFiles)
                {
                    pushFile(f, "File non tracciato", 560);
                }
            }
        }
    } // namespace

    bo::CompletionResult GitCompletionEngine::complete(const std::string_view inputLine,
                                                       const std::size_t cursorPos,
                                                       const std::filesystem::path& workingDir,
                                                       const GitCatalogService& catalog,
                                                       const bo::RepoCompletionSnapshot& repoSnapshot,
                                                       const bool explicitTrigger)
    {
        const auto seg = CommandLineTokenizer::parseActiveSegment(inputLine, cursorPos);

        bo::CompletionResult result;
        result.replaceStart = seg.activeReplaceStart;
        result.replaceLength = seg.activeReplaceLength;
        result.activePrefix = seg.activePrefix;

        // Stage 0: First token of segment (e.g. `g`, `gi`, `cd`)
        if (seg.activeTokenIndex == 0)
        {
            if (seg.activePrefix.empty() && !explicitTrigger)
            {
                return result;
            }

            struct BuiltinSpec
            {
                const char* name;
                const char* detail;
                int score;
            };
            static constexpr BuiltinSpec kBuiltins[] = {
                {"git", "Git distributed version control system", 1000},
                {"cd", "Cambia la directory corrente del terminale", 700},
                {"clear", "Pulisci l'output del terminale", 650},
                {"cls", "Pulisci l'output del terminale", 640},
                {"pwd", "Mostra la directory corrente", 630},
                {"exit", "Chiudi la scheda terminale corrente", 600}
            };

            std::unordered_set<std::string> seen;
            for (const auto& b : kBuiltins)
            {
                if (seg.activePrefix.empty() || startsWithCaseInsensitive(b.name, seg.activePrefix))
                {
                    seen.insert(b.name);
                    result.items.push_back(bo::CompletionItem{
                        .insertText = b.name,
                        .displayText = b.name,
                        .detail = b.detail,
                        .categoryLabel = (std::string_view(b.name) == "git") ? "git" : "builtin",
                        .kind = bo::CompletionItemKind::ShellBuiltin,
                        .matchStart = 0,
                        .matchLength = seg.activePrefix.size(),
                        .appendSpace = (std::string_view(b.name) == "git" || std::string_view(b.name) == "cd"),
                        .score = b.score
                    });
                }
            }

            // Also complete local directories & files at position 0 (e.g. `sc` -> `scripts/`, `.\sc` -> `.\scripts\`)
            addFilesystemCompletions(workingDir, seg.activePrefix, false, result.items, seen, 500);

            std::ranges::stable_sort(result.items, [](const bo::CompletionItem& a, const bo::CompletionItem& b)
            {
                if (a.score != b.score) return a.score > b.score;
                return a.displayText < b.displayText;
            });
            return result;
        }

        const std::string cmd0 = toLower(seg.tokens[0].value);

        // Non-git commands: support path completion for `cd` and general commands
        if (cmd0 != "git")
        {
            if (cmd0 == "cd" || explicitTrigger || !seg.activePrefix.empty())
            {
                std::unordered_set<std::string> seen;
                addFilesystemCompletions(workingDir, seg.activePrefix, cmd0 == "cd", result.items, seen, 500);
                std::ranges::stable_sort(result.items, [](const bo::CompletionItem& a, const bo::CompletionItem& b)
                {
                    if (a.score != b.score) return a.score > b.score;
                    return a.displayText < b.displayText;
                });
            }
            return result;
        }

        // We are inside a `git ...` command!
        // Walk tokens[1 .. activeTokenIndex) to skip global git options and find the subcommand.
        std::size_t subcommandIndex = std::string::npos;
        bool expectingGlobalOptionValue = false;
        for (std::size_t i = 1; i < seg.activeTokenIndex; ++i)
        {
            const std::string_view tok = seg.tokens[i].value;
            if (expectingGlobalOptionValue)
            {
                expectingGlobalOptionValue = false;
                continue;
            }
            if (tok.starts_with('-'))
            {
                if (globalOptionTakesSeparateArg(tok))
                {
                    expectingGlobalOptionValue = true;
                }
                continue;
            }
            subcommandIndex = i;
            break;
        }

        // If right after `-C `, complete directory paths
        if (expectingGlobalOptionValue)
        {
            if (seg.activeTokenIndex > 0 && seg.tokens[seg.activeTokenIndex - 1].value == "-C")
            {
                std::unordered_set<std::string> seen;
                addFilesystemCompletions(workingDir, seg.activePrefix, true, result.items, seen, 600);
            }
            return result;
        }

        // Stage 1: No subcommand yet -> complete global git options or ALL Git commands & aliases
        if (subcommandIndex == std::string::npos)
        {
            if (!seg.activePrefix.empty() && seg.activePrefix.front() == '-')
            {
                addOptionCompletions(GitCatalogService::globalOptions(), seg.activePrefix, result.items);
            }
            else
            {
                addCommandListCompletions(catalog.allCommands(), seg.activePrefix,
                                          bo::CompletionItemKind::GitCommand, result.items);
            }

            std::ranges::stable_sort(result.items, [](const bo::CompletionItem& a, const bo::CompletionItem& b)
            {
                if (a.score != b.score) return a.score > b.score;
                return a.displayText < b.displayText;
            });
            return result;
        }

        // Stage 2: Subcommand is known!
        const std::string subcommand = seg.tokens[subcommandIndex].value;
        result.activeSubcommand = subcommand;

        // Inspect arguments between subcommand and activeTokenIndex
        bool seenDoubleDash = false;
        std::vector<std::string> positionalArgs;
        for (std::size_t i = subcommandIndex + 1; i < seg.activeTokenIndex; ++i)
        {
            const std::string_view tok = seg.tokens[i].value;
            if (tok == "--")
            {
                seenDoubleDash = true;
                continue;
            }
            if (!seenDoubleDash && tok.starts_with('-'))
            {
                continue;
            }
            positionalArgs.emplace_back(tok);
        }

        // 2a. Flag completion (`git <cmd> -...`)
        if (!seenDoubleDash && !seg.activePrefix.empty() && seg.activePrefix.front() == '-')
        {
            if (const auto opts = catalog.cachedOptionsFor(subcommand); opts.has_value())
            {
                addOptionCompletions(*opts, seg.activePrefix, result.items);
            }
            else
            {
                result.needsSubcommandOptionsLoad = true;
            }

            std::ranges::stable_sort(result.items, [](const bo::CompletionItem& a, const bo::CompletionItem& b)
            {
                if (a.score != b.score) return a.score > b.score;
                return a.displayText < b.displayText;
            });
            return result;
        }

        // 2b. `git help <command>` -> complete all Git commands!
        if (subcommand == "help" && positionalArgs.empty())
        {
            addCommandListCompletions(catalog.allCommands(), seg.activePrefix,
                                      bo::CompletionItemKind::GitCommand, result.items);
            std::ranges::stable_sort(result.items, [](const bo::CompletionItem& a, const bo::CompletionItem& b)
            {
                if (a.score != b.score) return a.score > b.score;
                return a.displayText < b.displayText;
            });
            return result;
        }

        std::unordered_set<std::string> seen;

        // 2c. After `--` pathspec separator -> always suggest working tree / filesystem files
        if (seenDoubleDash)
        {
            addWorkingTreeFileCompletions(repoSnapshot, seg.activePrefix, subcommand == "add", result.items, seen);
            addFilesystemCompletions(workingDir, seg.activePrefix, false, result.items, seen, 400);
        }
        else
        {
            // 2d. Sub-subcommands on first positional arg (e.g. `git stash `, `git remote `, `git worktree `)
            const auto subSubs = GitCatalogService::subSubcommandsFor(subcommand);
            if (positionalArgs.empty() && !subSubs.empty())
            {
                addCommandListCompletions(subSubs, seg.activePrefix,
                                          bo::CompletionItemKind::GitSubcommand, result.items);
                for (const auto& it : result.items)
                {
                    seen.insert(it.displayText);
                }
            }

            // 2e. Context-aware repository entities
            if (subcommand == "stash" && !positionalArgs.empty())
            {
                const std::string_view action = positionalArgs[0];
                if (action == "pop" || action == "apply" || action == "drop" ||
                    action == "show" || action == "branch")
                {
                    addStashCompletions(repoSnapshot, seg.activePrefix, result.items, seen);
                }
            }
            else if (subcommand == "remote" && !positionalArgs.empty())
            {
                const std::string_view action = positionalArgs[0];
                if (action == "remove" || action == "rm" || action == "rename" || action == "show" ||
                    action == "prune" || action == "get-url" || action == "set-url" || action == "update" ||
                    action == "set-head" || action == "set-branches")
                {
                    addRemoteCompletions(repoSnapshot, seg.activePrefix, result.items, seen);
                }
            }
            else if (isRemoteCommand(subcommand))
            {
                if (positionalArgs.empty())
                {
                    addRemoteCompletions(repoSnapshot, seg.activePrefix, result.items, seen);
                }
                else
                {
                    addBranchAndRefCompletions(repoSnapshot, seg.activePrefix, result.items, seen,
                                               subcommand != "push", true);
                }
            }
            else if (subcommand == "add")
            {
                if (seg.activePrefix.empty() || seg.activePrefix == ".")
                {
                    if (seen.insert(".").second)
                    {
                        result.items.push_back(bo::CompletionItem{
                            .insertText = ".",
                            .displayText = ".",
                            .detail = "Aggiungi tutte le modifiche nella directory corrente",
                            .categoryLabel = "path",
                            .kind = bo::CompletionItemKind::FilePath,
                            .matchStart = 0,
                            .matchLength = seg.activePrefix.size(),
                            .appendSpace = true,
                            .score = 620
                        });
                    }
                }
                addWorkingTreeFileCompletions(repoSnapshot, seg.activePrefix, true, result.items, seen);
                addFilesystemCompletions(workingDir, seg.activePrefix, false, result.items, seen, 400);
            }
            else if (subcommand == "restore" || subcommand == "rm" || subcommand == "mv" || subcommand == "blame")
            {
                addWorkingTreeFileCompletions(repoSnapshot, seg.activePrefix, false, result.items, seen);
                addFilesystemCompletions(workingDir, seg.activePrefix, false, result.items, seen, 400);
            }
            else if (isBranchRefCommand(subcommand))
            {
                addBranchAndRefCompletions(repoSnapshot, seg.activePrefix, result.items, seen,
                                           true, subcommand != "switch");
                if (isFileCommand(subcommand))
                {
                    addWorkingTreeFileCompletions(repoSnapshot, seg.activePrefix, false, result.items, seen);
                }
            }
            else if (subcommand == "worktree" && !positionalArgs.empty() && positionalArgs[0] == "add")
            {
                if (positionalArgs.size() == 1)
                {
                    addFilesystemCompletions(workingDir, seg.activePrefix, true, result.items, seen, 500);
                }
                else
                {
                    addBranchAndRefCompletions(repoSnapshot, seg.activePrefix, result.items, seen, true, true);
                }
            }
            else if (explicitTrigger && seg.activePrefix.empty())
            {
                // When explicitly pressing Ctrl+Space after any other git command, offer its flags + files
                if (const auto opts = catalog.cachedOptionsFor(subcommand); opts.has_value())
                {
                    addOptionCompletions(*opts, "", result.items);
                }
                else
                {
                    result.needsSubcommandOptionsLoad = true;
                }
                addFilesystemCompletions(workingDir, "", false, result.items, seen, 300);
            }
        }

        // If the user explicitly pressed Ctrl+Space after `git <cmd> ` with empty prefix, also include flags below contextual items
        if (explicitTrigger && !seenDoubleDash && seg.activePrefix.empty())
        {
            if (const auto opts = catalog.cachedOptionsFor(subcommand); opts.has_value())
            {
                // Append flags (avoid duplicates)
                std::vector<bo::CompletionItem> flagItems;
                addOptionCompletions(*opts, "", flagItems);
                for (auto& fi : flagItems)
                {
                    fi.score -= 200; // keep contextual items (branches/sub-subcommands) above flags
                    if (seen.insert(fi.displayText).second)
                    {
                        result.items.push_back(std::move(fi));
                    }
                }
            }
            else
            {
                result.needsSubcommandOptionsLoad = true;
            }
        }

        std::ranges::stable_sort(result.items, [](const bo::CompletionItem& a, const bo::CompletionItem& b)
        {
            if (a.score != b.score) return a.score > b.score;
            return a.displayText < b.displayText;
        });

        return result;
    }
} // namespace gf::terminal::bl
