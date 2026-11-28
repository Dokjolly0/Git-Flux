#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gf::infra::process
{
#ifdef _WIN32
    [[nodiscard]] std::wstring utf8ToWide(std::string_view utf8);

    /// Applies MSVCRT / CommandLineToArgvW quoting rules directly without passing through cmd.exe.
    void appendQuotedWindowsArg(std::wstring& cmdLine, std::wstring_view arg);

    [[nodiscard]] std::wstring buildWindowsCommandLine(const std::filesystem::path& executable,
                                                       const std::vector<std::string>& utf8Args);

    [[nodiscard]] std::wstring buildWindowsEnvironmentBlock(
        const std::vector<std::pair<std::string, std::string>>& overrides,
        const std::vector<std::wstring_view>& stripKeys = {});
#else
    [[nodiscard]] std::vector<std::string> buildPosixEnvironment(
        const std::vector<std::pair<std::string, std::string>>& overrides,
        const std::vector<std::string_view>& stripKeys = {});
#endif
} // namespace gf::infra::process
