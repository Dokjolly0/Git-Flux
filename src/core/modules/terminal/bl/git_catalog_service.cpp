#include "core/modules/terminal/bl/git_catalog_service.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace gf::terminal::bl
{
    namespace
    {
        [[nodiscard]] std::string_view trimView(std::string_view sv) noexcept
        {
            while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n'))
            {
                sv.remove_prefix(1);
            }
            while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n'))
            {
                sv.remove_suffix(1);
            }
            return sv;
        }

        [[nodiscard]] std::string toLowerCopy(const std::string_view sv)
        {
            std::string out;
            out.reserve(sv.size());
            for (const char c : sv)
            {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        }

        [[nodiscard]] bool isInteractiveGuiTool(const std::string_view cmd) noexcept
        {
            return cmd == "gitk" || cmd == "gui" || cmd == "citool" || cmd == "difftool" || cmd == "mergetool";
        }

        [[nodiscard]] bool isValidCommandName(const std::string_view name) noexcept
        {
            if (name.empty()) return false;
            for (const char c : name)
            {
                const unsigned char uc = static_cast<unsigned char>(c);
                if (!(std::isalnum(uc) || c == '-' || c == '_' || c == '.'))
                {
                    return false;
                }
            }
            return true;
        }

        void mergeOptionSpec(std::vector<bo::GitOptionSpec>& target,
                             std::unordered_map<std::string, std::size_t>& byLong,
                             std::unordered_map<std::string, std::size_t>& byShort,
                             bo::GitOptionSpec spec)
        {
            if (spec.longFlag.empty() && spec.shortFlag.empty())
            {
                return;
            }

            std::optional<std::size_t> existingIdx;
            if (!spec.longFlag.empty())
            {
                if (const auto it = byLong.find(spec.longFlag); it != byLong.end())
                {
                    existingIdx = it->second;
                }
            }
            if (!existingIdx && !spec.shortFlag.empty())
            {
                if (const auto it = byShort.find(spec.shortFlag); it != byShort.end())
                {
                    existingIdx = it->second;
                }
            }

            if (existingIdx)
            {
                auto& dst = target[*existingIdx];
                if (dst.shortFlag.empty() && !spec.shortFlag.empty())
                {
                    dst.shortFlag = spec.shortFlag;
                    byShort[dst.shortFlag] = *existingIdx;
                }
                if (dst.longFlag.empty() && !spec.longFlag.empty())
                {
                    dst.longFlag = spec.longFlag;
                    byLong[dst.longFlag] = *existingIdx;
                }
                if (dst.valueHint.empty() && !spec.valueHint.empty())
                {
                    dst.valueHint = spec.valueHint;
                }
                if (dst.description.empty() && !spec.description.empty())
                {
                    dst.description = spec.description;
                }
                dst.takesValue = dst.takesValue || spec.takesValue;
                dst.isNegatable = dst.isNegatable || spec.isNegatable;
                return;
            }

            const std::size_t idx = target.size();
            if (!spec.longFlag.empty()) byLong[spec.longFlag] = idx;
            if (!spec.shortFlag.empty()) byShort[spec.shortFlag] = idx;
            target.push_back(std::move(spec));
        }

        // Parses a single option declaration segment like:
        //   "-m, --[no-]message <message>"
        //   "--amend"
        //   "-u, --untracked-files[=<mode>]"
        //   "-S, --gpg-sign[=<key-id>]"
        //   "-q, --quiet"
        [[nodiscard]] bool parseOptionHeaderToken(std::string_view header, bo::GitOptionSpec& out)
        {
            header = trimView(header);
            if (header.empty() || header.front() != '-')
            {
                return false;
            }

            // Split on commas to separate short and long forms (stopping before value hint if not comma-separated)
            std::size_t pos = 0;
            while (pos < header.size())
            {
                while (pos < header.size() && (header[pos] == ' ' || header[pos] == ','))
                {
                    ++pos;
                }
                if (pos >= header.size()) break;

                if (header[pos] == '-')
                {
                    std::size_t end = pos;
                    while (end < header.size() && header[end] != ',' && header[end] != ' ' &&
                           header[end] != '=' && header[end] != '[')
                    {
                        ++end;
                    }
                    // Special case: "--[no-]foo" has '[' inside the flag name!
                    if (end < header.size() && header.substr(pos, 7) == "--[no-]")
                    {
                        end = pos + 7;
                        while (end < header.size() && header[end] != ',' && header[end] != ' ' &&
                               header[end] != '=' && header[end] != '[')
                        {
                            ++end;
                        }
                    }

                    std::string_view flag = header.substr(pos, end - pos);
                    if (flag.starts_with("--[no-]"))
                    {
                        out.longFlag = std::string("--") + std::string(flag.substr(7));
                        out.isNegatable = true;
                    }
                    else if (flag.starts_with("--") && flag.size() > 2)
                    {
                        out.longFlag = std::string(flag);
                    }
                    else if (flag.size() == 2 && flag[0] == '-' && flag[1] != '-')
                    {
                        out.shortFlag = std::string(flag);
                    }

                    pos = end;
                    // Check for immediate `=<hint>` or `[=<hint>]`
                    if (pos < header.size() && (header[pos] == '=' || header[pos] == '['))
                    {
                        std::size_t hintEnd = pos;
                        while (hintEnd < header.size() && header[hintEnd] != ',' && header[hintEnd] != ' ')
                        {
                            ++hintEnd;
                        }
                        const std::string_view hint = header.substr(pos, hintEnd - pos);
                        if (hint.find('=') != std::string_view::npos || hint.find('<') != std::string_view::npos)
                        {
                            out.takesValue = true;
                            if (out.valueHint.empty())
                            {
                                out.valueHint = std::string(hint);
                            }
                        }
                        pos = hintEnd;
                    }
                }
                else
                {
                    // Remaining text is the value placeholder (e.g. "<file>", "[<mode>]", "...")
                    const std::string_view rest = trimView(header.substr(pos));
                    if (!rest.empty())
                    {
                        out.takesValue = true;
                        if (out.valueHint.empty())
                        {
                            out.valueHint = std::string(rest);
                        }
                    }
                    break;
                }
            }

            return !out.shortFlag.empty() || !out.longFlag.empty();
        }
    } // namespace

    GitCatalogService::GitCatalogService(gf::git::ports::IGitCommandExecutor* executor)
        : m_executor(executor)
        , m_baseCommands(builtinFallbackCommands())
    {
    }

    std::vector<bo::GitCommandSpec> GitCatalogService::parseHelpAllOutput(const std::string_view output)
    {
        std::vector<bo::GitCommandSpec> result;
        std::optional<bo::GitCommandCategory> currentCategory;
        bool skipSection = false;

        std::size_t pos = 0;
        while (pos < output.size())
        {
            std::size_t nl = output.find('\n', pos);
            if (nl == std::string_view::npos) nl = output.size();
            std::string_view line = output.substr(pos, nl - pos);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            pos = (nl < output.size()) ? nl + 1 : nl;

            if (line.empty())
            {
                continue;
            }

            // Section headers start at column 0 (no leading space)
            if (line.front() != ' ' && line.front() != '\t')
            {
                const std::string lower = toLowerCopy(trimView(line));
                skipSection = false;
                if (lower.starts_with("main porcelain"))
                {
                    currentCategory = bo::GitCommandCategory::MainPorcelain;
                }
                else if (lower.starts_with("ancillary commands / manipulators"))
                {
                    currentCategory = bo::GitCommandCategory::AncillaryManipulator;
                }
                else if (lower.starts_with("ancillary commands / interrogators"))
                {
                    currentCategory = bo::GitCommandCategory::AncillaryInterrogator;
                }
                else if (lower.starts_with("interacting with others"))
                {
                    currentCategory = bo::GitCommandCategory::InteractingWithOthers;
                }
                else if (lower.starts_with("low-level commands / manipulators"))
                {
                    currentCategory = bo::GitCommandCategory::LowLevelManipulator;
                }
                else if (lower.starts_with("low-level commands / interrogators"))
                {
                    currentCategory = bo::GitCommandCategory::LowLevelInterrogator;
                }
                else if (lower.starts_with("low-level commands / syncing"))
                {
                    currentCategory = bo::GitCommandCategory::LowLevelSync;
                }
                else if (lower.starts_with("low-level commands / internal helpers"))
                {
                    currentCategory = bo::GitCommandCategory::LowLevelHelper;
                }
                else if (lower.starts_with("external commands"))
                {
                    currentCategory = bo::GitCommandCategory::External;
                }
                else if (lower.starts_with("command aliases"))
                {
                    currentCategory = bo::GitCommandCategory::Alias;
                }
                else if (lower.starts_with("user-facing") || lower.starts_with("developer-facing") ||
                         lower.starts_with("guides") || lower.starts_with("see 'git help"))
                {
                    currentCategory.reset();
                    skipSection = true;
                }
                continue;
            }

            if (skipSection || !currentCategory.has_value())
            {
                continue;
            }

            const std::string_view trimmed = trimView(line);
            if (trimmed.empty()) continue;

            // Format: "   add                     Add file contents to the index"
            // External commands may have only the name without description: "   lfs"
            std::size_t nameEnd = 0;
            while (nameEnd < trimmed.size() && trimmed[nameEnd] != ' ' && trimmed[nameEnd] != '\t')
            {
                ++nameEnd;
            }
            const std::string_view name = trimmed.substr(0, nameEnd);
            if (!isValidCommandName(name))
            {
                continue;
            }

            std::string_view desc;
            if (nameEnd < trimmed.size())
            {
                desc = trimView(trimmed.substr(nameEnd));
            }

            result.push_back(bo::GitCommandSpec{
                .name = std::string(name),
                .description = std::string(desc),
                .category = *currentCategory
            });
        }

        return result;
    }

    std::vector<std::string> GitCatalogService::parseListCmdsOutput(const std::string_view output)
    {
        std::vector<std::string> cmds;
        std::size_t pos = 0;
        while (pos < output.size())
        {
            std::size_t nl = output.find('\n', pos);
            if (nl == std::string_view::npos) nl = output.size();
            const std::string_view token = trimView(output.substr(pos, nl - pos));
            pos = (nl < output.size()) ? nl + 1 : nl;
            if (isValidCommandName(token))
            {
                cmds.emplace_back(token);
            }
        }
        return cmds;
    }

    std::vector<bo::GitCommandSpec> GitCatalogService::parseAliasesOutput(const std::string_view output)
    {
        std::vector<bo::GitCommandSpec> aliases;
        std::unordered_map<std::string, std::size_t> byName;

        std::size_t pos = 0;
        while (pos < output.size())
        {
            std::size_t nl = output.find('\n', pos);
            if (nl == std::string_view::npos) nl = output.size();
            const std::string_view line = trimView(output.substr(pos, nl - pos));
            pos = (nl < output.size()) ? nl + 1 : nl;

            if (!line.starts_with("alias."))
            {
                continue;
            }
            const std::string_view rest = line.substr(6);
            const std::size_t sp = rest.find_first_of(" \t");
            const std::string_view name = (sp == std::string_view::npos) ? rest : rest.substr(0, sp);
            const std::string_view expansion = (sp == std::string_view::npos)
                                                   ? std::string_view{}
                                                   : trimView(rest.substr(sp + 1));
            if (!isValidCommandName(name))
            {
                continue;
            }

            std::string desc = expansion.empty()
                                   ? std::string("Alias Git")
                                   : (std::string("Alias: ") + std::string(expansion));
            if (const auto it = byName.find(std::string(name)); it != byName.end())
            {
                // Later entries override earlier ones (repo-local overrides global)
                aliases[it->second].description = std::move(desc);
            }
            else
            {
                byName[std::string(name)] = aliases.size();
                aliases.push_back(bo::GitCommandSpec{
                    .name = std::string(name),
                    .description = std::move(desc),
                    .category = bo::GitCommandCategory::Alias
                });
            }
        }
        return aliases;
    }

    std::vector<bo::GitOptionSpec> GitCatalogService::parseSubcommandHelpOptions(const std::string_view output)
    {
        std::vector<bo::GitOptionSpec> options;
        std::unordered_map<std::string, std::size_t> byLong;
        std::unordered_map<std::string, std::size_t> byShort;

        bo::GitOptionSpec pending{};
        bool hasPending = false;

        auto flushPending = [&]()
        {
            if (!hasPending) return;
            mergeOptionSpec(options, byLong, byShort, std::move(pending));
            pending = bo::GitOptionSpec{};
            hasPending = false;
        };

        std::size_t pos = 0;
        while (pos < output.size())
        {
            std::size_t nl = output.find('\n', pos);
            if (nl == std::string_view::npos) nl = output.size();
            std::string_view line = output.substr(pos, nl - pos);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            pos = (nl < output.size()) ? nl + 1 : nl;

            const std::string_view trimmed = trimView(line);
            if (trimmed.empty())
            {
                flushPending();
                continue;
            }

            // Count leading spaces
            std::size_t indent = 0;
            while (indent < line.size() && line[indent] == ' ')
            {
                ++indent;
            }

            // Option lines in `git <cmd> -h` are indented by 4 spaces (or 2..8 spaces) and start with '-'
            if (indent >= 2 && indent <= 10 && trimmed.front() == '-')
            {
                flushPending();

                // Look for 2+ consecutive spaces separating option header from description on same line
                std::size_t splitPos = std::string_view::npos;
                for (std::size_t i = 1; i + 1 < trimmed.size(); ++i)
                {
                    if (trimmed[i] == ' ' && trimmed[i + 1] == ' ')
                    {
                        splitPos = i;
                        break;
                    }
                }

                const std::string_view headerPart = (splitPos == std::string_view::npos)
                                                        ? trimmed
                                                        : trimmed.substr(0, splitPos);
                const std::string_view descPart = (splitPos == std::string_view::npos)
                                                      ? std::string_view{}
                                                      : trimView(trimmed.substr(splitPos));

                bo::GitOptionSpec spec{};
                if (parseOptionHeaderToken(headerPart, spec))
                {
                    spec.description = std::string(descPart);
                    pending = std::move(spec);
                    hasPending = true;
                }
            }
            else if (hasPending && indent >= 16)
            {
                // Continuation line for the pending option's description (column ~26 in git -h)
                if (pending.description.empty())
                {
                    pending.description = std::string(trimmed);
                }
                else
                {
                    pending.description.push_back(' ');
                    pending.description.append(trimmed);
                }
            }
            else
            {
                flushPending();
            }
        }
        flushPending();

        return options;
    }

    std::vector<bo::GitOptionSpec> GitCatalogService::parseCompletionHelperOptions(const std::string_view output)
    {
        std::vector<bo::GitOptionSpec> options;
        std::unordered_map<std::string, std::size_t> byLong;
        std::unordered_map<std::string, std::size_t> byShort;

        std::size_t i = 0;
        while (i < output.size())
        {
            while (i < output.size() && std::isspace(static_cast<unsigned char>(output[i])))
            {
                ++i;
            }
            if (i >= output.size()) break;
            std::size_t j = i;
            while (j < output.size() && !std::isspace(static_cast<unsigned char>(output[j])))
            {
                ++j;
            }
            std::string_view tok = output.substr(i, j - i);
            i = j;

            if (!tok.starts_with("--") || tok == "--")
            {
                continue;
            }

            bo::GitOptionSpec spec{};
            if (tok.back() == '=')
            {
                spec.longFlag = std::string(tok.substr(0, tok.size() - 1));
                spec.takesValue = true;
            }
            else
            {
                spec.longFlag = std::string(tok);
            }
            mergeOptionSpec(options, byLong, byShort, std::move(spec));
        }

        return options;
    }

    void GitCatalogService::discoverAllCommands(const std::filesystem::path& repoPath)
    {
        std::vector<bo::GitCommandSpec> discovered = builtinFallbackCommands();
        std::unordered_map<std::string, std::size_t> byName;
        for (std::size_t i = 0; i < discovered.size(); ++i)
        {
            byName[discovered[i].name] = i;
        }

        if (m_executor)
        {
            const std::vector<std::pair<std::string, std::string>> cLocaleEnv = {
                {"LC_ALL", "C"},
                {"LANG", "C"}
            };

            // 1. `git help -a` (authoritative categories + official descriptions)
            {
                gf::git::bo::GitCommand cmd;
                cmd.args = {"help", "-a"};
                cmd.timeout = std::chrono::milliseconds{5000};
                cmd.extraEnvironment = cLocaleEnv;
                if (const auto res = m_executor->execute(cmd); res && !res->stdoutBytes.empty())
                {
                    auto parsed = parseHelpAllOutput(res->stdoutBytes);
                    for (auto& spec : parsed)
                    {
                        if (spec.category == bo::GitCommandCategory::Alias)
                        {
                            continue; // aliases are loaded separately with full expansion
                        }
                        if (const auto it = byName.find(spec.name); it != byName.end())
                        {
                            discovered[it->second].category = spec.category;
                            if (!spec.description.empty())
                            {
                                discovered[it->second].description = std::move(spec.description);
                            }
                        }
                        else
                        {
                            byName[spec.name] = discovered.size();
                            discovered.push_back(std::move(spec));
                        }
                    }
                }
            }

            // 2. `git --list-cmds=main,others,nohelpers` (100% coverage of all runnable commands)
            {
                gf::git::bo::GitCommand cmd;
                cmd.args = {"--list-cmds=main,others,nohelpers"};
                cmd.timeout = std::chrono::milliseconds{5000};
                cmd.extraEnvironment = cLocaleEnv;
                if (const auto res = m_executor->execute(cmd); res && res->succeeded())
                {
                    for (const auto& name : parseListCmdsOutput(res->stdoutBytes))
                    {
                        if (!byName.contains(name))
                        {
                            byName[name] = discovered.size();
                            discovered.push_back(bo::GitCommandSpec{
                                .name = name,
                                .description = "Comando Git",
                                .category = bo::GitCommandCategory::External
                            });
                        }
                    }
                }
            }
        }

        std::ranges::sort(discovered, [](const bo::GitCommandSpec& a, const bo::GitCommandSpec& b)
        {
            const int pa = bo::categoryPriority(a.category);
            const int pb = bo::categoryPriority(b.category);
            if (pa != pb) return pa < pb;
            return a.name < b.name;
        });

        {
            std::scoped_lock lock(m_mutex);
            m_baseCommands = std::move(discovered);
        }

        refreshAliases(repoPath);
        m_discovered.store(true, std::memory_order_release);
    }

    void GitCatalogService::refreshAliases(const std::filesystem::path& repoPath)
    {
        if (!m_executor)
        {
            return;
        }

        gf::git::bo::GitCommand cmd;
        cmd.args = {"config", "--get-regexp", "^alias\\."};
        if (!repoPath.empty() && std::filesystem::exists(repoPath))
        {
            cmd.workingDirectory = repoPath;
        }
        cmd.timeout = std::chrono::milliseconds{3000};

        std::vector<bo::GitCommandSpec> aliases;
        if (const auto res = m_executor->execute(cmd); res && !res->stdoutBytes.empty())
        {
            aliases = parseAliasesOutput(res->stdoutBytes);
        }

        std::scoped_lock lock(m_mutex);
        m_aliases = std::move(aliases);
    }

    std::vector<bo::GitCommandSpec> GitCatalogService::allCommands() const
    {
        std::scoped_lock lock(m_mutex);
        std::vector<bo::GitCommandSpec> merged;
        merged.reserve(m_baseCommands.size() + m_aliases.size());
        std::unordered_set<std::string> seen;
        seen.reserve(m_baseCommands.size() + m_aliases.size());

        for (const auto& cmd : m_baseCommands)
        {
            if (seen.insert(cmd.name).second)
            {
                merged.push_back(cmd);
            }
        }
        for (const auto& alias : m_aliases)
        {
            if (seen.insert(alias.name).second)
            {
                merged.push_back(alias);
            }
        }
        return merged;
    }

    std::optional<std::vector<bo::GitOptionSpec>> GitCatalogService::cachedOptionsFor(const std::string_view subcommand) const
    {
        std::scoped_lock lock(m_mutex);
        if (const auto it = m_optionsCache.find(std::string(subcommand)); it != m_optionsCache.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    void GitCatalogService::setCachedOptionsFor(std::string subcommand, std::vector<bo::GitOptionSpec> options)
    {
        std::scoped_lock lock(m_mutex);
        m_optionsCache[std::move(subcommand)] = std::move(options);
    }

    void GitCatalogService::setCommandsForTesting(std::vector<bo::GitCommandSpec> commands)
    {
        std::scoped_lock lock(m_mutex);
        m_baseCommands = std::move(commands);
        m_aliases.clear();
        m_discovered.store(true, std::memory_order_release);
    }

    std::vector<bo::GitOptionSpec> GitCatalogService::loadOptionsFor(const std::string_view subcommand)
    {
        if (subcommand.empty() || !isValidCommandName(subcommand))
        {
            return {};
        }

        if (auto cached = cachedOptionsFor(subcommand); cached.has_value())
        {
            return *cached;
        }

        std::vector<bo::GitOptionSpec> merged;
        std::unordered_map<std::string, std::size_t> byLong;
        std::unordered_map<std::string, std::size_t> byShort;

        if (m_executor && !isInteractiveGuiTool(subcommand))
        {
            const std::vector<std::pair<std::string, std::string>> cLocaleEnv = {
                {"LC_ALL", "C"},
                {"LANG", "C"}
            };

            // 1. `git <subcommand> -h` (rich descriptions + short flags + value hints)
            {
                gf::git::bo::GitCommand cmd;
                cmd.args = {std::string(subcommand), "-h"};
                cmd.timeout = std::chrono::milliseconds{3000};
                cmd.extraEnvironment = cLocaleEnv;
                if (const auto res = m_executor->execute(cmd); res)
                {
                    std::string combined = res->stdoutBytes;
                    if (!res->stderrBytes.empty())
                    {
                        if (!combined.empty()) combined.push_back('\n');
                        combined.append(res->stderrBytes);
                    }
                    for (auto& opt : parseSubcommandHelpOptions(combined))
                    {
                        mergeOptionSpec(merged, byLong, byShort, std::move(opt));
                    }
                }
            }

            // 2. `git <subcommand> --git-completion-helper` (catches flags omitted from `-h`)
            {
                gf::git::bo::GitCommand cmd;
                cmd.args = {std::string(subcommand), "--git-completion-helper"};
                cmd.timeout = std::chrono::milliseconds{3000};
                cmd.extraEnvironment = cLocaleEnv;
                if (const auto res = m_executor->execute(cmd); res && res->succeeded() && !res->stdoutBytes.empty())
                {
                    for (auto& opt : parseCompletionHelperOptions(res->stdoutBytes))
                    {
                        mergeOptionSpec(merged, byLong, byShort, std::move(opt));
                    }
                }
            }
        }

        std::ranges::sort(merged, [](const bo::GitOptionSpec& a, const bo::GitOptionSpec& b)
        {
            const std::string_view ka = !a.longFlag.empty() ? std::string_view(a.longFlag) : std::string_view(a.shortFlag);
            const std::string_view kb = !b.longFlag.empty() ? std::string_view(b.longFlag) : std::string_view(b.shortFlag);
            return ka < kb;
        });

        setCachedOptionsFor(std::string(subcommand), merged);
        return merged;
    }

    const std::vector<bo::GitOptionSpec>& GitCatalogService::globalOptions()
    {
        static const std::vector<bo::GitOptionSpec> kGlobal = {
            {"-C", "", "<path>", "Run as if git was started in <path>", true, false},
            {"-c", "", "<name>=<value>", "Pass a configuration parameter to the command", true, false},
            {"-v", "--version", "", "Prints the Git suite version", false, false},
            {"-h", "--help", "", "Prints the synopsis and a list of commonly used commands", false, false},
            {"-p", "--paginate", "", "Pipe all output into less (or PAGER)", false, false},
            {"-P", "--no-pager", "", "Do not pipe Git output into a pager", false, false},
            {"", "--bare", "", "Treat the repository as a bare repository", false, false},
            {"", "--git-dir", "=<path>", "Set the path to the repository (.git directory)", true, false},
            {"", "--work-tree", "=<path>", "Set the path to the working tree", true, false},
            {"", "--namespace", "=<name>", "Set the Git namespace", true, false},
            {"", "--no-replace-objects", "", "Do not use replacement refs to replace Git objects", false, false},
            {"", "--no-lazy-fetch", "", "Do not fetch missing objects from the promisor remote", false, false},
            {"", "--no-optional-locks", "", "Do not perform optional operations that require locks", false, false},
            {"", "--no-advice", "", "Disable all advice hints from being printed", false, false},
            {"", "--exec-path", "[=<path>]", "Path to wherever your core Git programs are installed", false, false},
            {"", "--html-path", "", "Print the path where Git's HTML documentation is installed", false, false},
            {"", "--man-path", "", "Print the manpath for the man pages", false, false},
            {"", "--info-path", "", "Print the path where the Info files are installed", false, false},
            {"", "--config-env", "=<name>=<envvar>", "Like -c <name>=<value>, reading value from envvar", true, false},
            {"", "--list-cmds", "=<group>", "List commands by group", true, false}
        };
        return kGlobal;
    }

    std::vector<bo::GitCommandSpec> GitCatalogService::subSubcommandsFor(const std::string_view subcommand)
    {
        using C = bo::GitCommandCategory;
        if (subcommand == "stash")
        {
            return {
                {"push", "Save local modifications to a new stash entry", C::MainPorcelain},
                {"pop", "Remove a single stashed state and apply it to working tree", C::MainPorcelain},
                {"apply", "Apply stashed state without removing it from stash list", C::MainPorcelain},
                {"list", "List the stash entries that you currently have", C::MainPorcelain},
                {"show", "Show the changes recorded in the stash entry as a diff", C::MainPorcelain},
                {"drop", "Remove a single stash entry from the list", C::MainPorcelain},
                {"clear", "Remove all the stash entries", C::MainPorcelain},
                {"branch", "Create and check out a new branch from a stash entry", C::MainPorcelain},
                {"create", "Create a stash entry without storing it in ref namespace", C::AncillaryManipulator},
                {"store", "Store a given stash created via create in the stash ref", C::AncillaryManipulator},
                {"save", "Deprecated: use stash push instead", C::AncillaryManipulator}
            };
        }
        if (subcommand == "remote")
        {
            return {
                {"add", "Add a remote named <name> for the repository at <url>", C::MainPorcelain},
                {"rename", "Rename the remote named <old> to <new>", C::MainPorcelain},
                {"remove", "Remove the remote named <name>", C::MainPorcelain},
                {"rm", "Alias for remote remove", C::MainPorcelain},
                {"get-url", "Retrieve the URLs for a remote", C::MainPorcelain},
                {"set-url", "Change URLs for the remote", C::MainPorcelain},
                {"show", "Give some information about the remote <name>", C::MainPorcelain},
                {"prune", "Delete stale references associated with <name>", C::MainPorcelain},
                {"update", "Fetch updates for remotes or remote groups", C::MainPorcelain},
                {"set-head", "Set or delete the default branch for the named remote", C::AncillaryManipulator},
                {"set-branches", "Change the list of branches tracked by the named remote", C::AncillaryManipulator}
            };
        }
        if (subcommand == "worktree")
        {
            return {
                {"add", "Create a new working tree linked to this repository", C::MainPorcelain},
                {"list", "List details of each working tree", C::MainPorcelain},
                {"remove", "Remove a working tree", C::MainPorcelain},
                {"move", "Move a working tree to a new location", C::MainPorcelain},
                {"prune", "Prune working tree information in $GIT_DIR/worktrees", C::MainPorcelain},
                {"lock", "Prevent a working tree from being pruned or moved", C::AncillaryManipulator},
                {"unlock", "Unlock a working tree so it can be pruned or moved", C::AncillaryManipulator},
                {"repair", "Repair working tree administrative files", C::AncillaryManipulator}
            };
        }
        if (subcommand == "submodule")
        {
            return {
                {"add", "Add the given repository as a submodule", C::MainPorcelain},
                {"status", "Show the status of the submodules", C::MainPorcelain},
                {"init", "Initialize the submodules recorded in the index", C::MainPorcelain},
                {"deinit", "Unregister the given submodules", C::MainPorcelain},
                {"update", "Update the registered submodules to match expected commits", C::MainPorcelain},
                {"foreach", "Evaluate an arbitrary shell command in each checked out submodule", C::MainPorcelain},
                {"sync", "Synchronize submodules' remote URL configuration", C::MainPorcelain},
                {"summary", "Show commit summary between given commit and working tree", C::AncillaryInterrogator},
                {"set-branch", "Set the default remote tracking branch for the submodule", C::AncillaryManipulator},
                {"set-url", "Set the URL of the specified submodule", C::AncillaryManipulator},
                {"absorbgitdirs", "Embed git directory of submodule into superproject", C::AncillaryManipulator}
            };
        }
        if (subcommand == "bisect")
        {
            return {
                {"start", "Start a bisect session", C::MainPorcelain},
                {"bad", "Mark current or given commit as bad", C::MainPorcelain},
                {"good", "Mark current or given commit as good", C::MainPorcelain},
                {"new", "Mark commit as having the new behavior", C::MainPorcelain},
                {"old", "Mark commit as having the old behavior", C::MainPorcelain},
                {"skip", "Skip the current commit during bisect", C::MainPorcelain},
                {"reset", "Finish bisect and return to original HEAD", C::MainPorcelain},
                {"run", "Automatically bisect using a script or command", C::MainPorcelain},
                {"log", "Show the bisect log so far", C::AncillaryInterrogator},
                {"replay", "Replay a saved bisect log file", C::AncillaryManipulator},
                {"terms", "Show the terms currently used for old/new states", C::AncillaryInterrogator},
                {"visualize", "Show remaining suspects in gitk/git log", C::AncillaryInterrogator},
                {"view", "Alias for bisect visualize", C::AncillaryInterrogator}
            };
        }
        if (subcommand == "notes")
        {
            return {
                {"list", "List the notes object for a given object", C::MainPorcelain},
                {"add", "Add notes for a given object", C::MainPorcelain},
                {"append", "Append to the notes of an existing object", C::MainPorcelain},
                {"edit", "Edit the notes for a given object", C::MainPorcelain},
                {"show", "Show the notes for a given object", C::MainPorcelain},
                {"copy", "Copy the notes for the first object onto the second", C::MainPorcelain},
                {"merge", "Merge the given notes ref into the current notes ref", C::MainPorcelain},
                {"remove", "Remove the notes for given objects", C::MainPorcelain},
                {"prune", "Remove all notes for non-existent/unreachable objects", C::AncillaryManipulator},
                {"get-ref", "Print the current notes ref", C::AncillaryInterrogator}
            };
        }
        if (subcommand == "reflog")
        {
            return {
                {"show", "Show the log of the reference provided in the command-line", C::MainPorcelain},
                {"list", "List all refs which have a corresponding reflog", C::MainPorcelain},
                {"expire", "Prune older reflog entries", C::AncillaryManipulator},
                {"delete", "Delete single entries from the reflog", C::AncillaryManipulator},
                {"exists", "Check whether a ref has a reflog", C::AncillaryInterrogator},
                {"drop", "Remove the reflog for the specified references", C::AncillaryManipulator}
            };
        }
        if (subcommand == "sparse-checkout")
        {
            return {
                {"list", "Describe the directories or patterns in the sparse-checkout file", C::MainPorcelain},
                {"init", "Enable sparse-checkout (deprecated in favor of set)", C::MainPorcelain},
                {"set", "Enable sparse-checkout and write patterns", C::MainPorcelain},
                {"add", "Add patterns to the sparse-checkout file", C::MainPorcelain},
                {"reapply", "Reapply the sparsity pattern rules to paths in working tree", C::MainPorcelain},
                {"disable", "Disable sparse-checkout and restore full working directory", C::MainPorcelain},
                {"check-rules", "Check whether sparsity rules match one or more paths", C::AncillaryInterrogator}
            };
        }
        if (subcommand == "maintenance")
        {
            return {
                {"run", "Run one or more maintenance tasks", C::MainPorcelain},
                {"start", "Start running maintenance on the current repository", C::MainPorcelain},
                {"stop", "Halt the background maintenance schedule", C::MainPorcelain},
                {"register", "Add the current repository to maintenance.repo config", C::MainPorcelain},
                {"unregister", "Remove the current repository from maintenance.repo config", C::MainPorcelain}
            };
        }
        if (subcommand == "bundle")
        {
            return {
                {"create", "Create a bundle file from specified refs", C::MainPorcelain},
                {"verify", "Check that a bundle file is valid and can be applied", C::MainPorcelain},
                {"list-heads", "List the references defined in the bundle", C::MainPorcelain},
                {"unbundle", "Pass objects in the bundle to git index-pack", C::MainPorcelain}
            };
        }
        if (subcommand == "config")
        {
            return {
                {"list", "List all variables set in config file along with values", C::MainPorcelain},
                {"get", "Get value for a given key", C::MainPorcelain},
                {"set", "Set a configuration value", C::MainPorcelain},
                {"unset", "Remove a configuration key", C::MainPorcelain},
                {"edit", "Open an editor to modify the specified config file", C::MainPorcelain},
                {"rename-section", "Rename the given section to a new name", C::AncillaryManipulator},
                {"remove-section", "Remove the given section from the configuration file", C::AncillaryManipulator}
            };
        }
        if (subcommand == "rerere")
        {
            return {
                {"status", "Print paths with conflicts whose merge resolution will be recorded", C::MainPorcelain},
                {"diff", "Display diffs for the current state of the resolution", C::MainPorcelain},
                {"remaining", "Print paths with conflicts that have not been resolved", C::MainPorcelain},
                {"forget", "Reset the conflict resolutions which rerere has recorded", C::MainPorcelain},
                {"clear", "Reset the metadata used by rerere", C::AncillaryManipulator},
                {"gc", "Prune records of conflicted merges that occurred a long time ago", C::AncillaryManipulator}
            };
        }
        if (subcommand == "lfs")
        {
            return {
                {"track", "View or add Git LFS paths to Git Attributes", C::External},
                {"untrack", "Remove Git LFS paths from Git Attributes", C::External},
                {"status", "Show the status of Git LFS files in the working tree", C::External},
                {"ls-files", "Show information about Git LFS files in the index and working tree", C::External},
                {"pull", "Fetch Git LFS changes from the remote & checkout any required files", C::External},
                {"push", "Push queued large files to the Git LFS endpoint", C::External},
                {"fetch", "Download Git LFS files from a remote", C::External},
                {"checkout", "Update working copy with file content if available", C::External},
                {"prune", "Delete old LFS files from local storage", C::External},
                {"migrate", "Migrate history to or from Git LFS", C::External},
                {"install", "Install Git LFS configuration", C::External},
                {"env", "Display the Git LFS environment", C::External},
                {"version", "Report the version number", C::External}
            };
        }
        if (subcommand == "commit-graph")
        {
            return {
                {"write", "Write a commit-graph file based on commits in packfiles", C::LowLevelManipulator},
                {"verify", "Read the commit-graph file and verify its contents", C::LowLevelInterrogator}
            };
        }
        if (subcommand == "multi-pack-index")
        {
            return {
                {"write", "Write a new MIDX file", C::LowLevelManipulator},
                {"verify", "Verify the contents of the MIDX file", C::LowLevelInterrogator},
                {"expire", "Delete packfiles with no objects referenced by MIDX", C::LowLevelManipulator},
                {"repack", "Create a new packfile containing objects in small packfiles", C::LowLevelManipulator}
            };
        }
        if (subcommand == "credential")
        {
            return {
                {"fill", "Request credentials from configured credential helpers", C::LowLevelHelper},
                {"approve", "Store credentials with configured credential helpers", C::LowLevelHelper},
                {"reject", "Erase matching credentials from credential helpers", C::LowLevelHelper}
            };
        }
        return {};
    }

    std::vector<bo::GitCommandSpec> GitCatalogService::builtinFallbackCommands()
    {
        using C = bo::GitCommandCategory;
        return {
            // Main Porcelain
            {"add", "Add file contents to the index", C::MainPorcelain},
            {"am", "Apply a series of patches from a mailbox", C::MainPorcelain},
            {"archive", "Create an archive of files from a named tree", C::MainPorcelain},
            {"bisect", "Use binary search to find the commit that introduced a bug", C::MainPorcelain},
            {"branch", "List, create, or delete branches", C::MainPorcelain},
            {"bundle", "Move objects and refs by archive", C::MainPorcelain},
            {"checkout", "Switch branches or restore working tree files", C::MainPorcelain},
            {"cherry-pick", "Apply the changes introduced by some existing commits", C::MainPorcelain},
            {"clean", "Remove untracked files from the working tree", C::MainPorcelain},
            {"clone", "Clone a repository into a new directory", C::MainPorcelain},
            {"commit", "Record changes to the repository", C::MainPorcelain},
            {"describe", "Give an object a human readable name based on an available ref", C::MainPorcelain},
            {"diff", "Show changes between commits, commit and working tree, etc", C::MainPorcelain},
            {"fetch", "Download objects and refs from another repository", C::MainPorcelain},
            {"format-patch", "Prepare patches for e-mail submission", C::MainPorcelain},
            {"gc", "Cleanup unnecessary files and optimize the local repository", C::MainPorcelain},
            {"grep", "Print lines matching a pattern", C::MainPorcelain},
            {"init", "Create an empty Git repository or reinitialize an existing one", C::MainPorcelain},
            {"log", "Show commit logs", C::MainPorcelain},
            {"maintenance", "Run tasks to optimize Git repository data", C::MainPorcelain},
            {"merge", "Join two or more development histories together", C::MainPorcelain},
            {"mv", "Move or rename a file, a directory, or a symlink", C::MainPorcelain},
            {"notes", "Add or inspect object notes", C::MainPorcelain},
            {"pull", "Fetch from and integrate with another repository or a local branch", C::MainPorcelain},
            {"push", "Update remote refs along with associated objects", C::MainPorcelain},
            {"range-diff", "Compare two commit ranges", C::MainPorcelain},
            {"rebase", "Reapply commits on top of another base tip", C::MainPorcelain},
            {"reset", "Reset current HEAD to the specified state", C::MainPorcelain},
            {"restore", "Restore working tree files", C::MainPorcelain},
            {"revert", "Revert some existing commits", C::MainPorcelain},
            {"rm", "Remove files from the working tree and from the index", C::MainPorcelain},
            {"shortlog", "Summarize git log output", C::MainPorcelain},
            {"show", "Show various types of objects", C::MainPorcelain},
            {"sparse-checkout", "Reduce your working tree to a subset of tracked files", C::MainPorcelain},
            {"stash", "Stash the changes in a dirty working directory away", C::MainPorcelain},
            {"status", "Show the working tree status", C::MainPorcelain},
            {"submodule", "Initialize, update or inspect submodules", C::MainPorcelain},
            {"switch", "Switch branches", C::MainPorcelain},
            {"tag", "Create, list, delete or verify a tag object signed with GPG", C::MainPorcelain},
            {"worktree", "Manage multiple working trees", C::MainPorcelain},

            // Ancillary Manipulators & Interrogators
            {"blame", "Show what revision and author last modified each line of a file", C::AncillaryInterrogator},
            {"bugreport", "Collect information for user to file a bug report", C::AncillaryInterrogator},
            {"cherry", "Find commits yet to be applied to upstream", C::AncillaryInterrogator},
            {"config", "Get and set repository or global options", C::AncillaryManipulator},
            {"count-objects", "Count unpacked number of objects and their disk consumption", C::AncillaryInterrogator},
            {"diagnose", "Generate a zip archive of diagnostic information", C::AncillaryInterrogator},
            {"difftool", "Show changes using common diff tools", C::AncillaryInterrogator},
            {"fast-export", "Git data exporter", C::AncillaryManipulator},
            {"fast-import", "Backend for fast Git data importers", C::AncillaryManipulator},
            {"filter-branch", "Rewrite branches", C::AncillaryManipulator},
            {"fsck", "Verifies the connectivity and validity of the objects in the database", C::AncillaryInterrogator},
            {"help", "Display help information about Git", C::AncillaryInterrogator},
            {"mergetool", "Run merge conflict resolution tools to resolve merge conflicts", C::AncillaryManipulator},
            {"pack-refs", "Pack heads and tags for efficient repository access", C::AncillaryManipulator},
            {"prune", "Prune all unreachable objects from the object database", C::AncillaryManipulator},
            {"reflog", "Manage reflog information", C::AncillaryManipulator},
            {"remote", "Manage set of tracked repositories", C::AncillaryManipulator},
            {"repack", "Pack unpacked objects in a repository", C::AncillaryManipulator},
            {"replace", "Create, list, delete refs to replace objects", C::AncillaryManipulator},
            {"rerere", "Reuse recorded resolution of conflicted merges", C::AncillaryInterrogator},
            {"show-branch", "Show branches and their commits", C::AncillaryInterrogator},
            {"verify-commit", "Check the GPG signature of commits", C::AncillaryInterrogator},
            {"verify-tag", "Check the GPG signature of tags", C::AncillaryInterrogator},
            {"version", "Display version information about Git", C::AncillaryInterrogator},
            {"whatchanged", "Show logs with differences each commit introduces", C::AncillaryInterrogator},

            // Low-Level / Plumbing (common subset; dynamically extended by `git help -a` + `--list-cmds`)
            {"apply", "Apply a patch to files and/or to the index", C::LowLevelManipulator},
            {"cat-file", "Provide contents or details of repository objects", C::LowLevelInterrogator},
            {"check-attr", "Display gitattributes information", C::LowLevelInterrogator},
            {"check-ignore", "Debug gitignore / exclude files", C::LowLevelInterrogator},
            {"check-mailmap", "Show canonical names and email addresses of contacts", C::LowLevelInterrogator},
            {"check-ref-format", "Ensures that a reference name is well formed", C::LowLevelInterrogator},
            {"checkout-index", "Copy files from the index to the working tree", C::LowLevelManipulator},
            {"commit-graph", "Write and verify Git commit-graph files", C::LowLevelManipulator},
            {"commit-tree", "Create a new commit object", C::LowLevelManipulator},
            {"diff-files", "Compares files in the working tree and the index", C::LowLevelInterrogator},
            {"diff-index", "Compare a tree to the working tree or index", C::LowLevelInterrogator},
            {"diff-tree", "Compares the content and mode of blobs found via two tree objects", C::LowLevelInterrogator},
            {"for-each-ref", "Output information on each ref", C::LowLevelInterrogator},
            {"hash-object", "Compute object ID and optionally create an object from a file", C::LowLevelManipulator},
            {"ls-files", "Show information about files in the index and the working tree", C::LowLevelInterrogator},
            {"ls-remote", "List references in a remote repository", C::LowLevelInterrogator},
            {"ls-tree", "List the contents of a tree object", C::LowLevelInterrogator},
            {"merge-base", "Find as good common ancestors as possible for a merge", C::LowLevelInterrogator},
            {"merge-tree", "Perform merge without touching index or working tree", C::LowLevelManipulator},
            {"mktree", "Build a tree-object from ls-tree formatted text", C::LowLevelManipulator},
            {"multi-pack-index", "Write and verify multi-pack-indexes", C::LowLevelManipulator},
            {"name-rev", "Find symbolic names for given revs", C::LowLevelInterrogator},
            {"pack-objects", "Create a packed archive of objects", C::LowLevelManipulator},
            {"read-tree", "Reads tree information into the index", C::LowLevelManipulator},
            {"rev-list", "Lists commit objects in reverse chronological order", C::LowLevelInterrogator},
            {"rev-parse", "Pick out and massage parameters", C::LowLevelInterrogator},
            {"show-ref", "List references in a local repository", C::LowLevelInterrogator},
            {"symbolic-ref", "Read, modify and delete symbolic refs", C::LowLevelManipulator},
            {"unpack-objects", "Unpack objects from a packed archive", C::LowLevelManipulator},
            {"update-index", "Register file contents in the working tree to the index", C::LowLevelManipulator},
            {"update-ref", "Update the object name stored in a ref safely", C::LowLevelManipulator},
            {"var", "Show a Git logical variable", C::LowLevelInterrogator},
            {"verify-pack", "Validate packed Git archive files", C::LowLevelInterrogator},
            {"write-tree", "Create a tree object from the current index", C::LowLevelManipulator}
        };
    }
} // namespace gf::terminal::bl
