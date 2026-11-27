#include "core/modules/terminal/bl/script_parameter_analyzer.hpp"

#include "core/shared/utf8_path.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace gf::terminal::bl
{
    namespace
    {
        namespace tbo = gf::terminal::bo;

        constexpr std::size_t kMaxScriptBytes = 256 * 1024;

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

        [[nodiscard]] std::string_view trimSv(std::string_view sv) noexcept
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

        [[nodiscard]] bool isIdentChar(const char c) noexcept
        {
            const unsigned char uc = static_cast<unsigned char>(c);
            return std::isalnum(uc) || c == '_' || c == '-';
        }

        [[nodiscard]] bool isFlagChar(const char c) noexcept
        {
            const unsigned char uc = static_cast<unsigned char>(c);
            return std::isalnum(uc) || c == '-' || c == '_' || c == '?';
        }

        [[nodiscard]] std::string unquoteSimple(std::string_view sv)
        {
            sv = trimSv(sv);
            if (sv.size() >= 2 &&
                ((sv.front() == '"' && sv.back() == '"') ||
                 (sv.front() == '\'' && sv.back() == '\'')))
            {
                return std::string(sv.substr(1, sv.size() - 2));
            }
            return std::string(sv);
        }

        // Extracts all quoted strings from a slice like `("q", "qt")` or `['debug', 'release']`
        [[nodiscard]] std::vector<std::string> extractQuotedStrings(const std::string_view sv)
        {
            std::vector<std::string> out;
            std::size_t i = 0;
            while (i < sv.size())
            {
                if (sv[i] == '"' || sv[i] == '\'')
                {
                    const char q = sv[i++];
                    std::string val;
                    while (i < sv.size())
                    {
                        if (sv[i] == '\\' && i + 1 < sv.size())
                        {
                            val.push_back(sv[i + 1]);
                            i += 2;
                            continue;
                        }
                        if (sv[i] == q)
                        {
                            ++i;
                            break;
                        }
                        val.push_back(sv[i++]);
                    }
                    if (!val.empty())
                    {
                        out.push_back(std::move(val));
                    }
                }
                else
                {
                    ++i;
                }
            }
            return out;
        }

        // Finds matching closing delimiter starting right after opening char (handles quotes)
        [[nodiscard]] std::size_t findMatchingClose(const std::string_view sv,
                                                    const std::size_t openPos,
                                                    const char openCh,
                                                    const char closeCh)
        {
            int depth = 1;
            char quote = '\0';
            for (std::size_t i = openPos + 1; i < sv.size(); ++i)
            {
                const char c = sv[i];
                if (quote != '\0')
                {
                    if (c == '\\' && i + 1 < sv.size())
                    {
                        ++i;
                    }
                    else if (c == quote)
                    {
                        quote = '\0';
                    }
                    continue;
                }
                if (c == '"' || c == '\'')
                {
                    quote = c;
                    continue;
                }
                if (c == openCh) ++depth;
                else if (c == closeCh)
                {
                    --depth;
                    if (depth == 0) return i;
                }
            }
            return std::string_view::npos;
        }

        // Merges a parsed parameter into `params`, avoiding duplicates across short/long aliases.
        void mergeScriptParam(std::vector<tbo::ScriptParamSpec>& params,
                              std::unordered_map<std::string, std::size_t>& indexByLowerFlag,
                              tbo::ScriptParamSpec spec)
        {
            if (spec.name.empty()) return;

            std::optional<std::size_t> existing;
            if (const auto it = indexByLowerFlag.find(toLowerCopy(spec.name)); it != indexByLowerFlag.end())
            {
                existing = it->second;
            }
            if (!existing)
            {
                for (const auto& a : spec.aliases)
                {
                    if (const auto it = indexByLowerFlag.find(toLowerCopy(a)); it != indexByLowerFlag.end())
                    {
                        existing = it->second;
                        break;
                    }
                }
            }

            if (existing)
            {
                auto& dst = params[*existing];
                // Prefer a long flag (`--foo`) as canonical `name` over a short flag (`-f`)
                if (!dst.name.starts_with("--") && spec.name.starts_with("--"))
                {
                    if (std::ranges::find(dst.aliases, dst.name) == dst.aliases.end())
                    {
                        dst.aliases.push_back(dst.name);
                    }
                    dst.name = spec.name;
                    indexByLowerFlag[toLowerCopy(dst.name)] = *existing;
                }
                else if (toLowerCopy(spec.name) != toLowerCopy(dst.name) &&
                         std::ranges::find(dst.aliases, spec.name) == dst.aliases.end())
                {
                    dst.aliases.push_back(spec.name);
                    indexByLowerFlag[toLowerCopy(spec.name)] = *existing;
                }

                for (const auto& a : spec.aliases)
                {
                    if (toLowerCopy(a) != toLowerCopy(dst.name) &&
                        std::ranges::find(dst.aliases, a) == dst.aliases.end())
                    {
                        dst.aliases.push_back(a);
                        indexByLowerFlag[toLowerCopy(a)] = *existing;
                    }
                }
                if (dst.description.empty() && !spec.description.empty())
                {
                    dst.description = spec.description;
                }
                if (dst.valueHint.empty() && !spec.valueHint.empty())
                {
                    dst.valueHint = spec.valueHint;
                }
                if (dst.allowedValues.empty() && !spec.allowedValues.empty())
                {
                    dst.allowedValues = spec.allowedValues;
                }
                dst.takesValue = dst.takesValue || spec.takesValue;
                return;
            }

            const std::size_t idx = params.size();
            indexByLowerFlag[toLowerCopy(spec.name)] = idx;
            for (const auto& a : spec.aliases)
            {
                indexByLowerFlag[toLowerCopy(a)] = idx;
            }
            params.push_back(std::move(spec));
        }

        // Parses documented option lines such as:
        //   "  -q, --qt            Build Desktop Qt Quick 6 preset"
        //   "  -t, --target NAME   Build only the specified target"
        //   "  /clean              Clean build directory"
        void parseHelpStyleOptionLine(std::string_view line,
                                      const bool allowSlashFlags,
                                      std::vector<tbo::ScriptParamSpec>& params,
                                      std::unordered_map<std::string, std::size_t>& indexByFlag)
        {
            // Strip leading `#`, `REM`, `::`, `echo`
            line = trimSv(line);
            if (line.starts_with('#'))
            {
                line = trimSv(line.substr(1));
            }
            else if (line.starts_with("::"))
            {
                line = trimSv(line.substr(2));
            }
            else
            {
                const std::string low = toLowerCopy(line);
                if (low.starts_with("echo ") || low.starts_with("echo\t"))
                {
                    line = trimSv(line.substr(5));
                }
                else if (low.starts_with("rem ") || low.starts_with("rem\t"))
                {
                    line = trimSv(line.substr(4));
                }
            }

            if (line.empty()) return;
            const bool startsFlag = (line.front() == '-') || (allowSlashFlags && line.front() == '/');
            if (!startsFlag) return;
            if (line.starts_with("---") || line.starts_with("===") || line.starts_with("//")) return;

            // Require at least 2 consecutive spaces separating option signature from description
            std::size_t splitPos = std::string_view::npos;
            for (std::size_t i = 1; i + 1 < line.size(); ++i)
            {
                if (line[i] == ' ' && line[i + 1] == ' ')
                {
                    splitPos = i;
                    break;
                }
            }
            if (splitPos == std::string_view::npos) return;

            const std::string_view header = trimSv(line.substr(0, splitPos));
            const std::string_view desc = trimSv(line.substr(splitPos));
            if (desc.empty()) return;

            std::vector<std::string> flags;
            std::string valueHint;

            std::size_t pos = 0;
            while (pos < header.size())
            {
                while (pos < header.size() && (header[pos] == ' ' || header[pos] == ',' || header[pos] == '|'))
                {
                    ++pos;
                }
                if (pos >= header.size()) break;

                if (header[pos] == '-' || (allowSlashFlags && header[pos] == '/'))
                {
                    std::size_t end = pos + 1;
                    while (end < header.size() && isFlagChar(header[end]))
                    {
                        ++end;
                    }
                    const std::string_view f = header.substr(pos, end - pos);
                    if (f.size() >= 2 && f != "--")
                    {
                        flags.emplace_back(f);
                    }
                    pos = end;
                    if (pos < header.size() && header[pos] == '=')
                    {
                        std::size_t hEnd = pos;
                        while (hEnd < header.size() && header[hEnd] != ' ' && header[hEnd] != ',')
                        {
                            ++hEnd;
                        }
                        valueHint = std::string(header.substr(pos + 1, hEnd - (pos + 1)));
                        pos = hEnd;
                    }
                }
                else
                {
                    const std::string_view rest = trimSv(header.substr(pos));
                    if (!rest.empty() && valueHint.empty())
                    {
                        valueHint = std::string(rest);
                    }
                    break;
                }
            }

            if (flags.empty()) return;

            // Choose the longest/long flag as canonical name
            std::string canonical = flags.front();
            for (const auto& f : flags)
            {
                if (f.starts_with("--") && !canonical.starts_with("--"))
                {
                    canonical = f;
                }
            }

            tbo::ScriptParamSpec spec;
            spec.name = canonical;
            for (const auto& f : flags)
            {
                if (f != canonical) spec.aliases.push_back(f);
            }
            if (!valueHint.empty())
            {
                spec.takesValue = true;
                spec.valueHint = valueHint;
            }
            spec.description = std::string(desc);
            mergeScriptParam(params, indexByFlag, std::move(spec));
        }

        struct CachedScriptEntry
        {
            std::filesystem::file_time_type mtime{};
            tbo::ScriptMetadata metadata;
        };

        std::mutex& cacheMutex()
        {
            static std::mutex mu;
            return mu;
        }

        std::unordered_map<std::string, CachedScriptEntry>& scriptCache()
        {
            static std::unordered_map<std::string, CachedScriptEntry> cache;
            return cache;
        }
    } // namespace

    tbo::ScriptKind ScriptParameterAnalyzer::detectScriptKind(const std::filesystem::path& path,
                                                              const std::string_view content)
    {
        const std::string ext = toLowerCopy(gf::core::pathToUtf8(path.extension()));
        if (ext == ".ps1") return tbo::ScriptKind::PowerShell;
        if (ext == ".bat" || ext == ".cmd") return tbo::ScriptKind::Batch;
        if (ext == ".sh" || ext == ".bash" || ext == ".zsh" || ext == ".ksh" || ext == ".fish")
        {
            return tbo::ScriptKind::PosixShell;
        }
        if (ext == ".py" || ext == ".pyw") return tbo::ScriptKind::Python;

        // Inspect shebang `#!` for extensionless scripts (common on Linux/macOS)
        if (content.starts_with("#!"))
        {
            const std::size_t nl = content.find('\n');
            const std::string firstLine = toLowerCopy(
                nl == std::string_view::npos ? content : content.substr(0, nl));
            if (firstLine.find("python") != std::string::npos) return tbo::ScriptKind::Python;
            if (firstLine.find("pwsh") != std::string::npos) return tbo::ScriptKind::PowerShell;
            if (firstLine.find("bash") != std::string::npos ||
                firstLine.find("zsh") != std::string::npos ||
                firstLine.find("/sh") != std::string::npos ||
                firstLine.find(" sh") != std::string::npos)
            {
                return tbo::ScriptKind::PosixShell;
            }
        }
        return tbo::ScriptKind::Unknown;
    }

    tbo::ScriptMetadata ScriptParameterAnalyzer::parsePowerShell(const std::string_view content)
    {
        tbo::ScriptMetadata meta;
        meta.kind = tbo::ScriptKind::PowerShell;

        // 1. Parse comment-based help `<# ... .SYNOPSIS ... .PARAMETER Name ... #>`
        std::unordered_map<std::string, std::string> paramDocs;
        if (const std::size_t helpStart = content.find("<#"); helpStart != std::string_view::npos)
        {
            if (const std::size_t helpEnd = content.find("#>", helpStart + 2); helpEnd != std::string_view::npos)
            {
                const std::string_view block = content.substr(helpStart + 2, helpEnd - (helpStart + 2));
                std::string currentParam;
                bool inSynopsis = false;

                std::size_t pos = 0;
                while (pos < block.size())
                {
                    std::size_t nl = block.find('\n', pos);
                    if (nl == std::string_view::npos) nl = block.size();
                    const std::string_view line = trimSv(block.substr(pos, nl - pos));
                    pos = (nl < block.size()) ? nl + 1 : nl;

                    if (line.empty()) continue;
                    const std::string low = toLowerCopy(line);
                    if (low.starts_with(".parameter"))
                    {
                        inSynopsis = false;
                        currentParam = toLowerCopy(trimSv(line.substr(10)));
                    }
                    else if (low.starts_with(".synopsis"))
                    {
                        currentParam.clear();
                        inSynopsis = true;
                    }
                    else if (line.front() == '.')
                    {
                        currentParam.clear();
                        inSynopsis = false;
                    }
                    else if (!currentParam.empty())
                    {
                        auto& doc = paramDocs[currentParam];
                        if (!doc.empty()) doc.push_back(' ');
                        doc.append(line);
                    }
                    else if (inSynopsis)
                    {
                        if (!meta.summary.empty()) meta.summary.push_back(' ');
                        meta.summary.append(line);
                    }
                }
            }
        }

        // 2. Locate top-level `param(...)` block (ignoring comment blocks)
        std::size_t searchPos = 0;
        if (const std::size_t helpStart = content.find("<#"); helpStart != std::string_view::npos)
        {
            if (const std::size_t helpEnd = content.find("#>", helpStart + 2); helpEnd != std::string_view::npos)
            {
                searchPos = helpEnd + 2;
            }
        }

        const std::string lowerContent = toLowerCopy(content);
        std::size_t paramOpen = std::string_view::npos;
        while (searchPos < lowerContent.size())
        {
            const std::size_t found = lowerContent.find("param", searchPos);
            if (found == std::string::npos) break;
            const bool wordStart = (found == 0 || !isIdentChar(lowerContent[found - 1]));
            if (wordStart)
            {
                std::size_t p = found + 5;
                while (p < content.size() && (content[p] == ' ' || content[p] == '\t' || content[p] == '\r' || content[p] == '\n'))
                {
                    ++p;
                }
                if (p < content.size() && content[p] == '(')
                {
                    paramOpen = p;
                    break;
                }
            }
            searchPos = found + 5;
        }

        if (paramOpen == std::string_view::npos)
        {
            return meta;
        }

        const std::size_t paramClose = findMatchingClose(content, paramOpen, '(', ')');
        if (paramClose == std::string_view::npos)
        {
            return meta;
        }

        const std::string_view paramBody = content.substr(paramOpen + 1, paramClose - (paramOpen + 1));

        // 3. Split param(...) on top-level commas (ignoring commas inside (...) / [...] / quotes / #comments)
        std::vector<std::string_view> decls;
        {
            int parenDepth = 0;
            int bracketDepth = 0;
            char quote = '\0';
            std::size_t start = 0;

            for (std::size_t i = 0; i < paramBody.size(); ++i)
            {
                const char c = paramBody[i];
                if (quote != '\0')
                {
                    if (c == '`' && i + 1 < paramBody.size()) { ++i; continue; }
                    if (c == quote) quote = '\0';
                    continue;
                }
                if (c == '#')
                {
                    const std::size_t nl = paramBody.find('\n', i);
                    if (nl == std::string_view::npos) break;
                    i = nl;
                    continue;
                }
                if (c == '"' || c == '\'') { quote = c; continue; }
                if (c == '(') ++parenDepth;
                else if (c == ')') --parenDepth;
                else if (c == '[') ++bracketDepth;
                else if (c == ']') --bracketDepth;
                else if (c == ',' && parenDepth == 0 && bracketDepth == 0)
                {
                    decls.push_back(paramBody.substr(start, i - start));
                    start = i + 1;
                }
            }
            if (start < paramBody.size())
            {
                decls.push_back(paramBody.substr(start));
            }
        }

        std::unordered_map<std::string, std::size_t> indexByFlag;

        for (const std::string_view rawDecl : decls)
        {
            // Find the `$ParamName` identifier
            std::size_t dollarPos = std::string_view::npos;
            {
                int parenDepth = 0;
                int bracketDepth = 0;
                char quote = '\0';
                for (std::size_t i = 0; i < rawDecl.size(); ++i)
                {
                    const char c = rawDecl[i];
                    if (quote != '\0')
                    {
                        if (c == quote) quote = '\0';
                        continue;
                    }
                    if (c == '#')
                    {
                        const std::size_t nl = rawDecl.find('\n', i);
                        if (nl == std::string_view::npos) break;
                        i = nl;
                        continue;
                    }
                    if (c == '"' || c == '\'') { quote = c; continue; }
                    if (c == '(') ++parenDepth;
                    else if (c == ')') --parenDepth;
                    else if (c == '[') ++bracketDepth;
                    else if (c == ']') --bracketDepth;
                    else if (c == '$' && parenDepth == 0 && bracketDepth == 0)
                    {
                        dollarPos = i;
                        break;
                    }
                }
            }
            if (dollarPos == std::string_view::npos) continue;

            std::size_t nameEnd = dollarPos + 1;
            while (nameEnd < rawDecl.size() && (std::isalnum(static_cast<unsigned char>(rawDecl[nameEnd])) || rawDecl[nameEnd] == '_'))
            {
                ++nameEnd;
            }
            const std::string_view varName = rawDecl.substr(dollarPos + 1, nameEnd - (dollarPos + 1));
            if (varName.empty()) continue;

            tbo::ScriptParamSpec spec;
            spec.name = "-" + std::string(varName);
            spec.takesValue = true;

            // Inspect attributes & type annotations before `$varName`
            const std::string_view attrs = rawDecl.substr(0, dollarPos);
            const std::string attrsLower = toLowerCopy(attrs);

            if (attrsLower.find("[switch]") != std::string::npos ||
                attrsLower.find("[switchparameter]") != std::string::npos)
            {
                spec.takesValue = false;
            }
            else if (attrsLower.find("[string]") != std::string::npos)
            {
                spec.valueHint = "<string>";
            }
            else if (attrsLower.find("[int]") != std::string::npos ||
                     attrsLower.find("[int32]") != std::string::npos)
            {
                spec.valueHint = "<int>";
            }
            else if (attrsLower.find("[bool]") != std::string::npos)
            {
                spec.valueHint = "<bool>";
            }
            else if (attrsLower.find("[string[]]") != std::string::npos)
            {
                spec.valueHint = "<string[]>";
            }

            // Extract [Alias("a", "b")]
            if (const std::size_t aliasPos = attrsLower.find("alias("); aliasPos != std::string::npos)
            {
                if (const std::size_t closeP = findMatchingClose(attrs, aliasPos + 5, '(', ')');
                    closeP != std::string_view::npos)
                {
                    for (const auto& a : extractQuotedStrings(attrs.substr(aliasPos + 6, closeP - (aliasPos + 6))))
                    {
                        spec.aliases.push_back(a.starts_with('-') ? a : ("-" + a));
                    }
                }
            }

            // Extract [ValidateSet("Debug", "Release")]
            if (const std::size_t vsPos = attrsLower.find("validateset("); vsPos != std::string::npos)
            {
                if (const std::size_t closeP = findMatchingClose(attrs, vsPos + 11, '(', ')');
                    closeP != std::string_view::npos)
                {
                    spec.allowedValues = extractQuotedStrings(attrs.substr(vsPos + 12, closeP - (vsPos + 12)));
                }
            }

            // Look up description from `.PARAMETER <Name>`
            if (const auto it = paramDocs.find(toLowerCopy(varName)); it != paramDocs.end())
            {
                spec.description = it->second;
            }

            // Fallback: inline `# comment` or default value (`= ...`) after `$varName`
            if (spec.description.empty() && nameEnd < rawDecl.size())
            {
                const std::string_view tail = rawDecl.substr(nameEnd);
                if (const std::size_t hashPos = tail.find('#'); hashPos != std::string_view::npos)
                {
                    const std::size_t nl = tail.find('\n', hashPos);
                    spec.description = std::string(trimSv(
                        tail.substr(hashPos + 1, nl == std::string_view::npos ? std::string_view::npos : nl - hashPos - 1)));
                }
                else if (const std::size_t eqPos = tail.find('='); eqPos != std::string_view::npos)
                {
                    std::string_view defVal = trimSv(tail.substr(eqPos + 1));
                    if (const std::size_t nl = defVal.find('\n'); nl != std::string_view::npos)
                    {
                        defVal = trimSv(defVal.substr(0, nl));
                    }
                    if (!defVal.empty() && defVal.size() <= 48)
                    {
                        spec.description = "Default: " + unquoteSimple(defVal);
                    }
                }
            }

            mergeScriptParam(meta.params, indexByFlag, std::move(spec));
        }

        return meta;
    }

    tbo::ScriptMetadata ScriptParameterAnalyzer::parsePosixShell(const std::string_view content)
    {
        tbo::ScriptMetadata meta;
        meta.kind = tbo::ScriptKind::PosixShell;

        std::unordered_map<std::string, std::size_t> indexByFlag;

        // Pass 1: Extract documented options & descriptions from comments and usage/help heredocs
        {
            std::size_t pos = 0;
            while (pos < content.size())
            {
                std::size_t nl = content.find('\n', pos);
                if (nl == std::string_view::npos) nl = content.size();
                const std::string_view line = content.substr(pos, nl - pos);
                pos = (nl < content.size()) ? nl + 1 : nl;
                parseHelpStyleOptionLine(line, false, meta.params, indexByFlag);
            }
        }

        // Pass 2: Extract flags from `case` arms (`-q|--qt)`, `--clean)`, `-t|--target)`)
        {
            std::size_t pos = 0;
            while (pos < content.size())
            {
                std::size_t nl = content.find('\n', pos);
                if (nl == std::string_view::npos) nl = content.size();
                const std::string_view line = trimSv(content.substr(pos, nl - pos));
                const std::size_t nextPos = (nl < content.size()) ? nl + 1 : nl;
                pos = nextPos;

                if (line.empty() || line.front() != '-' || line.starts_with("---")) continue;
                const std::size_t closeParen = line.find(')');
                if (closeParen == std::string_view::npos || closeParen > 64) continue;

                const std::string_view pattern = trimSv(line.substr(0, closeParen));
                std::vector<std::string> flags;
                std::size_t p = 0;
                bool valid = true;
                while (p < pattern.size())
                {
                    const std::size_t bar = pattern.find('|', p);
                    const std::string_view tok = trimSv(
                        bar == std::string_view::npos ? pattern.substr(p) : pattern.substr(p, bar - p));
                    if (tok.size() >= 2 && tok.front() == '-' && tok != "--")
                    {
                        bool allOk = true;
                        for (std::size_t k = 1; k < tok.size(); ++k)
                        {
                            if (!isFlagChar(tok[k]) && tok[k] != '*') { allOk = false; break; }
                        }
                        if (allOk && tok.find('*') == std::string_view::npos)
                        {
                            flags.emplace_back(tok);
                        }
                        else
                        {
                            valid = false;
                        }
                    }
                    else
                    {
                        valid = false;
                    }
                    if (bar == std::string_view::npos) break;
                    p = bar + 1;
                }

                if (!valid || flags.empty()) continue;

                // Peek next few lines to detect `shift 2` or `$2` (takesValue)
                const std::string_view armPreview = content.substr(
                    nextPos, std::min<std::size_t>(200, content.size() - nextPos));
                const std::size_t armEnd = armPreview.find(";;");
                const std::string_view armBody = (armEnd == std::string_view::npos) ? armPreview : armPreview.substr(0, armEnd);
                const bool takesVal = (armBody.find("shift 2") != std::string_view::npos) ||
                                      (armBody.find("$2") != std::string_view::npos) ||
                                      (armBody.find("${2") != std::string_view::npos);

                std::string canonical = flags.front();
                for (const auto& f : flags)
                {
                    if (f.starts_with("--") && !canonical.starts_with("--")) canonical = f;
                }

                tbo::ScriptParamSpec spec;
                spec.name = canonical;
                for (const auto& f : flags)
                {
                    if (f != canonical) spec.aliases.push_back(f);
                }
                spec.takesValue = takesVal;
                mergeScriptParam(meta.params, indexByFlag, std::move(spec));
            }
        }

        // Pass 3: Extract short flags from `getopts "abc:d:"`
        {
            std::size_t pos = 0;
            while (pos < content.size())
            {
                const std::size_t g = content.find("getopts", pos);
                if (g == std::string_view::npos) break;
                pos = g + 7;
                if (g > 0 && isIdentChar(content[g - 1])) continue;
                const std::size_t nl = content.find('\n', pos);
                const std::string_view rest = content.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
                const auto quoted = extractQuotedStrings(rest);
                if (!quoted.empty())
                {
                    const std::string& optSpec = quoted.front();
                    for (std::size_t i = 0; i < optSpec.size(); ++i)
                    {
                        const char ch = optSpec[i];
                        if (ch == ':') continue;
                        if (std::isalnum(static_cast<unsigned char>(ch)))
                        {
                            tbo::ScriptParamSpec spec;
                            spec.name = std::string("-") + ch;
                            spec.takesValue = (i + 1 < optSpec.size() && optSpec[i + 1] == ':');
                            mergeScriptParam(meta.params, indexByFlag, std::move(spec));
                        }
                    }
                }
            }
        }

        return meta;
    }

    tbo::ScriptMetadata ScriptParameterAnalyzer::parseBatch(const std::string_view content)
    {
        tbo::ScriptMetadata meta;
        meta.kind = tbo::ScriptKind::Batch;

        std::unordered_map<std::string, std::size_t> indexByFlag;

        std::size_t pos = 0;
        while (pos < content.size())
        {
            std::size_t nl = content.find('\n', pos);
            if (nl == std::string_view::npos) nl = content.size();
            const std::string_view rawLine = content.substr(pos, nl - pos);
            pos = (nl < content.size()) ? nl + 1 : nl;

            // 1. Documented usage lines (`echo   /clean   Clean build`, `REM   --release   ...`)
            parseHelpStyleOptionLine(rawLine, true, meta.params, indexByFlag);

            // 2. Conditional argument checks (`if /i "%~1"=="--clean"`, `if "%1"=="/h"`)
            const std::string low = toLowerCopy(trimSv(rawLine));
            if (low.starts_with("if ") || low.starts_with("if\t"))
            {
                if (const std::size_t eq = rawLine.find("=="); eq != std::string_view::npos)
                {
                    std::string_view rhs = trimSv(rawLine.substr(eq + 2));
                    std::size_t end = 0;
                    if (!rhs.empty() && rhs.front() == '"')
                    {
                        const std::size_t q2 = rhs.find('"', 1);
                        if (q2 != std::string_view::npos)
                        {
                            rhs = rhs.substr(1, q2 - 1);
                            end = rhs.size();
                        }
                    }
                    else
                    {
                        while (end < rhs.size() && rhs[end] != ' ' && rhs[end] != '\t' && rhs[end] != '(')
                        {
                            ++end;
                        }
                        rhs = rhs.substr(0, end);
                    }

                    if (rhs.size() >= 2 && (rhs.front() == '-' || rhs.front() == '/') && rhs != "--" && rhs != "//")
                    {
                        bool ok = true;
                        for (std::size_t k = 1; k < rhs.size(); ++k)
                        {
                            if (!isFlagChar(rhs[k])) { ok = false; break; }
                        }
                        if (ok)
                        {
                            tbo::ScriptParamSpec spec;
                            spec.name = std::string(rhs);
                            mergeScriptParam(meta.params, indexByFlag, std::move(spec));
                        }
                    }
                }
            }
        }

        return meta;
    }

    tbo::ScriptMetadata ScriptParameterAnalyzer::parsePython(const std::string_view content)
    {
        tbo::ScriptMetadata meta;
        meta.kind = tbo::ScriptKind::Python;

        std::unordered_map<std::string, std::size_t> indexByFlag;

        auto parseArgCall = [&](const std::string_view callBody)
        {
            // Collect leading string literals (the flag names: "-f", "--file") before first keyword arg `=`
            std::vector<std::string> flags;
            std::size_t i = 0;
            while (i < callBody.size())
            {
                while (i < callBody.size() && (std::isspace(static_cast<unsigned char>(callBody[i])) || callBody[i] == ','))
                {
                    ++i;
                }
                if (i >= callBody.size()) break;
                if (callBody[i] == '"' || callBody[i] == '\'')
                {
                    const char q = callBody[i++];
                    std::string s;
                    while (i < callBody.size())
                    {
                        if (callBody[i] == '\\' && i + 1 < callBody.size()) { s.push_back(callBody[i + 1]); i += 2; continue; }
                        if (callBody[i] == q) { ++i; break; }
                        s.push_back(callBody[i++]);
                    }
                    if (s.size() >= 2 && s.front() == '-')
                    {
                        flags.push_back(std::move(s));
                    }
                }
                else
                {
                    break; // reached keyword arguments like help=..., action=...
                }
            }

            if (flags.empty()) return;

            std::string canonical = flags.front();
            for (const auto& f : flags)
            {
                if (f.starts_with("--") && !canonical.starts_with("--")) canonical = f;
            }

            tbo::ScriptParamSpec spec;
            spec.name = canonical;
            for (const auto& f : flags)
            {
                if (f != canonical) spec.aliases.push_back(f);
            }
            spec.takesValue = true;

            // Detect action="store_true" / "store_false" / "count" / is_flag=True
            if (callBody.find("store_true") != std::string_view::npos ||
                callBody.find("store_false") != std::string_view::npos ||
                callBody.find("action='count'") != std::string_view::npos ||
                callBody.find("action=\"count\"") != std::string_view::npos ||
                callBody.find("is_flag=True") != std::string_view::npos)
            {
                spec.takesValue = false;
            }

            // Extract help="..." or help='...'
            auto extractKwString = [&](const std::string_view kw) -> std::string
            {
                std::size_t p = 0;
                while (p < callBody.size())
                {
                    const std::size_t found = callBody.find(kw, p);
                    if (found == std::string_view::npos) break;
                    const bool okStart = (found == 0 || !isIdentChar(callBody[found - 1]));
                    p = found + kw.size();
                    if (!okStart) continue;
                    while (p < callBody.size() && std::isspace(static_cast<unsigned char>(callBody[p]))) ++p;
                    if (p < callBody.size() && callBody[p] == '=')
                    {
                        ++p;
                        while (p < callBody.size() && std::isspace(static_cast<unsigned char>(callBody[p]))) ++p;
                        if (p < callBody.size() && (callBody[p] == 'f' || callBody[p] == 'r') &&
                            p + 1 < callBody.size() && (callBody[p + 1] == '"' || callBody[p + 1] == '\''))
                        {
                            ++p;
                        }
                        if (p < callBody.size() && (callBody[p] == '"' || callBody[p] == '\''))
                        {
                            const char q = callBody[p++];
                            std::string val;
                            while (p < callBody.size())
                            {
                                if (callBody[p] == '\\' && p + 1 < callBody.size()) { val.push_back(callBody[p + 1]); p += 2; continue; }
                                if (callBody[p] == q) break;
                                val.push_back(callBody[p++]);
                            }
                            return val;
                        }
                    }
                }
                return {};
            };

            spec.description = extractKwString("help");
            if (const std::string mv = extractKwString("metavar"); !mv.empty())
            {
                spec.valueHint = "<" + mv + ">";
            }

            // Extract choices=[...]
            if (const std::size_t chPos = callBody.find("choices"); chPos != std::string_view::npos)
            {
                if (const std::size_t br = callBody.find('[', chPos); br != std::string_view::npos)
                {
                    if (const std::size_t brEnd = findMatchingClose(callBody, br, '[', ']'); brEnd != std::string_view::npos)
                    {
                        spec.allowedValues = extractQuotedStrings(callBody.substr(br + 1, brEnd - (br + 1)));
                    }
                }
            }

            mergeScriptParam(meta.params, indexByFlag, std::move(spec));
        };

        for (const std::string_view marker : {std::string_view("add_argument("), std::string_view("click.option(")})
        {
            std::size_t pos = 0;
            while (pos < content.size())
            {
                const std::size_t found = content.find(marker, pos);
                if (found == std::string_view::npos) break;
                const std::size_t openParen = found + marker.size() - 1;
                const std::size_t closeParen = findMatchingClose(content, openParen, '(', ')');
                if (closeParen == std::string_view::npos) break;
                parseArgCall(content.substr(openParen + 1, closeParen - (openParen + 1)));
                pos = closeParen + 1;
            }
        }

        return meta;
    }

    tbo::ScriptMetadata ScriptParameterAnalyzer::analyzeFileCached(const std::filesystem::path& path)
    {
        if (path.empty()) return {};

        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec))
        {
            return {};
        }

        const auto mtime = std::filesystem::last_write_time(path, ec);
        if (ec) return {};

        const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
        const std::string cacheKey = gf::core::pathToUtf8(ec ? path : canonical);

        {
            std::scoped_lock lock(cacheMutex());
            if (const auto it = scriptCache().find(cacheKey); it != scriptCache().end() && it->second.mtime == mtime)
            {
                return it->second.metadata;
            }
        }

        // Fast check by extension before opening huge binary files
        tbo::ScriptKind kind = detectScriptKind(path);
        const auto fileSize = std::filesystem::file_size(path, ec);
        if (ec || fileSize == 0 || (kind == tbo::ScriptKind::Unknown && fileSize > kMaxScriptBytes))
        {
            return {};
        }

        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return {};

        const std::size_t toRead = static_cast<std::size_t>(std::min<std::uintmax_t>(fileSize, kMaxScriptBytes));
        std::string content(toRead, '\0');
        in.read(content.data(), static_cast<std::streamsize>(toRead));
        content.resize(static_cast<std::size_t>(in.gcount()));

        if (kind == tbo::ScriptKind::Unknown)
        {
            kind = detectScriptKind(path, content);
        }

        tbo::ScriptMetadata meta;
        switch (kind)
        {
        case tbo::ScriptKind::PowerShell:
            meta = parsePowerShell(content);
            break;
        case tbo::ScriptKind::PosixShell:
            meta = parsePosixShell(content);
            break;
        case tbo::ScriptKind::Batch:
            meta = parseBatch(content);
            break;
        case tbo::ScriptKind::Python:
            meta = parsePython(content);
            break;
        case tbo::ScriptKind::Unknown:
        default:
            break;
        }

        {
            std::scoped_lock lock(cacheMutex());
            scriptCache()[cacheKey] = CachedScriptEntry{.mtime = mtime, .metadata = meta};
        }

        return meta;
    }
} // namespace gf::terminal::bl
