#pragma once

#include "core/modules/terminal/bo/terminal_models.hpp"

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace gf::terminal::ports
{
    class ITerminalProcessHandle
    {
    public:
        virtual ~ITerminalProcessHandle() = default;

        /// Writes UTF-8 bytes to the running process stdin. Returns false if closed/failed.
        virtual bool writeStdin(std::string_view utf8Data) = 0;

        /// Requests cancellation/termination of the running process and its child tree.
        virtual void terminate() = 0;

        /// Returns true if the process has not yet finished.
        [[nodiscard]] virtual bool isRunning() const noexcept = 0;

        /// Blocks until the process finishes and all output callbacks have completed.
        virtual void wait() = 0;
    };

    class ITerminalProcessRunner
    {
    public:
        virtual ~ITerminalProcessRunner() = default;

        /// Detects available shells on the current OS (e.g. PowerShell, CMD, Git Bash on Windows).
        [[nodiscard]] virtual std::vector<bo::TerminalShellInfo> availableShells() const = 0;

        /// Starts a command asynchronously, streaming output chunks and invoking onFinished on exit.
        [[nodiscard]] virtual std::expected<std::shared_ptr<ITerminalProcessHandle>, std::string>
        start(const bo::TerminalExecutionRequest& request, bo::TerminalExecutionCallbacks callbacks) = 0;
    };
} // namespace gf::terminal::ports
