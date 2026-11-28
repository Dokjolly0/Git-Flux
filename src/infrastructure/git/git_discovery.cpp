#include "infrastructure/git/git_discovery.hpp"
#include "core/shared/string_utils.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdlib>
#include <sstream>
#include <unistd.h>
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace gf::infra::git
{
    std::expected<gf::git::bo::GitExecutableInfo, gf::git::bo::GitExecutionError>
    GitDiscovery::discover()
    {
        namespace fs = std::filesystem;
        using gf::git::bo::GitErrorKind;
        using gf::git::bo::GitExecutableInfo;
        using gf::git::bo::GitExecutionError;

        GitExecutableInfo info;

#ifdef _WIN32
        constexpr DWORD kBufferSize = MAX_PATH * 4;
        wchar_t buffer[kBufferSize]{};
        wchar_t* filePart = nullptr;
        const DWORD len = SearchPathW(nullptr, L"git.exe", nullptr,
                                      kBufferSize, buffer, &filePart);
        if (len == 0 || len >= kBufferSize)
        {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::ExecutableNotFound,
                .message = "Impossibile trovare git.exe nel PATH di sistema"
            });
        }

        info.discoveredInPath = fs::path(std::wstring_view(buffer, len)).lexically_normal();
        info.directBinaryPath = info.discoveredInPath;

        // Check if path points to "...\Git\cmd\git.exe" (Git for Windows wrapper shim)
        if (const fs::path parentDir = info.discoveredInPath.parent_path();
            gf::core::toLowerAscii(parentDir.filename().string()) == "cmd")
        {
            const fs::path gitRoot = parentDir.parent_path();
            const fs::path candidates[] = {
                gitRoot / "ucrt64" / "bin" / "git.exe",
                gitRoot / "mingw64" / "bin" / "git.exe",
                gitRoot / "clangarm64" / "bin" / "git.exe",
                gitRoot / "mingw32" / "bin" / "git.exe"
            };
            for (const auto& candidate : candidates)
            {
                if (std::error_code ec; fs::exists(candidate, ec) && !ec)
                {
                    info.directBinaryPath = candidate.lexically_normal();
                    info.isWindowsCmdShim = true;
                    break;
                }
            }
        }
#else
        const char* pathEnv = std::getenv("PATH");
        if (!pathEnv)
        {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::ExecutableNotFound,
                .message = "Variabile PATH non definita"
            });
        }
        std::stringstream ss(pathEnv);
        std::string dir;
        while (std::getline(ss, dir, ':'))
        {
            if (dir.empty())
            {
                continue;
            }
            fs::path candidate = fs::path(dir) / "git";
            if (::access(candidate.c_str(), X_OK) == 0)
            {
                info.discoveredInPath = candidate;
                info.directBinaryPath = candidate;
                break;
            }
        }
        if (info.discoveredInPath.empty())
        {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::ExecutableNotFound,
                .message = "Impossibile trovare l'eseguibile git nel PATH"
            });
        }
#endif

        return info;
    }
} // namespace gf::infra::git
