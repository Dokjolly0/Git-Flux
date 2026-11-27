#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace gf::terminal::bo
{
    enum class ScriptKind
    {
        Unknown,
        PowerShell,
        Batch,
        PosixShell,
        Python
    };

    enum class TargetOs
    {
        Windows,
        Linux,
        MacOs
    };

    [[nodiscard]] constexpr TargetOs currentTargetOs() noexcept
    {
#ifdef _WIN32
        return TargetOs::Windows;
#elif defined(__APPLE__)
        return TargetOs::MacOs;
#else
        return TargetOs::Linux;
#endif
    }

    [[nodiscard]] inline std::string_view scriptKindBadge(const ScriptKind kind) noexcept
    {
        switch (kind)
        {
        case ScriptKind::PowerShell: return "ps1";
        case ScriptKind::Batch: return "bat";
        case ScriptKind::PosixShell: return "sh";
        case ScriptKind::Python: return "py";
        case ScriptKind::Unknown:
        default: return "script";
        }
    }

    [[nodiscard]] inline bool isScriptNativeToOs(const ScriptKind kind, const TargetOs os) noexcept
    {
        switch (os)
        {
        case TargetOs::Windows:
            return kind == ScriptKind::PowerShell || kind == ScriptKind::Batch || kind == ScriptKind::Python;
        case TargetOs::Linux:
        case TargetOs::MacOs:
            return kind == ScriptKind::PosixShell || kind == ScriptKind::Python;
        }
        return false;
    }

    struct ScriptParamSpec
    {
        std::string name;                     // Canonical flag, e.g. "-Target", "--target", "/clean"
        std::vector<std::string> aliases;     // Short/alias flags, e.g. {"-t"}
        std::string valueHint;                // e.g. "<string>", "<NAME>", "<int>"
        std::string description;              // Extracted from .PARAMETER / help / argparse
        std::vector<std::string> allowedValues; // From [ValidateSet(...)] or argparse choices=[...]
        bool takesValue = false;
    };

    struct ScriptMetadata
    {
        ScriptKind kind = ScriptKind::Unknown;
        std::string summary;
        std::vector<ScriptParamSpec> params;
    };
} // namespace gf::terminal::bo
