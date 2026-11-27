#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gf::terminal::bo
{
    enum class TerminalShellKind
    {
        Auto,
        PowerShell,
        Cmd,
        GitBash,
        PosixShell
    };

    struct TerminalShellInfo
    {
        TerminalShellKind kind = TerminalShellKind::Auto;
        std::string id;          // e.g. "powershell", "cmd", "git-bash", "bash", "zsh"
        std::string displayName; // e.g. "PowerShell", "Command Prompt", "Git Bash", "zsh"
        std::filesystem::path executablePath;
    };

    enum class TerminalOutputStream
    {
        Stdout,
        Stderr,
        System
    };

    struct TerminalExecutionRequest
    {
        std::string commandLine;
        std::filesystem::path workingDirectory;
        TerminalShellKind shellKind = TerminalShellKind::Auto;
        bool forceGitColor = true;
        std::vector<std::pair<std::string, std::string>> extraEnvironment{};
    };

    struct TerminalCommandResult
    {
        int exitCode = -1;
        std::chrono::milliseconds duration{0};
        bool wasCancelled = false;
        std::string errorMessage;
    };

    struct TerminalExecutionCallbacks
    {
        std::function<void(TerminalOutputStream, std::string_view)> onOutput;
        std::function<void(const TerminalCommandResult&)> onFinished;
    };

    struct TerminalTextStyle
    {
        std::string fgColor; // Hex string "#RRGGBB" or empty for default
        std::string bgColor; // Hex string "#RRGGBB" or empty for default
        bool bold = false;
        bool dim = false;
        bool italic = false;
        bool underline = false;

        [[nodiscard]] bool operator==(const TerminalTextStyle&) const noexcept = default;
    };

    struct TerminalStyledSpan
    {
        std::string text;
        TerminalTextStyle style{};
    };

    struct TerminalRenderedLine
    {
        std::vector<TerminalStyledSpan> spans;
        std::string plainText;
        std::string richHtml;
    };

    [[nodiscard]] inline std::string_view shellKindToId(const TerminalShellKind kind) noexcept
    {
        switch (kind)
        {
        case TerminalShellKind::PowerShell: return "powershell";
        case TerminalShellKind::Cmd: return "cmd";
        case TerminalShellKind::GitBash: return "git-bash";
        case TerminalShellKind::PosixShell: return "posix";
        case TerminalShellKind::Auto:
        default: return "auto";
        }
    }

    [[nodiscard]] inline TerminalShellKind shellKindFromId(const std::string_view id) noexcept
    {
        if (id == "powershell" || id == "pwsh") return TerminalShellKind::PowerShell;
        if (id == "cmd") return TerminalShellKind::Cmd;
        if (id == "git-bash" || id == "gitbash" || id == "bash-win") return TerminalShellKind::GitBash;
        if (id == "posix" || id == "bash" || id == "zsh" || id == "sh") return TerminalShellKind::PosixShell;
        return TerminalShellKind::Auto;
    }
} // namespace gf::terminal::bo
