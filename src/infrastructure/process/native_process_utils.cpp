#include "infrastructure/process/native_process_utils.hpp"

#include <algorithm>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cwctype>
#else
extern char** environ;
#endif

namespace gf::infra::process
{
#ifdef _WIN32
    std::wstring utf8ToWide(const std::string_view utf8)
    {
        if (utf8.empty())
        {
            return {};
        }
        const int needed = MultiByteToWideChar(CP_UTF8, 0,
                                               utf8.data(), static_cast<int>(utf8.size()),
                                               nullptr, 0);
        if (needed <= 0)
        {
            return {};
        }
        std::wstring result(static_cast<std::size_t>(needed), L'\0');
        MultiByteToWideChar(CP_UTF8, 0,
                            utf8.data(), static_cast<int>(utf8.size()),
                            result.data(), needed);
        return result;
    }

    void appendQuotedWindowsArg(std::wstring& cmdLine, const std::wstring_view arg)
    {
        const bool needsQuotes = arg.empty() ||
            arg.find_first_of(L" \t\n\v\"") != std::wstring_view::npos;
        if (!needsQuotes)
        {
            cmdLine.append(arg);
            return;
        }

        cmdLine.push_back(L'"');
        for (std::size_t i = 0; i < arg.size();)
        {
            std::size_t backslashes = 0;
            while (i < arg.size() && arg[i] == L'\\')
            {
                ++backslashes;
                ++i;
            }

            if (i == arg.size())
            {
                cmdLine.append(backslashes * 2, L'\\');
                break;
            }
            if (arg[i] == L'"')
            {
                cmdLine.append(backslashes * 2 + 1, L'\\');
                cmdLine.push_back(L'"');
                ++i;
            }
            else
            {
                cmdLine.append(backslashes, L'\\');
                cmdLine.push_back(arg[i]);
                ++i;
            }
        }
        cmdLine.push_back(L'"');
    }

    std::wstring buildWindowsCommandLine(const std::filesystem::path& executable,
                                         const std::vector<std::string>& utf8Args)
    {
        std::wstring cmdLine;
        appendQuotedWindowsArg(cmdLine, executable.wstring());
        for (const auto& arg : utf8Args)
        {
            cmdLine.push_back(L' ');
            appendQuotedWindowsArg(cmdLine, utf8ToWide(arg));
        }
        return cmdLine;
    }

    std::wstring buildWindowsEnvironmentBlock(
        const std::vector<std::pair<std::string, std::string>>& overrides,
        const std::vector<std::wstring_view>& stripKeys)
    {
        auto toUpperW = [](const std::wstring_view s)
        {
            std::wstring out;
            out.reserve(s.size());
            for (const wchar_t c : s)
            {
                out.push_back(static_cast<wchar_t>(std::towupper(c)));
            }
            return out;
        };

        std::vector<std::pair<std::wstring, std::wstring>> entries;
        std::unordered_map<std::wstring, std::size_t> keyIndex;

        auto shouldStrip = [&](const std::wstring_view upperKey)
        {
            return std::ranges::any_of(stripKeys, [&](const std::wstring_view sk)
            {
                return toUpperW(sk) == upperKey;
            });
        };

        if (LPWCH envStrings = GetEnvironmentStringsW(); envStrings != nullptr)
        {
            const wchar_t* ptr = envStrings;
            while (*ptr != L'\0')
            {
                const std::wstring_view entry(ptr);
                ptr += entry.size() + 1;
                const std::size_t eq = entry.find(L'=', 1);
                if (eq == std::wstring_view::npos) continue;
                std::wstring key(entry.substr(0, eq));
                std::wstring val(entry.substr(eq + 1));
                const std::wstring upper = toUpperW(key);
                if (shouldStrip(upper)) continue;
                keyIndex[upper] = entries.size();
                entries.emplace_back(std::move(key), std::move(val));
            }
            FreeEnvironmentStringsW(envStrings);
        }

        for (const auto& [k, v] : overrides)
        {
            std::wstring wk = utf8ToWide(k);
            std::wstring wv = utf8ToWide(v);
            const std::wstring upper = toUpperW(wk);
            if (const auto it = keyIndex.find(upper); it != keyIndex.end())
            {
                entries[it->second].second = std::move(wv);
            }
            else
            {
                keyIndex[upper] = entries.size();
                entries.emplace_back(std::move(wk), std::move(wv));
            }
        }

        std::ranges::sort(entries, [&](const auto& a, const auto& b)
        {
            return toUpperW(a.first) < toUpperW(b.first);
        });

        std::wstring block;
        for (const auto& [k, v] : entries)
        {
            block.append(k);
            block.push_back(L'=');
            block.append(v);
            block.push_back(L'\0');
        }
        block.push_back(L'\0');
        return block;
    }
#else
    std::vector<std::string> buildPosixEnvironment(
        const std::vector<std::pair<std::string, std::string>>& overrides,
        const std::vector<std::string_view>& stripKeys)
    {
        std::unordered_map<std::string, std::string> envMap;
        if (environ != nullptr)
        {
            for (char** p = environ; *p != nullptr; ++p)
            {
                const std::string_view entry(*p);
                if (const std::size_t eq = entry.find('='); eq != std::string_view::npos)
                {
                    const std::string_view key = entry.substr(0, eq);
                    if (std::ranges::find(stripKeys, key) != stripKeys.end())
                    {
                        continue;
                    }
                    envMap[std::string(key)] = std::string(entry.substr(eq + 1));
                }
            }
        }
        for (const auto& [k, v] : overrides)
        {
            envMap[k] = v;
        }

        std::vector<std::string> result;
        result.reserve(envMap.size());
        for (const auto& [k, v] : envMap)
        {
            result.push_back(k + "=" + v);
        }
        return result;
    }
#endif
} // namespace gf::infra::process
